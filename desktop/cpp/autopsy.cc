
#include "autopsy.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <queue>

using namespace std;

#define MAX_POINTS 1500

struct __attribute__((packed)) Header {
  uint8_t version_major = 0;
  uint8_t version_minor = 1;
  uint8_t compression_type = 0;
  uint16_t segment_start;
  uint32_t index_size;
};

struct Trace {
  std::string trace;
  bool filtered = false;
  uint64_t max_aggregate = 0;
  vector<Chunk*> chunks;
  vector<TimeValue> aggregate;
};
    

bool operator<(const TimeValue& a, const TimeValue& b) {
  return a.time > b.time;
}

class Dataset {
  public:
    Dataset() {}
    void Reset(string& dir_path) {
      if (chunk_ptr_)
        delete [] chunk_ptr_;
      traces_.clear();
      
      string trace_file = dir_path + "hplgst.trace";
      // fopen trace file, build traces array
      FILE* trace_fd = fopen(trace_file.c_str(), "r");
      if (trace_fd == NULL) {
        cout << "failed to open file\n";
        return;
      }
      Header header;
      fread(&header, sizeof(Header), 1, trace_fd);

      vector<uint16_t> index;
      index.resize(header.index_size);
      fread(&index[0], 2, header.index_size, trace_fd);

      traces_.reserve(header.index_size);
      Trace t;
      char trace_buf[4096];
      for (int i = 0; i < header.index_size; i++) {
        if (index[i] > 4096) {
          cout << "index is too big!! " << index[i] << endl;
          return;
        } else {
          fread(trace_buf, index[i], 1, trace_fd);
          t.trace = string(trace_buf, index[i]);
          traces_.push_back(t);
        }
      }
      fclose(trace_fd);

      string chunk_file = dir_path + "hplgst.chunks";
      // for some reason I can't mmap the file so we open and copy ...
      FILE* chunk_fd = fopen(chunk_file.c_str(), "r");
      // file size produced by sanitizer is buggy and adds a bunch 0 data to 
      // end of file. not sure why yet ...
      //int file_size = 0;
      if (chunk_fd == NULL) {
        cout << "Failed to open chunk file" << endl;
        return;
      } /*else {
        fseek(chunk_fd, 0L, SEEK_END);
        file_size = ftell(chunk_fd);
        rewind(chunk_fd);
      }*/
     
      fread(&header, sizeof(Header), 1, chunk_fd);
      
      index.resize(header.index_size);
      fread(&index[0], 2, header.index_size, chunk_fd);

      num_chunks_ = header.index_size;
      //file_size = file_size - sizeof(Header) - header.index_size*2;
      //cout << " file size now " << file_size << endl;
      chunk_ptr_ = new char[num_chunks_*sizeof(Chunk)];
      fread(chunk_ptr_, num_chunks_*sizeof(Chunk), 1, chunk_fd);

      // we can do this because all the fields are the same size (structs)
      chunks_ = (Chunk*) (chunk_ptr_);
      fclose(chunk_fd);

      // sort the chunks makes bin/aggregate easier
      sort(chunks_, chunks_+num_chunks_, [](Chunk& a, Chunk& b) {
            return a.timestamp_start < b.timestamp_start;
          });

      // set trace structure pointers to their chunks
      min_time_ = 0;
      for (int i = 0; i < num_chunks_; i++) {
        Trace& t = traces_[chunks_[i].stack_index];
        t.chunks.push_back(&chunks_[i]);
        if (chunks_[i].timestamp_end > max_time_)
          max_time_ = chunks_[i].timestamp_end;
      }
      filter_min_time_ = 0;
      filter_max_time_ = max_time_;
      
      // populate chunk aggregate vectors
      for (auto& t : traces_) {
        Aggregate(t.aggregate, t.max_aggregate, t.chunks);
      }
      aggregates_.reserve(num_chunks_*2);
    }

    void AggregateAll(vector<TimeValue>& values) {
      // build aggregate structure
      // bin via sampling into times and values arrays
      if (aggregates_.empty()) Aggregate(aggregates_, max_aggregate_, chunks_, num_chunks_);
      SampleValues(aggregates_, values);
    }
    
    void AggregateTrace(vector<TimeValue>& values, int trace_index) {
      // build aggregate structure
      // bin via sampling into times and values arrays
      Trace& t = traces_[trace_index];
      SampleValues(t.aggregate, values);
    }

    uint64_t MaxAggregate() { return max_aggregate_; }
    uint64_t MaxTime() { return max_time_; }
    uint64_t MinTime() { return min_time_; }
    uint64_t FilterMaxTime() { return filter_max_time_; }
    uint64_t FilterMinTime() { return filter_min_time_; }

    void SetTraceFilter(string& str) {
      for (auto& s : trace_filters_) 
        if (s == str)
          return;
      trace_filters_.push_back(str);
      bool filtered = false;
      for (auto& trace : traces_) {
        if (trace.trace.find(str) == string::npos) {
          trace.filtered = true;
          filtered = true;
        }
      }
      if (filtered)
        aggregates_.clear();
    }

    void TraceFilterReset() {
      if (!trace_filters_.empty()) aggregates_.clear();
      trace_filters_.clear();
      for (auto& trace : traces_)
        trace.filtered = false;
    }

    void Traces(vector<TraceValue>& traces) {
      TraceValue tmp;
      traces.reserve(traces_.size());
      for (int i = 0; i < traces_.size(); i++) {
        if (traces_[i].filtered)
          continue;

        tmp.trace = &traces_[i].trace;
        tmp.trace_index = i;
        tmp.chunk_index = 0;
        bool overlaps = false;
        for (auto& chunk : traces_[i].chunks) {
          if (chunk->timestamp_start < filter_max_time_ && chunk->timestamp_end > filter_min_time_) {
            overlaps = true;
            break;
          }
        }
        if (!overlaps) continue;

        tmp.num_chunks = traces_[i].chunks.size();
        traces.push_back(tmp);
      }
    }

    void TraceChunks(std::vector<Chunk*>& chunks, int trace_index, int chunk_index, int num_chunks) {
      // TODO adjust indexing to account for time filtering
      chunks.reserve(num_chunks);
      if (trace_index >= traces_.size()) {
        cout << "TRACE INDEX OVER SIZE\n";
        return;
      }
      Trace& t = traces_[trace_index];
      if (chunk_index >= t.chunks.size()) {
        cout << "CHUNK INDEX OVER SIZE\n";
        return;
      }
      int bound = chunk_index + num_chunks;
      if (bound > t.chunks.size())
        bound = t.chunks.size();
      for (int i = chunk_index; i < bound; i++) {
        chunks.push_back(t.chunks[i]);
      }
    }

    void SetFilterMinMax(uint64_t min, uint64_t max) {
      if (min >= max) return;
      if (max > max_time_) return;

      filter_min_time_ = min;
      filter_max_time_ = max;
    }

    void FilterMinMaxReset() {
      filter_min_time_ = min_time_;
      filter_max_time_ = max_time_;
    }

  private:

    Chunk* chunks_;
    vector<TimeValue> aggregates_;
    uint32_t num_chunks_;
    vector<Trace> traces_;
    char* chunk_ptr_ = nullptr;
    uint64_t min_time_ = UINT64_MAX;
    uint64_t max_time_ = 0;
    uint64_t max_aggregate_ = 0;
    uint64_t filter_max_time_;
    uint64_t filter_min_time_;

    vector<string> trace_filters_;
    priority_queue<TimeValue> queue_;

    void SampleValues(vector<TimeValue>& points, vector<TimeValue>& values) {
      values.reserve(MAX_POINTS+2);
      // first, find the filtered interval
      cout << "points size is " << points.size() << endl;
      int j = 0;
      for(; j < points.size() && points[j].time < filter_min_time_; j++);
      int min = j;
      j++;
      cout << "min " << min << " j now " << j << endl;
      if (j == points.size()) {
        // basically, there were no points inside the interval, 
        // so we set a straight line of the appropriate value during the filter interval
        values.push_back({filter_min_time_, points[min-1].value});
        values.push_back({filter_max_time_, points[min-1].value});
        return;
      }
      for(; j < points.size() && points[j].time <= filter_max_time_; j++);
      int max = j;
      //cout << "max " << max << endl;
      int num_points = max - min + 1;
      if (num_points == 0 || num_points < 0) {
        cout << "num points is fucked " << num_points <<  endl;
        return;
      }


      if (num_points > MAX_POINTS) {
        values.push_back({filter_min_time_, points[min == 0 ? min : min-1].value});
        // sample MAX POINTS points
        float interval = (float)num_points / (float)MAX_POINTS;
        int i = 0;
        float index = 0.0f;
        while (i < MAX_POINTS) {
          int idx = (int) index + min;
          if (idx >= points.size()) {
            cout << " OH FUCK";
            break;
          }
          values.push_back(points[idx]);
          index += interval;
          i++;
        }
      } else {
        values.resize(max-min+1);
        values[0] = {filter_min_time_, points[min == 0 ? min : min-1].value};
        memcpy(&values[1], &points[min], sizeof(TimeValue)*(max-min));
      }
      //cout << "first value is " << values[0].value << " at time " << values[0].time << endl;
      values.push_back({filter_max_time_, values[values.size()-1].value});
    }

    void Aggregate(vector<TimeValue>& points, uint64_t& max_aggregate, 
        Chunk* chunks, int num_chunks) {
      if (!queue_.empty()) {
        cout << "THE QUEUE ISNT EMPTY WTF";
        return;
      }
      TimeValue tmp;
      int64_t running = 0;
      points.clear();
      points.push_back({0,0});

      int i = 0;
      while (i < num_chunks) {
        if (traces_[chunks[i].stack_index].filtered) {
          i++;
          continue;
        }
        if (!queue_.empty() && queue_.top().time < chunks[i].timestamp_start) {
          tmp.time = queue_.top().time;
          running += queue_.top().value;
          tmp.value = running;
          queue_.pop();
          points.push_back(tmp);
        } else {
          running += chunks[i].size;
          if (running > max_aggregate)
            max_aggregate = running;
          tmp.time = chunks[i].timestamp_start;
          tmp.value = running;
          points.push_back(tmp);
          tmp.time = chunks[i].timestamp_end;
          tmp.value = -chunks[i].size;
          queue_.push(tmp);
          i++;
        }
      }
      // drain the queue
      while (!queue_.empty()) {
          tmp.time = queue_.top().time;
          running += queue_.top().value;
          tmp.value = running;
          queue_.pop();
          points.push_back(tmp);
      }
      //points.push_back({max_time_,0});
    }
  
    // TODO deduplicate this code
    void Aggregate(vector<TimeValue>& points, uint64_t& max_aggregate, 
        vector<Chunk*>& chunks) {
      int num_chunks = chunks.size();
      if (!queue_.empty()) {
        cout << "THE QUEUE ISNT EMPTY WTF";
        return;
      }
      TimeValue tmp;
      int64_t running = 0;
      points.clear();
      points.push_back({0,0});

      int i = 0;
      while (i < num_chunks) {
        if (traces_[chunks[i]->stack_index].filtered) {
          i++;
          continue;
        }
        if (!queue_.empty() && queue_.top().time < chunks[i]->timestamp_start) {
          tmp.time = queue_.top().time;
          running += queue_.top().value;
          tmp.value = running;
          queue_.pop();
          points.push_back(tmp);
        } else {
          running += chunks[i]->size;
          if (running > max_aggregate)
            max_aggregate = running;
          tmp.time = chunks[i]->timestamp_start;
          tmp.value = running;
          points.push_back(tmp);
          tmp.time = chunks[i]->timestamp_end;
          tmp.value = -chunks[i]->size;
          queue_.push(tmp);
          i++;
        }
      }
      // drain the queue
      while (!queue_.empty()) {
          tmp.time = queue_.top().time;
          running += queue_.top().value;
          tmp.value = running;
          queue_.pop();
          points.push_back(tmp);
      }
    }

};

// its just easier this way ...
static Dataset theDataset;

void SetDataset(std::string& file_path) {
  theDataset.Reset(file_path);
}

void AggregateAll(std::vector<TimeValue>& values) {
  theDataset.AggregateAll(values);
}

uint64_t MaxAggregate() {
  return theDataset.MaxAggregate();
}

// TODO maybe these should just default to the filtered numbers?
uint64_t MaxTime() {
  return theDataset.MaxTime();
}

uint64_t MinTime() {
  return theDataset.MinTime();
}

uint64_t FilterMaxTime() {
  return theDataset.FilterMaxTime();
}

uint64_t FilterMinTime() {
  return theDataset.FilterMinTime();
}

void SetTraceKeyword(std::string& keyword) {
  // will only include traces that contain this keyword
  theDataset.SetTraceFilter(keyword);
}

void Traces(std::vector<TraceValue>& traces) {
  theDataset.Traces(traces);
}

void AggregateTrace(std::vector<TimeValue>& values, int trace_index) {
  theDataset.AggregateTrace(values, trace_index);
}

void TraceChunks(std::vector<Chunk*>& chunks, int trace_index, int chunk_index, int num_chunks) {
  theDataset.TraceChunks(chunks, trace_index, chunk_index, num_chunks);
}

void SetFilterMinMax(uint64_t min, uint64_t max) {
  theDataset.SetFilterMinMax(min, max);
}

void TraceFilterReset() {
  theDataset.TraceFilterReset();
}

void FilterMinMaxReset() {
  theDataset.FilterMinMaxReset();
}


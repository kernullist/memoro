
#include <node.h>
#include <iostream>
#include <v8.h>
#include "autopsy.h" 

using namespace v8;

void Autopsy_SetDataset(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();

  v8::String::Utf8Value s(args[0]);
  std::string file_path(*s);

  SetDataset(file_path);
  
}

void Autopsy_AggregateAll(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();
  std::vector<TimeValue> values;
  AggregateAll(values);
  //std::cout << "got " << values.size() << " time points!!\n";

  Local<Array> result_list = Array::New(isolate);
  for (unsigned int i = 0; i < values.size(); i++ ) {
    Local<Object> result = Object::New(isolate);
    result->Set(String::NewFromUtf8(isolate, "ts"), 
                            Number::New(isolate, values[i].time));
    result->Set(String::NewFromUtf8(isolate, "value"), 
                            Number::New(isolate, values[i].value));
    result_list->Set(i, result);
  }

  args.GetReturnValue().Set(result_list);
}

void Autopsy_AggregateTrace(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();
  static std::vector<TimeValue> values;
  values.clear();

  int trace_index = args[0]->NumberValue();

  AggregateTrace(values, trace_index);
  //std::cout << "got " << values.size() << " time points!!\n";

  Local<Array> result_list = Array::New(isolate);
  for (unsigned int i = 0; i < values.size(); i++ ) {
    Local<Object> result = Object::New(isolate);
    result->Set(String::NewFromUtf8(isolate, "ts"), 
                            Number::New(isolate, values[i].time));
    result->Set(String::NewFromUtf8(isolate, "value"), 
                            Number::New(isolate, values[i].value));
    result_list->Set(i, result);
  }

  args.GetReturnValue().Set(result_list);
}

void Autopsy_TraceChunks(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();
  static std::vector<Chunk*> chunks;
  chunks.clear();

  int trace_index = args[0]->NumberValue();
  int chunk_index = args[1]->NumberValue();
  int num_chunks  = args[2]->NumberValue();

  TraceChunks(chunks, trace_index, chunk_index, num_chunks);
  //std::cout << "got " << values.size() << " time points!!\n";

  Local<Array> result_list = Array::New(isolate);
  for (unsigned int i = 0; i < chunks.size(); i++ ) {
    Local<Object> result = Object::New(isolate);
    result->Set(String::NewFromUtf8(isolate, "num_reads"), 
                            Number::New(isolate, chunks[i]->num_reads));
    result->Set(String::NewFromUtf8(isolate, "num_writes"), 
                            Number::New(isolate, chunks[i]->num_writes));
    result->Set(String::NewFromUtf8(isolate, "size"), 
                            Number::New(isolate, chunks[i]->size));
    result->Set(String::NewFromUtf8(isolate, "ts_start"), 
                            Number::New(isolate, chunks[i]->timestamp_start));
    result->Set(String::NewFromUtf8(isolate, "ts_end"), 
                            Number::New(isolate, chunks[i]->timestamp_end));
    result->Set(String::NewFromUtf8(isolate, "ts_first"), 
                            Number::New(isolate, chunks[i]->timestamp_first_access));
    result->Set(String::NewFromUtf8(isolate, "ts_last"), 
                            Number::New(isolate, chunks[i]->timestamp_last_access));
    result_list->Set(i, result);
  }

  args.GetReturnValue().Set(result_list);
}

void Autopsy_Traces(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();
  static std::vector<TraceValue> traces; // just reuse this
  traces.clear();
  Traces(traces);

  Local<Array> result_list = Array::New(isolate);
  for (unsigned int i = 0; i < traces.size(); i++ ) {
    Local<Object> result = Object::New(isolate);
    result->Set(String::NewFromUtf8(isolate, "trace"), 
                            String::NewFromUtf8(isolate, traces[i].trace->c_str()));
    result->Set(String::NewFromUtf8(isolate, "trace_index"), 
                            Number::New(isolate, traces[i].trace_index));
    result->Set(String::NewFromUtf8(isolate, "num_chunks"), 
                            Number::New(isolate, traces[i].num_chunks));
    result->Set(String::NewFromUtf8(isolate, "chunk_index"), 
                            Number::New(isolate, traces[i].chunk_index));
    result_list->Set(i, result);
  }

  args.GetReturnValue().Set(result_list);
}

void Autopsy_MaxTime(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, MaxTime());

  args.GetReturnValue().Set(retval);
}

void Autopsy_MinTime(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, MinTime());

  args.GetReturnValue().Set(retval);
}

void Autopsy_FilterMaxTime(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, FilterMaxTime());

  args.GetReturnValue().Set(retval);
}

void Autopsy_FilterMinTime(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, FilterMinTime());

  args.GetReturnValue().Set(retval);
}

void Autopsy_MaxAggregate(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, MaxAggregate());

  args.GetReturnValue().Set(retval);
}

void Autopsy_SetTraceKeyword(const v8::FunctionCallbackInfo<v8::Value> & args) {

  v8::String::Utf8Value s(args[0]);
  std::string keyword(*s);

  SetTraceKeyword(keyword);
}

void Autopsy_SetFilterMinMax(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();

  uint64_t t1 = args[0]->NumberValue();
  uint64_t t2 = args[1]->NumberValue();
  SetFilterMinMax(t1, t2);

}

void Autopsy_TraceFilterReset(const v8::FunctionCallbackInfo<v8::Value> & args) {
  TraceFilterReset();
}

void Autopsy_FilterMinMaxReset(const v8::FunctionCallbackInfo<v8::Value> & args) {
  FilterMinMaxReset();
}

void init(Handle <Object> exports, Handle<Object> module) {
  NODE_SET_METHOD(exports, "set_dataset", Autopsy_SetDataset);
  NODE_SET_METHOD(exports, "aggregate_all", Autopsy_AggregateAll);
  NODE_SET_METHOD(exports, "max_time", Autopsy_MaxTime);
  NODE_SET_METHOD(exports, "min_time", Autopsy_MinTime);
  NODE_SET_METHOD(exports, "filter_max_time", Autopsy_FilterMaxTime);
  NODE_SET_METHOD(exports, "filter_min_time", Autopsy_FilterMinTime);
  NODE_SET_METHOD(exports, "max_aggregate", Autopsy_MaxAggregate);
  NODE_SET_METHOD(exports, "set_trace_keyword", Autopsy_SetTraceKeyword);
  NODE_SET_METHOD(exports, "traces", Autopsy_Traces);
  NODE_SET_METHOD(exports, "aggregate_trace", Autopsy_AggregateTrace);
  NODE_SET_METHOD(exports, "trace_chunks", Autopsy_TraceChunks);
  NODE_SET_METHOD(exports, "set_filter_minmax", Autopsy_SetFilterMinMax);
  NODE_SET_METHOD(exports, "trace_filter_reset", Autopsy_TraceFilterReset);
  NODE_SET_METHOD(exports, "filter_minmax_reset", Autopsy_FilterMinMaxReset);
}

NODE_MODULE(autopsy, init)


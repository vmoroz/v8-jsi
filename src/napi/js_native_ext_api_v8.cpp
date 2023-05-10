// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
// ----------------------------------------------------------------------------
// Some code is copied from Node.js project to compile V8 NAPI code
// without major changes.
// ----------------------------------------------------------------------------
// Original Node.js copyright:
//
// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "env-inl.h"

#include "V8JsiRuntime_impl.h"
#include "js_native_api_v8.h"
#include "public/ScriptStore.h"
#include "public/v8_api.h"

// namespace v8impl {

// // Responsible for notifying V8Runtime that the NAPI env is destroyed.
// struct V8RuntimeHolder : protected v8impl::RefTracker {
//   V8RuntimeHolder(napi_env env, v8runtime::V8Runtime *runtime) : runtime_{std::move(runtime)} {
//     Link(&env->finalizing_reflist);
//   }

//   ~V8RuntimeHolder() override {
//     Unlink();
//   }

//   void Finalize(bool is_env_teardown) override {
//     runtime_->SetIsEnvDeleted();
//     delete this;
//   }

//  private:
//   v8runtime::V8Runtime *runtime_;
// };

// } // namespace v8impl

napi_status napi_ext_has_unhandled_promise_rejection(napi_env env, bool *result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  auto runtime = v8runtime::V8Runtime::GetCurrent(env->context());
  CHECK_ARG(env, runtime);

  *result = runtime->HasUnhandledPromiseRejection();
  return napi_ok;
}

napi_status napi_get_and_clear_last_unhandled_promise_rejection(napi_env env, napi_value *result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  auto runtime = v8runtime::V8Runtime::GetCurrent(env->context());
  CHECK_ARG(env, runtime);

  auto rejectionInfo = runtime->GetAndClearLastUnhandledPromiseRejection();
  *result = v8impl::JsValueFromV8LocalValue(rejectionInfo->value.Get(env->isolate));
  return napi_ok;
}

napi_status napi_ext_run_script(napi_env env, napi_value source, const char *source_url, napi_value *result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, source);
  CHECK_ARG(env, source_url);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> v8_source = v8impl::V8LocalValueFromJsValue(source);

  if (!v8_source->IsString()) {
    return napi_set_last_error(env, napi_string_expected);
  }

  v8::Local<v8::Context> context = env->context();

  v8::Local<v8::String> urlV8String = v8::String::NewFromUtf8(context->GetIsolate(), source_url).ToLocalChecked();
  v8::ScriptOrigin origin(context->GetIsolate(), urlV8String);

  auto maybe_script = v8::Script::Compile(context, v8::Local<v8::String>::Cast(v8_source), &origin);
  CHECK_MAYBE_EMPTY(env, maybe_script, napi_generic_failure);

  auto script_result = maybe_script.ToLocalChecked()->Run(context);
  CHECK_MAYBE_EMPTY(env, script_result, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(script_result.ToLocalChecked());
  return GET_RETURN_STATUS(env);
}

//   NAPI_PREAMBLE(env);
//   CHECK_ARG(env, script_buffer);
//   CHECK_ARG(env, result);

//   std::shared_ptr<const facebook::jsi::Buffer> buffer{NodeApiJsiBuffer::CreateJsiBuffer(script_buffer)};
//   auto runtime = v8runtime::V8Runtime::GetCurrent(env->context());

//   std::uint64_t hash{0};
//   v8::Local<v8::String> sourceV8String = runtime->loadJavaScript(buffer, hash);
//   v8::Local<v8::Value> res = runtime->ExecuteString(sourceV8String, std::string(source_url ? source_url : ""), hash);

//   *result = v8impl::JsValueFromV8LocalValue(res);
//   return GET_RETURN_STATUS(env);
// }

// napi_status napi_ext_run_serialized_script(
//     napi_env env,
//     uint8_t const *buffer,
//     size_t buffer_length,
//     napi_value source,
//     char const *source_url,
//     napi_value *result) {
//   NAPI_PREAMBLE(env);
//   if (!buffer || !buffer_length) {
//     return napi_ext_run_script(env, source, source_url, result);
//   }
//   CHECK_ARG(env, source);
//   CHECK_ARG(env, source_url);
//   CHECK_ARG(env, result);

//   v8::Local<v8::Value> v8_source = v8impl::V8LocalValueFromJsValue(source);

//   if (!v8_source->IsString()) {
//     return napi_set_last_error(env, napi_string_expected);
//   }

//   v8::Local<v8::Context> context = env->context();

//   v8::Local<v8::String> urlV8String = v8::String::NewFromUtf8(context->GetIsolate(), source_url).ToLocalChecked();
//   v8::ScriptOrigin origin(context->GetIsolate(), urlV8String);

//   auto cached_data = new v8::ScriptCompiler::CachedData(buffer, static_cast<int>(buffer_length));
//   v8::ScriptCompiler::Source script_source(v8::Local<v8::String>::Cast(v8_source), origin, cached_data);
//   auto options = v8::ScriptCompiler::CompileOptions::kConsumeCodeCache;

//   auto maybe_script = v8::ScriptCompiler::Compile(context, &script_source, options);
//   CHECK_MAYBE_EMPTY(env, maybe_script, napi_generic_failure);

//   auto script_result = maybe_script.ToLocalChecked()->Run(context);
//   CHECK_MAYBE_EMPTY(env, script_result, napi_generic_failure);

//   *result = v8impl::JsValueFromV8LocalValue(script_result.ToLocalChecked());
//   return GET_RETURN_STATUS(env);
// }

// napi_status napi_ext_serialize_script(
//     napi_env env,
//     napi_value source,
//     char const *source_url,
//     napi_ext_buffer_callback buffer_cb,
//     void *buffer_hint) {
//   NAPI_PREAMBLE(env);
//   CHECK_ARG(env, source);
//   CHECK_ARG(env, buffer_cb);

//   v8::Local<v8::Value> v8_source = v8impl::V8LocalValueFromJsValue(source);

//   if (!v8_source->IsString()) {
//     return napi_set_last_error(env, napi_string_expected);
//   }

//   v8::Local<v8::Context> context = env->context();

//   v8::Local<v8::String> urlV8String = v8::String::NewFromUtf8(context->GetIsolate(), source_url).ToLocalChecked();
//   v8::ScriptOrigin origin(context->GetIsolate(), urlV8String);

//   v8::Local<v8::UnboundScript> script;
//   v8::ScriptCompiler::Source script_source(v8::Local<v8::String>::Cast(v8_source), origin);

//   if (v8::ScriptCompiler::CompileUnboundScript(context->GetIsolate(), &script_source).ToLocal(&script)) {
//     v8::ScriptCompiler::CachedData *code_cache = v8::ScriptCompiler::CreateCodeCache(script);

//     buffer_cb(env, code_cache->data, code_cache->length, buffer_hint);
//   }

//   return GET_RETURN_STATUS(env);
// }

napi_status napi_ext_collect_garbage(napi_env env) {
  env->isolate->RequestGarbageCollectionForTesting(v8::Isolate::kFullGarbageCollection);
  return napi_status::napi_ok;
}

namespace node {

namespace per_process {
// From node.cc
// Tells whether the per-process V8::Initialize() is called and
// if it is safe to call v8::Isolate::GetCurrent().
bool v8_initialized = false;
} // namespace per_process

// From node_errors.cc
[[noreturn]] void Assert(const AssertionInfo &info) {
#ifdef _WIN32
  char *processName{};
  _get_pgmptr(&processName);

  fprintf(
      stderr,
      "%s: %s:%s%s Assertion `%s' failed.\n",
      processName,
      info.file_line,
      info.function,
      *info.function ? ":" : "",
      info.message);
  fflush(stderr);
#endif // _WIN32

  TRACEV8RUNTIME_CRITICAL("Assertion failed");

  std::terminate();
}

} // namespace node

// TODO: [vmoroz] verify that finalize_cb runs in JS thread
// The created Buffer is the Uint8Array as in Node.js with version >= 4.
napi_status napi_create_external_buffer(
    napi_env env,
    size_t length,
    void *data,
    napi_finalize finalize_cb,
    void *finalize_hint,
    napi_value *result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  struct DeleterData {
    napi_env env;
    napi_finalize finalize_cb;
    void *finalize_hint;
  };

  v8::Isolate *isolate = env->isolate;

  DeleterData *deleterData = finalize_cb != nullptr ? new DeleterData{env, finalize_cb, finalize_hint} : nullptr;
  auto backingStore = v8::ArrayBuffer::NewBackingStore(
      data,
      length,
      [](void *data, size_t length, void *deleter_data) {
        DeleterData *deleterData = static_cast<DeleterData *>(deleter_data);
        if (deleterData != nullptr) {
          deleterData->finalize_cb(deleterData->env, data, deleterData->finalize_hint);
          delete deleterData;
        }
      },
      deleterData);

  v8::Local<v8::ArrayBuffer> arrayBuffer =
      v8::ArrayBuffer::New(isolate, std::shared_ptr<v8::BackingStore>(std::move(backingStore)));

  v8::Local<v8::Uint8Array> buffer = v8::Uint8Array::New(arrayBuffer, 0, length);

  *result = v8impl::JsValueFromV8LocalValue(buffer);
  return GET_RETURN_STATUS(env);
}

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

#define CHECKED_ENV(env) ((env) == nullptr) ? napi_invalid_arg : reinterpret_cast<v8impl::V8RuntimeEnv *>(env)

namespace v8impl {

class NodeApiJsiBuffer : public facebook::jsi::Buffer {
 public:
  NodeApiJsiBuffer(
      const uint8_t *data,
      size_t byteCount,
      napi_ext_data_delete_cb deleteDataCallback,
      void *deleterData) noexcept
      : data_(data), byteCount_(byteCount), deleteDataCallback_(deleteDataCallback), deleterData_(deleterData) {}

  ~NodeApiJsiBuffer() override {
    if (deleteDataCallback_ != nullptr) {
      deleteDataCallback_(const_cast<uint8_t *>(data_), deleterData_);
    }
  }

  NodeApiJsiBuffer(const NodeApiJsiBuffer &) = delete;
  NodeApiJsiBuffer &operator=(const NodeApiJsiBuffer &) = delete;

  const uint8_t *data() const override {
    return data_;
  }

  size_t size() const override {
    return byteCount_;
  }

 private:
  const uint8_t *data_{};
  size_t byteCount_{};
  napi_ext_data_delete_cb deleteDataCallback_{};
  void *deleterData_{};
};

class V8RuntimeEnv : public napi_env__, public v8runtime::V8Runtime {
 public:
  ~V8RuntimeEnv() override {}

  napi_status collectGarbage() {
    isolate->RequestGarbageCollectionForTesting(v8::Isolate::kFullGarbageCollection);
    return napi_status::napi_ok;
  }

  napi_status hasUnhandledPromiseRejection(bool *result) {
    CHECK_ARG(env, result);
    *result = HasUnhandledPromiseRejection();
    return napi_ok;
  }

  napi_status getAndClearLastUnhandledPromiseRejection(napi_value *result) {
    CHECK_ARG(env, result);
    auto rejectionInfo = GetAndClearLastUnhandledPromiseRejection();
    *result = v8impl::JsValueFromV8LocalValue(rejectionInfo->value.Get(isolate));
    return napi_ok;
  }

  napi_status runScript(napi_value source, const char *source_url, napi_value *result) {
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

  napi_status createPreparedScript(
      const uint8_t *scriptData,
      size_t scriptLength,
      napi_ext_data_delete_cb scriptDeleteCallback,
      void *deleterData,
      const char *sourceUrl,
      napi_ext_prepared_script *result) {
    NAPI_PREAMBLE(env);
    CHECK_ARG(env, scriptData);
    CHECK_ARG(env, sourceUrl);
    CHECK_ARG(env, result);
    std::shared_ptr<facebook::jsi::Buffer> scriptBuffer = std::shared_ptr<facebook::jsi::Buffer>(
        new NodeApiJsiBuffer(scriptData, scriptLength, scriptDeleteCallback, deleterData));
    std::shared_ptr<const facebook::jsi::PreparedJavaScript> preparedScript =
        prepareJavaScript2(scriptBuffer, sourceUrl);
    *result = reinterpret_cast<napi_ext_prepared_script>(
        new std::shared_ptr<facebook::jsi::PreparedJavaScript>(std::move(preparedScript)));
  }

  napi_status deletePreparedScript(napi_ext_prepared_script preparedScript) {
    CHECK_ARG(env, preparedScript);
    std::shared_ptr<facebook::jsi::PreparedJavaScript> *script =
        reinterpret_cast<std::shared_ptr<facebook::jsi::PreparedJavaScript> *>(preparedScript);
    delete script;
    return napi_clear_last_error(env);
  }

  napi_status runPreparedScript(napi_ext_prepared_script preparedScript, napi_value *result) {
    NAPI_PREAMBLE(env);
    CHECK_ARG(env, preparedScript);
    CHECK_ARG(env, result);

    std::shared_ptr<facebook::jsi::PreparedJavaScript> *script =
        reinterpret_cast<std::shared_ptr<facebook::jsi::PreparedJavaScript> *>(preparedScript);
    v8::MaybeLocal<v8::Value> maybeScriptResult = evaluatePreparedJavaScript2(*script);

    v8::Local<v8::Value> scriptResult = v8impl::V8LocalValueFromJsValue(source);
    CHECK_MAYBE_EMPTY(env, scriptResult, napi_generic_failure);

    *result = v8impl::JsValueFromV8LocalValue(scriptResult.ToLocalChecked());
    return GET_RETURN_STATUS(env);
  }

 private:
  napi_env env{this};
};

} // namespace v8impl

// Provides a hint to run garbage collection.
// It is typically used for unit tests.
NAPI_API
napi_ext_collect_garbage(napi_env env) {
  return CHECKED_ENV(env)->collectGarbage();
}

// Checks if the environment has an unhandled promise rejection.
NAPI_API napi_ext_has_unhandled_promise_rejection(napi_env env, bool *result) {
  return CHECKED_ENV(env)->hasUnhandledPromiseRejection(result);
}

// Gets and clears the last unhandled promise rejection.
NAPI_API napi_get_and_clear_last_unhandled_promise_rejection(napi_env env, napi_value *result) {
  return CHECKED_ENV(env)->getAndClearLastUnhandledPromiseRejection(result);
}

// To implement JSI description()
NAPI_API napi_ext_get_description(napi_env env, char *buf, size_t bufsize, size_t *result) {
  return CHECKED_ENV(env)->getDescription(buf, bufsize, result);
}

// To implement JSI drainMicrotasks()
NAPI_API napi_ext_drain_microtasks(napi_env env, int32_t max_count_hint, bool *result) {
  return CHECKED_ENV(env)->drainMicrotasks(buf, bufsize, result);
}

// To implement JSI isInspectable()
NAPI_API napi_ext_is_inspectable(napi_env env, bool *result) {
  return CHECKED_ENV(env)->isInspectable(result);
}

// Run script with source URL.
NAPI_API napi_ext_run_script(napi_env env, napi_value source, const char *source_url, napi_value *result) {
  return CHECKED_ENV(env)->runScript(source, source_url, result);
}

// Prepare the script for running.
NAPI_API napi_ext_create_prepared_script(
    napi_env env,
    const uint8_t *script_data,
    size_t script_length,
    napi_ext_data_delete_cb script_delete_cb,
    void *deleter_data,
    const char *source_url,
    napi_ext_prepared_script *result) {
  return CHECKED_ENV(env)->createPreparedScript(
      script_data, script_length, script_delete_cb, deleter_data, source_url, result);
}

// Delete the prepared script.
NAPI_API napi_ext_delete_prepared_script(napi_env env, napi_ext_prepared_script prepared_script) {
  return CHECKED_ENV(env)->deletePreparedScript(prepared_script);
}

// Run the prepared script.
NAPI_API napi_ext_prepared_script_run(napi_env env, napi_ext_prepared_script prepared_script, napi_value *result) {
  return CHECKED_ENV(env)->runPreparedScript(prepared_script, result);
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

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
#include "js_native_ext_api.h"
#include "public/ScriptStore.h"

namespace v8impl {

// Reference counter implementation.
struct ExtRefCounter : protected RefTracker {
  ~ExtRefCounter() override {
    Unlink();
  }

  void Ref() {
    ++ref_count_;
  }

  void Unref() {
    if (--ref_count_ == 0) {
      Finalize(false);
    }
  }

  virtual v8::Local<v8::Value> Get(napi_env env) = 0;

 protected:
  ExtRefCounter(napi_env env) {
    Link(&env->reflist);
  }

  void Finalize(bool is_env_teardown) override {
    delete this;
  }

 private:
  uint32_t ref_count_{1};
};

// Wrapper around v8impl::Persistent that implements reference counting.
struct ExtReference : protected ExtRefCounter {
  static ExtReference *New(napi_env env, v8::Local<v8::Value> value) {
    return new ExtReference(env, value);
  }

  v8::Local<v8::Value> Get(napi_env env) override {
    return persistent_.Get(env->isolate);
  }

 protected:
  ExtReference(napi_env env, v8::Local<v8::Value> value)
      : ExtRefCounter(env), persistent_(env->isolate, value) {}

 private:
  v8impl::Persistent<v8::Value> persistent_;
};

// Associates data with ExtReference.
struct ExtReferenceWithData : protected ExtReference {
  static ExtReferenceWithData *New(
      napi_env env,
      v8::Local<v8::Value> value,
      void *native_object,
      napi_finalize finalize_cb,
      void *finalize_hint) {
    return new ExtReferenceWithData(
        env, value, native_object, finalize_cb, finalize_hint);
  }

 protected:
  ExtReferenceWithData(
      napi_env env,
      v8::Local<v8::Value> value,
      void *native_object,
      napi_finalize finalize_cb,
      void *finalize_hint)
      : ExtReference(env, value),
        env_{env},
        native_object_{native_object},
        finalize_cb_{finalize_cb},
        finalize_hint_{finalize_hint} {}

  void Finalize(bool is_env_teardown) override {
    if (finalize_cb_) {
      finalize_cb_(env_, native_object_, finalize_hint_);
      finalize_cb_ = nullptr;
    }
    ExtReference::Finalize(is_env_teardown);
  }

 private:
  napi_env env_{nullptr};
  void *native_object_{nullptr};
  napi_finalize finalize_cb_{nullptr};
  void *finalize_hint_{nullptr};
};

// Wrapper around v8impl::Persistent that implements reference counting.
struct ExtWeakReference : protected ExtRefCounter {
  static ExtWeakReference *New(napi_env env, v8::Local<v8::Value> value) {
    return new ExtWeakReference(env, value);
  }

  ~ExtWeakReference() override {
    napi_delete_reference(env_, weak_ref_);
  }

  v8::Local<v8::Value> Get(napi_env env) override {
    napi_value result{};
    napi_get_reference_value(env, weak_ref_, &result);
    return result ? v8impl::V8LocalValueFromJsValue(result)
                  : v8::Local<v8::Value>();
  }

 protected:
  ExtWeakReference(napi_env env, v8::Local<v8::Value> value)
      : ExtRefCounter(env), env_{env} {
    napi_create_reference(
        env, v8impl::JsValueFromV8LocalValue(value), 0, &weak_ref_);
  }

 private:
  napi_env env_{nullptr};
  napi_ref weak_ref_{nullptr};
};

} // namespace v8impl

static struct napi_ext_env_scope__ {
  napi_ext_env_scope__(v8::Isolate *isolate, v8::Local<v8::Context> context)
      : isolate_scope(isolate ? new v8::Isolate::Scope(isolate) : nullptr),
        context_scope(
            !context.IsEmpty() ? new v8::Context::Scope(context) : nullptr) {}

  napi_ext_env_scope__(napi_ext_env_scope__ const &) = delete;
  napi_ext_env_scope__ &operator=(napi_ext_env_scope__ const &) = delete;

  napi_ext_env_scope__(napi_ext_env_scope__ &&other)
      : isolate_scope(other.isolate_scope), context_scope(other.context_scope) {
    other.isolate_scope = nullptr;
    other.context_scope = nullptr;
  }

  napi_ext_env_scope__ &operator=(napi_ext_env_scope__ &&other) {
    if (this != &other) {
      napi_ext_env_scope__ temp{std::move(*this)};
      Swap(other);
    }
    return *this;
  }

  void Swap(napi_ext_env_scope__ &other) {
    using std::swap;
    swap(isolate_scope, other.isolate_scope);
    swap(context_scope, other.context_scope);
  }

  ~napi_ext_env_scope__() {
    if (context_scope) {
      delete context_scope;
    }

    if (isolate_scope) {
      delete isolate_scope;
    }
  }

 private:
  v8::Isolate::Scope *isolate_scope{nullptr};
  v8::Context::Scope *context_scope{nullptr};
};

namespace {

// Responsible for V8Runtime destruction when the environment is destroyed.
struct V8RuntimeHolder : protected v8impl::RefTracker {
  V8RuntimeHolder(napi_env env, std::unique_ptr<v8runtime::V8Runtime> runtime)
      : runtime_{std::move(runtime)} {
    Link(&env->finalizing_reflist);
  }

  ~V8RuntimeHolder() override {
    Unlink();
  }

  void Finalize(bool is_env_teardown) override {
    delete this;
  }

 private:
  std::unique_ptr<v8runtime::V8Runtime> runtime_;
};

} // namespace

napi_status napi_ext_create_env(
    napi_ext_env_attributes attributes,
    napi_env *env) {
  v8runtime::V8RuntimeArgs args;
  args.trackGCObjectStats = false;
  args.enableTracing = false;
  args.enableJitTracing = false;
  args.enableMessageTracing = false;
  args.enableLog = false;
  args.enableGCTracing = false;

  if ((attributes & napi_ext_env_attribute_enable_gc_api) != 0) {
    args.enableGCApi = true;
  }

  if ((attributes & napi_ext_env_attribute_ignore_unhandled_promises) != 0) {
    args.ignoreUnhandledPromises = true;
  }

  auto runtime = std::make_unique<v8runtime::V8Runtime>(std::move(args));

  auto context = v8impl::PersistentToLocal::Strong(runtime->GetContext());
  *env = new napi_env__(context);

  // Keep the runtime as long as env exists.
  // It can be accessed from V8 context using V8Runtime::GetCurrent(context).
  V8RuntimeHolder runtimeHolder(*env, std::move(runtime));

  return napi_status::napi_ok;
}

napi_status napi_ext_clone_env(napi_env env) {
  CHECK_ENV(env);
  env->Ref();
  return napi_status::napi_ok;
}

napi_status napi_ext_release_env(napi_env env) {
  CHECK_ENV(env);
  env->Unref();
  return napi_status::napi_ok;
}

napi_status napi_ext_open_env_scope(napi_env env, napi_ext_env_scope *result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = new napi_ext_env_scope__(env->isolate, env->context());
  return napi_ok;
}

napi_status napi_ext_close_env_scope(napi_env env, napi_ext_env_scope scope) {
  CHECK_ENV(env);
  CHECK_ARG(env, scope);

  delete scope;
  return napi_ok;
}

napi_status napi_ext_has_unhandled_promise_rejection(
    napi_env env,
    bool *result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  auto runtime = v8runtime::V8Runtime::GetCurrent(env->context());
  CHECK_ARG(env, runtime);

  *result = runtime->HasUnhandledPromiseRejection();
  return napi_ok;
}

napi_status napi_get_and_clear_last_unhandled_promise_rejection(
    napi_env env,
    napi_value *result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  auto runtime = v8runtime::V8Runtime::GetCurrent(env->context());
  CHECK_ARG(env, runtime);

  auto rejectionInfo = runtime->GetAndClearLastUnhandledPromiseRejection();
  *result =
      v8impl::JsValueFromV8LocalValue(rejectionInfo->value.Get(env->isolate));
  return napi_ok;
}

napi_status napi_ext_run_script(
    napi_env env,
    napi_value source,
    const char *source_url,
    napi_value *result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, source);
  CHECK_ARG(env, source_url);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> v8_source = v8impl::V8LocalValueFromJsValue(source);

  if (!v8_source->IsString()) {
    return napi_set_last_error(env, napi_string_expected);
  }

  v8::Local<v8::Context> context = env->context();

  v8::Local<v8::String> urlV8String =
      v8::String::NewFromUtf8(context->GetIsolate(), source_url)
          .ToLocalChecked();
  v8::ScriptOrigin origin(urlV8String);

  auto maybe_script = v8::Script::Compile(
      context, v8::Local<v8::String>::Cast(v8_source), &origin);
  CHECK_MAYBE_EMPTY(env, maybe_script, napi_generic_failure);

  auto script_result = maybe_script.ToLocalChecked()->Run(context);
  CHECK_MAYBE_EMPTY(env, script_result, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(script_result.ToLocalChecked());
  return GET_RETURN_STATUS(env);
}

napi_status napi_ext_run_serialized_script(
    napi_env env,
    napi_value source,
    char const *source_url,
    uint8_t const *buffer,
    size_t buffer_length,
    napi_value *result) {
  NAPI_PREAMBLE(env);
  if (!buffer || !buffer_length) {
    return napi_ext_run_script(env, source, source_url, result);
  }
  CHECK_ARG(env, source);
  CHECK_ARG(env, source_url);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> v8_source = v8impl::V8LocalValueFromJsValue(source);

  if (!v8_source->IsString()) {
    return napi_set_last_error(env, napi_string_expected);
  }

  v8::Local<v8::Context> context = env->context();

  v8::Local<v8::String> urlV8String =
      v8::String::NewFromUtf8(context->GetIsolate(), source_url)
          .ToLocalChecked();
  v8::ScriptOrigin origin(urlV8String);

  auto cached_data = new v8::ScriptCompiler::CachedData(
      buffer, static_cast<int>(buffer_length));
  v8::ScriptCompiler::Source script_source(
      v8::Local<v8::String>::Cast(v8_source), origin, cached_data);
  auto options = v8::ScriptCompiler::CompileOptions::kConsumeCodeCache;

  auto maybe_script =
      v8::ScriptCompiler::Compile(context, &script_source, options);
  CHECK_MAYBE_EMPTY(env, maybe_script, napi_generic_failure);

  auto script_result = maybe_script.ToLocalChecked()->Run(context);
  CHECK_MAYBE_EMPTY(env, script_result, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(script_result.ToLocalChecked());
  return GET_RETURN_STATUS(env);
}

napi_status napi_ext_serialize_script(
    napi_env env,
    napi_value source,
    char const *source_url,
    napi_ext_buffer_callback buffer_cb,
    void *buffer_hint) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, source);
  CHECK_ARG(env, buffer_cb);

  v8::Local<v8::Value> v8_source = v8impl::V8LocalValueFromJsValue(source);

  if (!v8_source->IsString()) {
    return napi_set_last_error(env, napi_string_expected);
  }

  v8::Local<v8::Context> context = env->context();

  v8::Local<v8::String> urlV8String =
      v8::String::NewFromUtf8(context->GetIsolate(), source_url)
          .ToLocalChecked();
  v8::ScriptOrigin origin(urlV8String);

  v8::Local<v8::UnboundScript> script;
  v8::ScriptCompiler::Source script_source(
      v8::Local<v8::String>::Cast(v8_source), origin);

  if (v8::ScriptCompiler::CompileUnboundScript(
          context->GetIsolate(), &script_source)
          .ToLocal(&script)) {
    v8::ScriptCompiler::CachedData *code_cache =
        v8::ScriptCompiler::CreateCodeCache(script);

    buffer_cb(env, code_cache->data, code_cache->length, buffer_hint);
  }

  return GET_RETURN_STATUS(env);
}

napi_status napi_ext_collect_garbage(napi_env env) {
  env->isolate->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
  return napi_status::napi_ok;
}

NAPI_EXTERN napi_status napi_ext_get_unique_utf8_string_ref(
    napi_env env,
    const char *str,
    size_t length,
    napi_ext_ref *result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, str);
  CHECK_ARG(env, result);

  auto runtime = v8runtime::V8Runtime::GetCurrent(env->context());
  CHECK_ARG(env, runtime);
  STATUS_CALL(runtime->NapiGetUniqueUtf8StringRef(env, str, length, result));

  return GET_RETURN_STATUS(env);
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
  std::string name = "aaa"; // [vmoroz] GetHumanReadableProcessName();

  fprintf(
      stderr,
      "%s: %s:%s%s Assertion `%s' failed.\n",
      name.c_str(),
      info.file_line,
      info.function,
      *info.function ? ":" : "",
      info.message);
  fflush(stderr);

  // [vmoroz] Abort();
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

  DeleterData *deleterData = finalize_cb != nullptr
      ? new DeleterData{env, finalize_cb, finalize_hint}
      : nullptr;
  auto backingStore = v8::ArrayBuffer::NewBackingStore(
      data,
      length,
      [](void *data, size_t length, void *deleter_data) {
        DeleterData *deleterData = static_cast<DeleterData *>(deleter_data);
        if (deleterData != nullptr) {
          deleterData->finalize_cb(
              deleterData->env, data, deleterData->finalize_hint);
          delete deleterData;
        }
      },
      deleterData);

  v8::Local<v8::ArrayBuffer> arrayBuffer = v8::ArrayBuffer::New(
      isolate, std::shared_ptr<v8::BackingStore>(std::move(backingStore)));

  v8::Local<v8::Uint8Array> buffer =
      v8::Uint8Array::New(arrayBuffer, 0, length);

  *result = v8impl::JsValueFromV8LocalValue(buffer);
  return GET_RETURN_STATUS(env);
}

// Creates new napi_ext_ref with ref counter set to 1.
napi_status napi_ext_create_reference(
    napi_env env,
    napi_value value,
    napi_ext_ref *result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> v8_value = v8impl::V8LocalValueFromJsValue(value);

  v8impl::ExtReference *reference = v8impl::ExtReference::New(env, v8_value);

  *result = reinterpret_cast<napi_ext_ref>(reference);
  return napi_clear_last_error(env);
}

// Creates new napi_ext_ref and associates native data with the reference.
// The ref counter is set to 1.
napi_status napi_ext_create_reference_with_data(
    napi_env env,
    napi_value value,
    void *native_object,
    napi_finalize finalize_cb,
    void *finalize_hint,
    napi_ext_ref *result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, native_object);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> v8_value = v8impl::V8LocalValueFromJsValue(value);

  v8impl::ExtReferenceWithData *reference = v8impl::ExtReferenceWithData::New(
      env, v8_value, native_object, finalize_cb, finalize_hint);

  *result = reinterpret_cast<napi_ext_ref>(reference);
  return napi_clear_last_error(env);
}

napi_status napi_ext_create_weak_reference(
    napi_env env,
    napi_value value,
    napi_ext_ref *result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> v8_value = v8impl::V8LocalValueFromJsValue(value);

  v8impl::ExtWeakReference *reference =
      v8impl::ExtWeakReference::New(env, v8_value);

  *result = reinterpret_cast<napi_ext_ref>(reference);
  return napi_clear_last_error(env);
}

// Increments the reference count.
napi_status napi_ext_clone_reference(napi_env env, napi_ext_ref ref) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, ref);

  v8impl::ExtRefCounter *reference =
      reinterpret_cast<v8impl::ExtRefCounter *>(ref);
  reference->Ref();

  return napi_clear_last_error(env);
}

// Decrements the reference count.
// The provided ref must not be used after this call because it could be deleted
// if the internal ref counter became zero.
napi_status napi_ext_release_reference(napi_env env, napi_ext_ref ref) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, ref);

  v8impl::ExtRefCounter *reference =
      reinterpret_cast<v8impl::ExtRefCounter *>(ref);

  reference->Unref();

  return napi_clear_last_error(env);
}

// Gets the referenced value.
napi_status napi_ext_get_reference_value(
    napi_env env,
    napi_ext_ref ref,
    napi_value *result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, ref);
  CHECK_ARG(env, result);

  v8impl::ExtRefCounter *reference =
      reinterpret_cast<v8impl::ExtRefCounter *>(ref);
  *result = v8impl::JsValueFromV8LocalValue(reference->Get(env));

  return napi_clear_last_error(env);
}

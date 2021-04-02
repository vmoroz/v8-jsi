// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "ScriptStore.h"
#include "V8JsiRuntime_impl.h"
#include "js_engine_api.h"
#include "js_native_api_v8.h"
#include "js_native_api_v8_internals.h"
#include "src/flags/flags.h"
#include "v8.h"

// TODO: [vmoroz] Stop using global vars

std::unique_ptr<v8runtime::V8Runtime> runtime;
v8::Isolate *isolate_{nullptr};

struct napi_env_scope__ {
  napi_env_scope__(v8::Isolate *isolate, v8::Local<v8::Context> context)
      : isolate_scope(isolate ? new v8::Isolate::Scope(isolate) : nullptr),
        context_scope(!context.IsEmpty() ? new v8::Context::Scope(context) : nullptr) {}

  napi_env_scope__(napi_env_scope__ const &) = delete;
  napi_env_scope__ &operator=(napi_env_scope__ const &) = delete;

  napi_env_scope__(napi_env_scope__ &&other)
      : isolate_scope(other.isolate_scope), context_scope(other.context_scope) {
    other.isolate_scope = nullptr;
    other.context_scope = nullptr;
  }

  napi_env_scope__ &operator=(napi_env_scope__ &&other) {
    if (this != &other) {
      napi_env_scope__ temp{std::move(*this)};
      Swap(other);
    }
    return *this;
  }

  void Swap(napi_env_scope__ &other) {
    std::swap(isolate_scope, other.isolate_scope);
    std::swap(context_scope, other.context_scope);
  }

  ~napi_env_scope__() {
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

bool ignore_unhandled_promises_{false};
std::vector<std::tuple<
    v8::Global<v8::Promise>,
    v8::Global<v8::Message>,
    v8::Global<v8::Value>>>
    unhandled_promises_;

void PromiseRejectCallback(v8::PromiseRejectMessage data);

napi_status jse_create_env(jse_env_attributes /*attributes*/, napi_env *env) {
  v8runtime::V8RuntimeArgs args;
  ignore_unhandled_promises_ = false;

  // TODO: [vmoroz] use env attribute
  v8::internal::FLAG_expose_gc = true;

  unhandled_promises_.clear();

  runtime = std::make_unique<v8runtime::V8Runtime>(std::move(args));

  auto context = v8impl::PersistentToLocal::Strong(runtime->GetContext());

  isolate_ = context->GetIsolate();
  isolate_->SetPromiseRejectCallback(PromiseRejectCallback);

  *env = new napi_env__(context);
  return napi_status::napi_ok;
}

napi_status jse_delete_env(napi_env env) {
  delete env;
  runtime = nullptr;
  return napi_status::napi_ok;
}

napi_status jse_open_env_scope(napi_env env, napi_env_scope *result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = new napi_env_scope__(env->isolate, env->context());
  return napi_ok;
}

napi_status jse_close_env_scope(napi_env env, napi_env_scope scope) {
  CHECK_ENV(env);
  CHECK_ARG(env, scope);

  delete scope;
  return napi_ok;
}

napi_status jse_get_unhandled_promise_rejections(
    napi_env env,
    napi_value *buf,
    size_t bufsize,
    size_t startAt,
    size_t *result) {
  // TOOD: [vmoroz] check args
  if (bufsize == 0) {
    *result = startAt < unhandled_promises_.size()
        ? unhandled_promises_.size() - startAt
        : 0;
    return napi_ok;
  }
  size_t count = 0;
  for (size_t i = startAt; i < unhandled_promises_.size(); ++i) {
    const auto &tuple = unhandled_promises_[i];
    v8::Local<v8::Value> value = std::get<2>(tuple).Get(isolate_);
    buf[count] = v8impl::JsValueFromV8LocalValue(value);
    ++count;
  }
  *result = count;

  return napi_ok;
}

napi_status jse_clean_unhandled_promise_rejections(
    napi_env env,
    size_t *result) {
  // TOOD: [vmoroz] check args
  v8::HandleScope scope(isolate_);
  if (result) {
    *result = unhandled_promises_.size();
  }
  unhandled_promises_.clear();
  ignore_unhandled_promises_ = false;
  return napi_ok;
}

void RemoveUnhandledPromise(v8::Local<v8::Promise> promise) {
  if (ignore_unhandled_promises_)
    return;
  // Remove handled promises from the list
  DCHECK_EQ(promise->GetIsolate(), isolate_);
  auto removeIt = std::remove_if(
      std::begin(unhandled_promises_),
      std::end(unhandled_promises_),
      [&promise](auto const &entry) {
        v8::Local<v8::Promise> unhandled_promise =
            std::get<0>(entry).Get(isolate_);
        return unhandled_promise == promise;
      });
  unhandled_promises_.erase(removeIt, unhandled_promises_.end());
}

void AddUnhandledPromise(
    v8::Local<v8::Promise> promise,
    v8::Local<v8::Message> message,
    v8::Local<v8::Value> exception) {
  if (ignore_unhandled_promises_)
    return;
  DCHECK_EQ(promise->GetIsolate(), isolate_);
  unhandled_promises_.emplace_back(
      v8::Global<v8::Promise>(isolate_, promise),
      v8::Global<v8::Message>(isolate_, message),
      v8::Global<v8::Value>(isolate_, exception));
}

int HandleUnhandledPromiseRejections() {
  // Avoid recursive calls to HandleUnhandledPromiseRejections.
  if (ignore_unhandled_promises_)
    return 0;
  ignore_unhandled_promises_ = true;
  v8::HandleScope scope(isolate_);
  // Ignore promises that get added during error reporting.
  size_t i = 0;
  for (; i < unhandled_promises_.size(); i++) {
    const auto &tuple = unhandled_promises_[i];
    v8::Local<v8::Message> message = std::get<1>(tuple).Get(isolate_);
    v8::Local<v8::Value> value = std::get<2>(tuple).Get(isolate_);
    // Shell::ReportException(isolate_, message, value);
  }
  unhandled_promises_.clear();
  return static_cast<int>(i);
}

void PromiseRejectCallback(v8::PromiseRejectMessage data) {
  // if (options.ignore_unhandled_promises) return;
  if (data.GetEvent() == v8::kPromiseRejectAfterResolved ||
      data.GetEvent() == v8::kPromiseResolveAfterResolved) {
    // Ignore reject/resolve after resolved.
    return;
  }
  v8::Local<v8::Promise> promise = data.GetPromise();
  v8::Isolate *isolate = promise->GetIsolate();
  // PerIsolateData* isolate_data = PerIsolateData::Get(isolate);

  if (data.GetEvent() == v8::kPromiseHandlerAddedAfterReject) {
    RemoveUnhandledPromise(promise);
    return;
  }

  isolate->SetCaptureStackTraceForUncaughtExceptions(true);
  v8::Local<v8::Value> exception = data.GetValue();
  v8::Local<v8::Message> message;
  // Assume that all objects are stack-traces.
  if (exception->IsObject()) {
    message = v8::Exception::CreateMessage(isolate, exception);
  }
  if (!exception->IsNativeError() &&
      (message.IsEmpty() || message->GetStackTrace().IsEmpty())) {
    // If there is no real Error object, manually throw and catch a stack trace.
    v8::TryCatch try_catch(isolate);
    try_catch.SetVerbose(true);
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8Literal(isolate, "Unhandled Promise.")));
    message = try_catch.Message();
    exception = try_catch.Exception();
  }

  AddUnhandledPromise(promise, message, exception);
}

napi_status jse_run_script(
    napi_env env,
    napi_value script,
    const char *source_url,
    napi_value *result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, script);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> v8_script = v8impl::V8LocalValueFromJsValue(script);

  if (!v8_script->IsString()) {
    return napi_set_last_error(env, napi_string_expected);
  }

  v8::Local<v8::Context> context = env->context();

  v8::Local<v8::String> urlV8String =
      v8::String::NewFromUtf8(context->GetIsolate(), source_url)
          .ToLocalChecked();
  v8::ScriptOrigin origin(urlV8String);

  auto maybe_script = v8::Script::Compile(
      context, v8::Local<v8::String>::Cast(v8_script), &origin);
  CHECK_MAYBE_EMPTY(env, maybe_script, napi_generic_failure);

  auto script_result = maybe_script.ToLocalChecked()->Run(context);
  CHECK_MAYBE_EMPTY(env, script_result, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(script_result.ToLocalChecked());
  return GET_RETURN_STATUS(env);
}

napi_status jse_collect_garbage(napi_env env) {
  env->isolate->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
  return napi_status::napi_ok;
}

// From node.cc
namespace node {
namespace per_process {
// util.h
// Tells whether the per-process V8::Initialize() is called and
// if it is safe to call v8::Isolate::GetCurrent().
bool v8_initialized = false;
}  // namespace per_process
}  // namespace node

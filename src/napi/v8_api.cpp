// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "v8_api.h"
#include "V8JsiRuntime_impl.h"
#include "js_native_api_v8.h"
#include "js_native_api_v8_internals.h"
#include "public/ScriptStore.h"
#include "node_shim.h"

std::unique_ptr<v8runtime::V8Runtime> runtime;

node::IsolateData* isolateData{nullptr};
node::Environment* environment{nullptr};

struct IsolateScopeHolder {
  IsolateScopeHolder(v8::Isolate *isolate, v8::Local<v8::Context> *context)
      : isolate_scope(isolate ? new v8::Isolate::Scope(isolate) : nullptr),
        context_scope(context ? new v8::Context::Scope(*context) : nullptr) {}

  IsolateScopeHolder(IsolateScopeHolder const &) = delete;
  IsolateScopeHolder &operator=(IsolateScopeHolder const &) = delete;

  IsolateScopeHolder(IsolateScopeHolder &&other)
      : isolate_scope(other.isolate_scope), context_scope(other.context_scope) {
    other.isolate_scope = nullptr;
    other.context_scope = nullptr;
  }

  IsolateScopeHolder &operator=(IsolateScopeHolder &&other) {
    if (this != &other) {
      IsolateScopeHolder temp{std::move(*this)};
      Swap(other);
    }
    return *this;
  }

  void Swap(IsolateScopeHolder &other) {
    std::swap(isolate_scope, other.isolate_scope);
    std::swap(context_scope, other.context_scope);
  }

  ~IsolateScopeHolder() {
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

IsolateScopeHolder scopeHolder{nullptr, nullptr};

napi_env __cdecl v8_create_env() {
  v8runtime::V8RuntimeArgs args;

  runtime = std::make_unique<v8runtime::V8Runtime>(std::move(args));

  auto context = v8impl::PersistentToLocal::Strong(runtime->GetContext());

  isolateData = new node::IsolateData(context->GetIsolate());
  environment = new node::Environment(isolateData, context);

  scopeHolder = IsolateScopeHolder(context->GetIsolate(), &context);

  return new napi_env__(context);
}

void __cdecl v8_delete_env(napi_env env) {
  delete env;
  scopeHolder = IsolateScopeHolder(nullptr, nullptr);
  delete environment;
  delete isolateData;
  runtime = nullptr;
}

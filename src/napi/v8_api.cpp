// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "env-inl.h"

#include "V8JsiRuntime_impl.h"
#include "js_native_api_v8.h"
#include "public/ScriptStore.h"
#include "public/v8_api.h"

#define CHECKED_RUNTIME(runtime) (runtime == nullptr) ? v8_error : reinterpret_cast<v8impl::RuntimeWrapper *>(runtime)

#define CHECKED_CONFIG(config) (config == nullptr) ? v8_error : reinterpret_cast<v8impl::ConfigWrapper *>(config)

#define V8_CHECK_ARG(arg) \
  if (arg == nullptr) {   \
    return v8_error;      \
  }

// napi_status napi_create_v8_env(
//     ::v8::vm::Runtime &runtime,
//     bool isInspectable,
//     const ::v8::vm::RuntimeConfig &runtimeConfig,
//     napi_env *env);

// napi_status napi_ext_env_unref(napi_env env);

namespace v8impl {

class V8TaskRunner : public v8runtime::JSITaskRunner {
 public:
  V8TaskRunner(
      void *taskRunnerData,
      v8_task_runner_post_task_cb postTaskCallback,
      v8_data_delete_cb deleteCallback,
      void *deleterData)
      : taskRunnerData_(taskRunnerData),
        postTaskCallback_(postTaskCallback),
        deleteCallback_(deleteCallback),
        deleterData_(deleterData) {}

  ~V8TaskRunner() {
    if (deleteCallback_ != nullptr) {
      deleteCallback_(taskRunnerData_, deleterData_);
    }
  }

  void postTask(std::unique_ptr<v8runtime::JSITask> task) override {
    postTaskCallback_(
        taskRunnerData_,
        static_cast<void *>(task.release()),
        [](void *taskData) { static_cast<v8runtime::JSITask *>(taskData)->run(); },
        [](void *taskData, void * /*deleterData*/) { delete static_cast<v8runtime::JSITask *>(taskData); },
        /*deleterData:*/ nullptr);
  }

 private:
  void *taskRunnerData_; // a pointer to the task runner implementation
  v8_task_runner_post_task_cb postTaskCallback_;
  v8_data_delete_cb deleteCallback_;
  void *deleterData_;
};

class V8JsiBuffer : public facebook::jsi::Buffer {
 public:
  V8JsiBuffer(const uint8_t *data, size_t size, v8_data_delete_cb deleteCallback, void *deleterData)
      : data_(data), size_(size), deleteCallback_(deleteCallback), deleterData_(deleterData) {}

  ~V8JsiBuffer() override {
    if (deleteCallback_ != nullptr) {
      deleteCallback_(const_cast<uint8_t *>(data_), deleterData_);
    }
  }

  const uint8_t *data() const override {
    return data_;
  }

  size_t size() const override {
    return size_;
  }

 private:
  const uint8_t *data_{};
  size_t size_{};
  v8_data_delete_cb deleteCallback_{};
  void *deleterData_{};
};

class V8ScriptCache : public facebook::jsi::PreparedScriptStore {
 public:
  V8ScriptCache(
      void *scriptCacheData,
      v8_script_cache_load_cb scriptCacheLoadCallback,
      v8_script_cache_store_cb scriptCacheStoreCallback,
      v8_data_delete_cb scriptCacheDataDeleteCallback,
      void *deleterData) noexcept
      : scriptCacheData_(scriptCacheData),
        scriptCacheLoadCallback_(scriptCacheLoadCallback),
        scriptCacheStoreCallback_(scriptCacheStoreCallback),
        scriptCacheDataDeleteCallback_(scriptCacheDataDeleteCallback),
        deleterData_(deleterData) {}

  ~V8ScriptCache() override {
    if (scriptCacheDataDeleteCallback_) {
      scriptCacheDataDeleteCallback_(scriptCacheData_, deleterData_);
    }
  }

  std::shared_ptr<const facebook::jsi::Buffer> tryGetPreparedScript(
      const facebook::jsi::ScriptSignature &scriptSignature,
      const facebook::jsi::JSRuntimeSignature &runtimeMetadata,
      const char *prepareTag) noexcept override {
    const uint8_t *buffer{};
    size_t bufferSize{};
    v8_data_delete_cb bufferDeleteCallback{};
    void *bufferDeleterData{};
    scriptCacheLoadCallback_(
        scriptCacheData_,
        scriptSignature.url.c_str(),
        scriptSignature.version,
        runtimeMetadata.runtimeName.c_str(),
        runtimeMetadata.version,
        prepareTag,
        &buffer,
        &bufferSize,
        &bufferDeleteCallback,
        &bufferDeleterData);
    return std::make_shared<V8JsiBuffer>(buffer, bufferSize, bufferDeleteCallback, bufferDeleterData);
  }

  void persistPreparedScript(
      std::shared_ptr<const facebook::jsi::Buffer> preparedScript,
      const facebook::jsi::ScriptSignature &scriptSignature,
      const facebook::jsi::JSRuntimeSignature &runtimeMetadata,
      const char *prepareTag) noexcept override {
    scriptCacheStoreCallback_(
        scriptCacheData_,
        scriptSignature.url.c_str(),
        scriptSignature.version,
        runtimeMetadata.runtimeName.c_str(),
        runtimeMetadata.version,
        prepareTag,
        preparedScript->data(),
        preparedScript->size(),
        [](void * /*data*/, void *deleterData) {
          delete reinterpret_cast<std::shared_ptr<const facebook::jsi::Buffer> *>(deleterData);
        },
        new std::shared_ptr<const facebook::jsi::Buffer>(preparedScript));
  }

 private:
  void *scriptCacheData_{};
  v8_script_cache_load_cb scriptCacheLoadCallback_{};
  v8_script_cache_store_cb scriptCacheStoreCallback_{};
  v8_data_delete_cb scriptCacheDataDeleteCallback_{};
  void *deleterData_{};
};

class ConfigWrapper {
 public:
  v8_status enableDebugger(bool value) {
    enableDebugger_ = value;
    return v8_status::v8_ok;
  }

  v8_status setDebuggerRuntimeName(std::string name) {
    debuggerRuntimeName_ = std::move(name);
    return v8_status::v8_ok;
  }

  v8_status setDebuggerPort(uint16_t port) {
    debuggerPort_ = port;
    return v8_status::v8_ok;
  }

  v8_status setDebuggerBreakOnStart(bool value) {
    debuggerBreakOnStart_ = value;
    return v8_status::v8_ok;
  }

  v8_status enableMultithreading(bool value) {
    enableMultithreading_ = value;
    return v8_status::v8_ok;
  }

  v8_status setTaskRunner(std::shared_ptr<V8TaskRunner> taskRunner) {
    taskRunner_ = std::move(taskRunner);
    return v8_status::v8_ok;
  }

  v8_status setScriptCache(std::shared_ptr<V8ScriptCache> scriptCache) {
    scriptCache_ = std::move(scriptCache);
    return v8_status::v8_ok;
  }

  bool enableDebugger() const {
    return enableDebugger_;
  }

  const std::string &debuggerRuntimeName() const {
    return debuggerRuntimeName_;
  }

  uint16_t debuggerPort() {
    return debuggerPort_;
  }

  bool debuggerBreakOnStart() {
    return debuggerBreakOnStart_;
  }

  bool enableMultithreading() const {
    return enableMultithreading_;
  }

  const std::shared_ptr<V8TaskRunner> &taskRunner() const {
    return taskRunner_;
  }

  const std::shared_ptr<V8ScriptCache> &scriptCache() const {
    return scriptCache_;
  }

 private:
  bool enableDebugger_{};
  std::string debuggerRuntimeName_;
  uint16_t debuggerPort_{};
  bool debuggerBreakOnStart_{};
  bool enableMultithreading_{};
  std::shared_ptr<V8TaskRunner> taskRunner_;
  std::shared_ptr<V8ScriptCache> scriptCache_;
};

class RuntimeWrapper {
 public:
  explicit RuntimeWrapper(const ConfigWrapper &config) {
    //      : v8Runtime_(makeHermesRuntime(config.getRuntimeConfig())), vmRuntime_(getVMRuntime(*v8Runtime_)) {
    // napi_create_v8_env(vmRuntime_, config.enableDebugger(), {}, &env_);

    // if (config.enableDebugger()) {
    //   auto adapter = std::make_unique<HermesExecutorRuntimeAdapter>(v8Runtime_, config.taskRunner());
    //   std::string debuggerRuntimeName = config.debuggerRuntimeName();
    //   if (debuggerRuntimeName.empty()) {
    //     debuggerRuntimeName = "Hermes";
    //   }
    //   v8impl::inspector::chrome::enableDebugging(std::move(adapter), debuggerRuntimeName);
    // }
  }

  ~RuntimeWrapper() {
    // napi_ext_env_unref(env_);
  }

  v8_status getNodeApi(napi_env *env) {
    *env = env_;
    return v8_ok;
  }

 private:
  // ConfigWrapper config_;
  // std::shared_ptr<HermesRuntime> v8Runtime_;
  //::v8::vm::Runtime &vmRuntime_;
  napi_env env_;
};

} // namespace v8impl

V8_API v8_create_runtime(v8_config config, v8_runtime *runtime) {
  V8_CHECK_ARG(runtime);
  *runtime =
      reinterpret_cast<v8_runtime>(new v8impl::RuntimeWrapper(*reinterpret_cast<v8impl::ConfigWrapper *>(config)));
  return v8_ok;
}

V8_API v8_delete_runtime(v8_runtime runtime) {
  V8_CHECK_ARG(runtime);
  delete reinterpret_cast<v8impl::RuntimeWrapper *>(runtime);
  return v8_ok;
}

V8_API v8_get_node_api_env(v8_runtime runtime, napi_env *env) {
  return CHECKED_RUNTIME(runtime)->getNodeApi(env);
}

V8_API v8_create_config(v8_config *config) {
  V8_CHECK_ARG(config);
  *config = reinterpret_cast<v8_config>(new v8impl::ConfigWrapper());
  return v8_ok;
}

V8_API v8_delete_config(v8_config config) {
  V8_CHECK_ARG(config);
  delete reinterpret_cast<v8impl::ConfigWrapper *>(config);
  return v8_ok;
}

V8_API v8_config_enable_debugger(v8_config config, bool value) {
  return CHECKED_CONFIG(config)->enableDebugger(value);
}

V8_API v8_config_set_debugger_runtime_name(v8_config config, const char *name) {
  return CHECKED_CONFIG(config)->setDebuggerRuntimeName(name);
}

V8_API v8_config_set_debugger_port(v8_config config, uint16_t port) {
  return CHECKED_CONFIG(config)->setDebuggerPort(port);
}

V8_API v8_config_set_debugger_break_on_start(v8_config config, bool value) {
  return CHECKED_CONFIG(config)->setDebuggerBreakOnStart(value);
}

V8_API v8_config_enable_multithreading(v8_config config, bool value) {
  return CHECKED_CONFIG(config)->enableMultithreading(value);
}

V8_API v8_config_set_task_runner(
    v8_config config,
    void *task_runner_data,
    v8_task_runner_post_task_cb task_runner_post_task_cb,
    v8_data_delete_cb task_runner_data_delete_cb,
    void *deleter_data) {
  return CHECKED_CONFIG(config)->setTaskRunner(std::make_shared<v8impl::V8TaskRunner>(
      task_runner_data, task_runner_post_task_cb, task_runner_data_delete_cb, deleter_data));
}

V8_API v8_config_set_script_cache(
    v8_config config,
    void *script_cache_data,
    v8_script_cache_load_cb script_cache_load_cb,
    v8_script_cache_store_cb script_cache_store_cb,
    v8_data_delete_cb script_cache_data_delete_cb,
    void *deleter_data) {
  return CHECKED_CONFIG(config)->setScriptCache(std::make_shared<v8impl::V8ScriptCache>(
      script_cache_data, script_cache_load_cb, script_cache_store_cb, script_cache_data_delete_cb, deleter_data));
}

// napi_status napi_ext_create_env(napi_ext_env_settings *settings, napi_env *env) {
//   v8runtime::V8RuntimeArgs args;

//   args.flags.trackGCObjectStats = settings->flags.track_gc_object_stats;
//   args.flags.enableJitTracing = settings->flags.enable_jit_tracing;
//   args.flags.enableMessageTracing = settings->flags.enable_message_tracing;
//   args.flags.enableGCTracing = settings->flags.enable_gc_tracing;
//   args.flags.enableInspector = settings->flags.enable_inspector;
//   args.flags.waitForDebugger = settings->flags.wait_for_debugger;
//   args.flags.enableGCApi = settings->flags.enable_gc_api;
//   args.flags.ignoreUnhandledPromises = settings->flags.ignore_unhandled_promises;
//   args.flags.enableSystemInstrumentation = settings->flags.enable_system_instrumentation;
//   args.flags.sparkplug = settings->flags.sparkplug;
//   args.flags.predictable = settings->flags.predictable;
//   args.flags.optimize_for_size = settings->flags.optimize_for_size;
//   args.flags.always_compact = settings->flags.always_compact;
//   args.flags.jitless = settings->flags.jitless;
//   args.flags.lite_mode = settings->flags.lite_mode;
//   args.flags.thread_pool_size = settings->flags.thread_pool_size;
//   args.flags.enableMultiThread = settings->flags.enable_multi_thread;

//   args.foreground_task_runner =
//       std::shared_ptr<V8TaskRunner>(reinterpret_cast<V8TaskRunner *>(settings->foreground_task_runner));

//   if (settings->script_cache) {
//     args.preparedScriptStore = std::make_unique<NodeApiPreparedScriptStore>(settings->script_cache);
//   }

//   auto runtime = std::make_unique<v8runtime::V8Runtime>(std::move(args));

//   auto context = v8impl::PersistentToLocal::Strong(runtime->GetContext());
//   *env = new napi_env__(context, settings->flags.enable_multi_thread);

//   // Let the runtime exists. It can be accessed from the Context.
//   new v8impl::V8RuntimeHolder(*env, runtime.release());

//   return napi_status::napi_ok;
// }

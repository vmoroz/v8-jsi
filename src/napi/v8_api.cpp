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

#define CHECKED_RUNTIME(runtime) (runtime == nullptr) ? v8_error : reinterpret_cast<v8impl::RuntimeWrapper *>(runtime)

#define CHECKED_CONFIG(config) (config == nullptr) ? v8_error : reinterpret_cast<v8impl::ConfigWrapper *>(config)

#define CHECK_ARG(arg)  \
  if (arg == nullptr) { \
    return v8_error;    \
  }

napi_status napi_create_v8_env(
    ::v8::vm::Runtime &runtime,
    bool isInspectable,
    const ::v8::vm::RuntimeConfig &runtimeConfig,
    napi_env *env);

napi_status napi_ext_env_unref(napi_env env);

namespace v8impl {

class Task {
 public:
  virtual void invoke() noexcept = 0;

  static void run(void *task) {
    reinterpret_cast<Task *>(task)->invoke();
  }

  static void deleteTask(void *task, void * /*deleterData*/) {
    delete reinterpret_cast<Task *>(task);
  }
};

template <typename TLambda>
class LambdaTask : public Task {
 public:
  LambdaTask(TLambda &&lambda) : lambda_(std::move(lambda)) {}

  void invoke() noexcept override {
    lambda_();
  }

 private:
  TLambda lambda_;
};

struct V8TaskRunner : v8runtime::JSITaskRunner {
  V8TaskRunner(
      void *task_runner_data,
      v8_task_runner_post_task_cb task_runner_post_task_cb,
      v8_task_runner_release_cb task_runner_release_cb)
      : task_runner_data_(task_runner_data),
        task_runner_post_task_cb_(task_runner_post_task_cb),
        task_runner_release_cb_(task_runner_release_cb) {}

  ~V8TaskRunner() override {
    task_runner_release_cb_(task_runner_data_);
  }

  void postTask(std::unique_ptr<v8runtime::JSITask> task) override {
    task_runner_post_task_cb_(
        task_runner_data_,
        static_cast<void *>(task.release()),
        [](void *task_data) { static_cast<v8runtime::JSITask *>(task_data)->run(); },
        [](void *task_data) { delete static_cast<v8runtime::JSITask *>(task_data); });
  }

 private:
  void *task_runner_data_;
  v8_task_runner_post_task_cb task_runner_post_task_cb_;
  v8_task_runner_release_cb task_runner_release_cb_;
};

class TaskRunner : public v8runtime::JSITaskRunner {
 public:
  TaskRunner(
      void *taskRunnerData,
      v8_task_runner_post_task_cb postTaskCallback,
      v8_data_delete_cb deleteCallback,
      void *deleterData)
      : taskRunnerData_(taskRunnerData),
        postTaskCallback_(postTaskCallback),
        deleteCallback_(deleteCallback),
        deleterData_(deleterData) {}

  ~TaskRunner() {
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

class ScriptBuffer {
 public:
  ScriptBuffer(const uint8_t *data, size_t size, v8_data_delete_cb deleteCallback, void *deleterData)
      : data_(data), size_(size), deleteCallback_(deleteCallback), deleterData_(deleterData) {}

  ~ScriptBuffer() {
    if (deleteCallback_ != nullptr) {
      deleteCallback_(const_cast<uint8_t *>(data_), deleterData_);
    }
  }

  const uint8_t *data() {
    return data_;
  }

  size_t size() {
    return size_;
  }

  static void deleteBuffer(void * /*data*/, void *scriptBuffer) {
    delete reinterpret_cast<ScriptBuffer *>(scriptBuffer);
  }

 private:
  const uint8_t *data_{};
  size_t size_{};
  v8_data_delete_cb deleteCallback_{};
  void *deleterData_{};
};

class ScriptCache {
 public:
  ScriptCache(
      void *data,
      v8_script_cache_load_cb loadCallback,
      v8_script_cache_store_cb storeCallback,
      v8_data_delete_cb deleteCallback,
      void *deleterData)
      : data_(data),
        loadCallback_(loadCallback),
        storeCallback_(storeCallback),
        deleteCallback_(deleteCallback),
        deleterData_(deleterData) {}

  ~ScriptCache() {
    if (deleteCallback_ != nullptr) {
      deleteCallback_(data_, deleterData_);
    }
  }

  std::unique_ptr<ScriptBuffer> load(v8_script_cache_metadata *metadata) {
    const uint8_t *buffer{};
    size_t size{};
    v8_data_delete_cb deleteCallback{};
    void *deleterData{};
    loadCallback_(this, metadata, &buffer, &size, &deleteCallback, &deleterData);
    return std::make_unique<ScriptBuffer>(buffer, size, deleteCallback, deleterData);
  }

  void store(v8_script_cache_metadata *metadata, std::unique_ptr<ScriptBuffer> scriptBuffer) {
    storeCallback_(
        this, metadata, scriptBuffer->data(), scriptBuffer->size(), &ScriptBuffer::deleteBuffer, scriptBuffer.get());
    scriptBuffer.release();
  }

 private:
  void *data_;
  v8_script_cache_load_cb loadCallback_;
  v8_script_cache_store_cb storeCallback_;
  v8_data_delete_cb deleteCallback_;
  void *deleterData_;
};

struct V8PreparedScriptStore : facebook::jsi::PreparedScriptStore {
  V8PreparedScriptStore(
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

  ~V8PreparedScriptStore() override {
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
    return V8JsiBuffer::CreateJsiBuffer(buffer, bufferSize, bufferDeleteCallback, bufferDeleterData);
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

  v8_status setTaskRunner(std::unique_ptr<TaskRunner> taskRunner) {
    taskRunner_ = std::move(taskRunner);
    return v8_status::v8_ok;
  }

  v8_status setScriptCache(std::unique_ptr<ScriptCache> scriptCache) {
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

  std::shared_ptr<TaskRunner> taskRunner() const {
    return taskRunner_;
  }

  ScriptCache *scriptCache() {
    return scriptCache_.get();
  }

  ::v8::vm::RuntimeConfig getRuntimeConfig() const {
    ::v8::vm::RuntimeConfig::Builder config;
    if (enableDefaultCrashHandler_) {
      auto crashManager = std::make_shared<CrashManagerImpl>();
      config.withCrashMgr(crashManager);
    }
    return config.build();
  }

 private:
  bool enableDebugger_{};
  std::string debuggerRuntimeName_;
  uint16_t debuggerPort_{};
  bool debuggerBreakOnStart_{};
  bool enableMultithreading_{};
  std::shared_ptr<TaskRunner> taskRunner_;
  std::shared_ptr<ScriptCache> scriptCache_;
};

class RuntimeWrapper {
 public:
  explicit RuntimeWrapper(const ConfigWrapper &config)
      : v8Runtime_(makeHermesRuntime(config.getRuntimeConfig())), vmRuntime_(getVMRuntime(*v8Runtime_)) {
    napi_create_v8_env(vmRuntime_, config.enableDebugger(), {}, &env_);

    if (config.enableDebugger()) {
      auto adapter = std::make_unique<HermesExecutorRuntimeAdapter>(v8Runtime_, config.taskRunner());
      std::string debuggerRuntimeName = config.debuggerRuntimeName();
      if (debuggerRuntimeName.empty()) {
        debuggerRuntimeName = "Hermes";
      }
      v8impl::inspector::chrome::enableDebugging(std::move(adapter), debuggerRuntimeName);
    }
  }

  ~RuntimeWrapper() {
    napi_ext_env_unref(env_);
  }

  v8_status getNodeApi(napi_env *env) {
    *env = env_;
    return v8_ok;
  }

 private:
  ConfigWrapper config_;
  std::shared_ptr<HermesRuntime> v8Runtime_;
  ::v8::vm::Runtime &vmRuntime_;
  napi_env env_;
};

} // namespace v8impl

V8_API v8_create_runtime(v8_config config, v8_runtime *runtime) {
  CHECK_ARG(runtime);
  *runtime =
      reinterpret_cast<v8_runtime>(new v8impl::RuntimeWrapper(*reinterpret_cast<v8impl::ConfigWrapper *>(config)));
  return v8_ok;
}

V8_API v8_delete_runtime(v8_runtime runtime) {
  CHECK_ARG(runtime);
  delete reinterpret_cast<v8impl::RuntimeWrapper *>(runtime);
  return v8_ok;
}

V8_API v8_get_node_api_env(v8_runtime runtime, napi_env *env) {
  return CHECKED_RUNTIME(runtime)->getNodeApi(env);
}

V8_API v8_create_config(v8_config *config) {
  CHECK_ARG(config);
  *config = reinterpret_cast<v8_config>(new v8impl::ConfigWrapper());
  return v8_ok;
}

V8_API v8_delete_config(v8_config config) {
  CHECK_ARG(config);
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
  return CHECKED_CONFIG(config)->setTaskRunner(std::make_unique<v8impl::TaskRunner>(
      task_runner_data, task_runner_post_task_cb, task_runner_data_delete_cb, deleter_data));
}

V8_API v8_config_set_script_cache(
    v8_config config,
    void *script_cache_data,
    v8_script_cache_load_cb script_cache_load_cb,
    v8_script_cache_store_cb script_cache_store_cb,
    v8_data_delete_cb script_cache_data_delete_cb,
    void *deleter_data) {
  return CHECKED_CONFIG(config)->setScriptCache(std::make_unique<v8impl::ScriptCache>(
      script_cache_data, script_cache_load_cb, script_cache_store_cb, script_cache_data_delete_cb, deleter_data));
}

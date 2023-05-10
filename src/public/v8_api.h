// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#ifndef V8_API_H_
#define V8_API_H_

#include <js_native_ext_api.h>

#define V8_API NAPI_EXTERN v8_status NAPI_CDECL

EXTERN_C_START

enum v8_status {
  v8_ok,
  v8_error,
};

typedef struct v8_runtime_s *v8_runtime;
typedef struct v8_config_s *v8_config;

typedef void(NAPI_CDECL *v8_data_delete_cb)(void *data, void *deleter_data);

//=============================================================================
// hermes_runtime
//=============================================================================

V8_API v8_create_runtime(v8_config config, v8_runtime *runtime);
V8_API v8_delete_runtime(v8_runtime runtime);
V8_API v8_get_node_api_env(v8_runtime runtime, napi_env *env);

//=============================================================================
// v8_config
//=============================================================================

V8_API v8_create_config(v8_config *config);
V8_API v8_delete_config(v8_config config);

V8_API v8_config_enable_debugger(v8_config config, bool value);
V8_API v8_config_set_debugger_runtime_name(v8_config config, const char *name);
V8_API v8_config_set_debugger_port(v8_config config, uint16_t port);
V8_API v8_config_set_debugger_break_on_start(v8_config config, bool value);

V8_API v8_config_enable_multithreading(v8_config config, bool value);

//=============================================================================
// v8_config task runner
//=============================================================================

// A callback to run task
typedef void(NAPI_CDECL *v8_task_run_cb)(void *task_data);

// A callback to post task to the task runner
typedef void(NAPI_CDECL *v8_task_runner_post_task_cb)(
    void *task_runner_data,
    void *task_data,
    v8_task_run_cb task_run_cb,
    v8_data_delete_cb task_data_delete_cb,
    void *deleter_data);

V8_API v8_config_set_task_runner(
    v8_config config,
    void *task_runner_data,
    v8_task_runner_post_task_cb task_runner_post_task_cb,
    v8_data_delete_cb task_runner_data_delete_cb,
    void *deleter_data);

//=============================================================================
// v8_config script cache
//=============================================================================

typedef void(NAPI_CDECL *v8_script_cache_load_cb)(
    void *script_cache_data,
    const char *source_url,
    uint64_t source_hash,
    const char *runtime_name,
    uint64_t runtime_version,
    const char *cache_tag,
    const uint8_t **buffer,
    size_t *buffer_size,
    v8_data_delete_cb *buffer_delete_cb,
    void **deleter_data);

typedef void(NAPI_CDECL *v8_script_cache_store_cb)(
    void *script_cache_data,
    const char *source_url,
    uint64_t source_hash,
    const char *runtime_name,
    uint64_t runtime_version,
    const char *cache_tag,
    const uint8_t *buffer,
    size_t buffer_size,
    v8_data_delete_cb buffer_delete_cb,
    void *deleter_data);

V8_API v8_config_set_script_cache(
    v8_config config,
    void *script_cache_data,
    v8_script_cache_load_cb script_cache_load_cb,
    v8_script_cache_store_cb script_cache_store_cb,
    v8_data_delete_cb script_cache_data_delete_cb,
    void *deleter_data);

EXTERN_C_END

#endif // V8_API_H_

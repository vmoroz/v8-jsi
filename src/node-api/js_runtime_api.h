// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#ifndef SRC_JS_NATIVE_EXT_API_H_
#define SRC_JS_NATIVE_EXT_API_H_

#include "js_native_api.h"

//
// N-API extensions required for JavaScript engine hosting.
//
// It is a very early version of the APIs which we consider to be experimental.
// These APIs are not stable yet and are subject to change while we continue
// their development. After some time we will stabilize the APIs and make them
// "officially stable".
//

#define NAPI_API NAPI_EXTERN napi_status NAPI_CDECL

EXTERN_C_START

typedef void(NAPI_CDECL *jsr_data_delete_cb)(void *data, void *deleter_data);
typedef napi_status(NAPI_CDECL *jsr_invoke_in_context_cb)(void *data);

// Provides a hint to run garbage collection.
// It is typically used for unit tests.
NAPI_API jsr_collect_garbage(napi_env env);

// Checks if the environment has an unhandled promise rejection.
NAPI_API jsr_has_unhandled_promise_rejection(napi_env env, bool *result);

// Gets and clears the last unhandled promise rejection.
NAPI_API jsr_get_and_clear_last_unhandled_promise_rejection(napi_env env, napi_value *result);

// To implement JSI description()
NAPI_API jsr_get_description(napi_env env, char *buf, size_t bufsize, size_t *result);

// To implement JSI drainMicrotasks()
NAPI_API
jsr_drain_microtasks(napi_env env, int32_t max_count_hint, bool *result);

// To implement JSI isInspectable()
NAPI_API jsr_is_inspectable(napi_env env, bool *result);

// Storage for the engine-specific napi_env scope data.
// The struct should be created on in the call stack and its pointer passed to
// jsr_open_env_scope and jsr_close_env_scope methods.
typedef struct jsr_env_scope {
  void *placeholder[12];
} jsr_env_scope;

// Opens the napi_env scope in the current thread.
// Calling N-API functions without the opened scope may cause a failure.
// The scope must be closed by the jsr_close_env_scope call.
NAPI_API jsr_open_env_scope(napi_env env, jsr_env_scope *scope);

// Closes the napi_env in the current thread. It must match to the jsr_open_env_scope call.
NAPI_API jsr_close_env_scope(napi_env env, jsr_env_scope *scope);

//=============================================================================
// Script preparing and running.
//
// Script is usually converted to byte code, or in other words - prepared - for
// execution. Then, we can run the prepared script.
//=============================================================================

typedef struct jsr_prepared_script_s *jsr_prepared_script;

// Run script with source URL.
NAPI_API jsr_run_script(napi_env env, napi_value source, const char *source_url, napi_value *result);

// Prepare the script for running.
NAPI_API jsr_create_prepared_script(
    napi_env env,
    const uint8_t *script_data,
    size_t script_length,
    jsr_data_delete_cb script_delete_cb,
    void *deleter_data,
    const char *source_url,
    jsr_prepared_script *result);

// Delete the prepared script.
NAPI_API jsr_delete_prepared_script(napi_env env, jsr_prepared_script prepared_script);

// Run the prepared script.
NAPI_API jsr_prepared_script_run(napi_env env, jsr_prepared_script prepared_script, napi_value *result);

EXTERN_C_END

#endif // !SRC_JS_NATIVE_EXT_API_H_

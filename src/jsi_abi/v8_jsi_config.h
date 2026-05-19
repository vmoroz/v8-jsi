/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT license.
 *
 * V8-specific configuration for the JSI ABI factory.
 *
 * Mirrors the established jsr_config / v8_api split next door (see
 * src/node-api/js_runtime_api.h and src/node-api/v8_api.h):
 *   - jsi_config (declared in jsi_abi.h) is the engine-agnostic opaque type.
 *   - The v8_jsi_create_config / v8_jsi_config_set_* functions in this header
 *     are V8-specific. A future Hermes ABI port can add hermes_jsi_config_set_*
 *     setters against the same opaque jsi_config type.
 *
 * The DLL owns the config layout. Consumers only see opaque pointers and
 * pure-C setter functions. Adding a new V8 flag in the future is a new
 * setter — no struct version to bump, no field-order contract to preserve.
 *
 * Pure C header. Safe to include from C or C++.
 */

#ifndef V8_JSI_CONFIG_H
#define V8_JSI_CONFIG_H

#include "jsi_abi/jsi_abi.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Lifetime
 *============================================================================*/

/* Allocate a new V8 jsi_config with default values. */
JSI_API jsi_config JSI_CDECL v8_jsi_create_config(void);

/* Free a config previously returned by v8_jsi_create_config.
 * NULL is a no-op. */
JSI_API void JSI_CDECL v8_jsi_delete_config(jsi_config config);

/*============================================================================
 * Heap / threading
 *============================================================================*/

JSI_API void JSI_CDECL
v8_jsi_config_set_initial_heap_size(jsi_config config, size_t bytes);

JSI_API void JSI_CDECL
v8_jsi_config_set_maximum_heap_size(jsi_config config, size_t bytes);

/* Cap the V8 worker thread pool. 0 means default
 * (V8 picks min(num_cores - 1, 16)). */
JSI_API void JSI_CDECL
v8_jsi_config_set_thread_pool_size(jsi_config config, uint8_t size);

/*============================================================================
 * Inspector / debugger
 *============================================================================*/

/* Name shown in the inspector UI to distinguish parallel runtimes. */
JSI_API void JSI_CDECL
v8_jsi_config_set_debugger_runtime_name(jsi_config config, const char *name);

JSI_API void JSI_CDECL
v8_jsi_config_enable_inspector(jsi_config config, bool value);

JSI_API void JSI_CDECL
v8_jsi_config_set_inspector_port(jsi_config config, uint16_t port);

JSI_API void JSI_CDECL
v8_jsi_config_set_inspector_break_on_start(jsi_config config, bool value);

/*============================================================================
 * Microtasks / promises
 *============================================================================*/

/* If true, the runtime uses MicrotasksPolicy::kExplicit and the host must
 * drain microtasks itself. Default is false (V8 default policy). */
JSI_API void JSI_CDECL
v8_jsi_config_set_explicit_microtask_policy(jsi_config config, bool value);

/* If true, the PromiseRejectCallback that tracks unhandled rejections is
 * NOT installed. Default is false (track rejections). */
JSI_API void JSI_CDECL
v8_jsi_config_set_ignore_unhandled_promises(jsi_config config, bool value);

/*============================================================================
 * GC
 *============================================================================*/

/* Expose the global gc() function (passes --expose_gc to V8). */
JSI_API void JSI_CDECL
v8_jsi_config_enable_gc_api(jsi_config config, bool value);

/*============================================================================
 * Concurrency
 *============================================================================*/

/* Wrap isolate access in v8::Locker for multi-threaded use. */
JSI_API void JSI_CDECL
v8_jsi_config_enable_multi_thread(jsi_config config, bool value);

/*============================================================================
 * Tracing
 *============================================================================*/

JSI_API void JSI_CDECL
v8_jsi_config_enable_jit_tracing(jsi_config config, bool value);

JSI_API void JSI_CDECL
v8_jsi_config_enable_message_tracing(jsi_config config, bool value);

JSI_API void JSI_CDECL
v8_jsi_config_enable_gc_tracing(jsi_config config, bool value);

/* V8 ETW provider GUID 57277741-3638-4A4B-BDBA-0AC6E45DA56C
 * (passes --enable-system-instrumentation to V8). */
JSI_API void JSI_CDECL
v8_jsi_config_enable_system_instrumentation(jsi_config config, bool value);

/*============================================================================
 * V8 experimental flags (memory / perf trade-offs)
 *
 * NOTE: V8 command-line flags are process-global state. They are read by V8
 * only during the FIRST platform initialization in the process. Subsequent
 * runtimes inherit whatever flags were set on first init — these setters on
 * later configs are silently ignored by V8 itself. This is V8's design, not
 * a config-layer constraint.
 *============================================================================*/

/* https://v8.dev/blog/sparkplug — non-optimizing JIT tier. */
JSI_API void JSI_CDECL
v8_jsi_config_set_sparkplug(jsi_config config, bool value);

/* Reduce thread count at a CPU cost — used for predictable benchmarks. */
JSI_API void JSI_CDECL
v8_jsi_config_set_predictable(jsi_config config, bool value);

/* Bias optimizations toward smaller memory footprint over speed. */
JSI_API void JSI_CDECL
v8_jsi_config_set_optimize_for_size(jsi_config config, bool value);

/* Compact the heap on every full GC. */
JSI_API void JSI_CDECL
v8_jsi_config_set_always_compact(jsi_config config, bool value);

/* Disable the V8 JIT entirely. */
JSI_API void JSI_CDECL
v8_jsi_config_set_jitless(jsi_config config, bool value);

/* Trade performance for memory savings (umbrella for several flags). */
JSI_API void JSI_CDECL
v8_jsi_config_set_lite_mode(jsi_config config, bool value);

/*============================================================================
 * Script cache (prepared-script storage)
 *
 * Mirrors jsr_config_set_script_cache in src/node-api/js_runtime_api.h
 * byte-for-byte. Consumers register a pair of C callbacks that load and store
 * compiled bytecode on behalf of jsi_prepare_javascript. NULL callbacks mean
 * "no cache" — prepare always falls back to fresh V8 compilation.
 *
 * Lifetime: when the setter is called the config takes ownership of
 * script_cache_data; on runtime construction the runtime takes over (the
 * config's pointer is cleared so it isn't deleted twice). When the runtime
 * is destroyed it invokes script_cache_data_delete_cb(script_cache_data,
 * deleter_data) exactly once. This matches jsr_config semantics, so the
 * consumer can free the config immediately after v8_create_runtime returns.
 *============================================================================*/

typedef void (JSI_CDECL *v8_jsi_script_cache_load_cb)(
    void *script_cache_data,
    const char *source_url,
    uint64_t source_hash,
    const char *runtime_name,
    uint64_t runtime_version,
    const char *cache_tag,
    const uint8_t **buffer,                /* out */
    size_t *buffer_size,                   /* out */
    jsi_data_delete_cb *buffer_delete_cb,  /* out */
    void **deleter_data);                  /* out */

typedef void (JSI_CDECL *v8_jsi_script_cache_store_cb)(
    void *script_cache_data,
    const char *source_url,
    uint64_t source_hash,
    const char *runtime_name,
    uint64_t runtime_version,
    const char *cache_tag,
    const uint8_t *buffer,
    size_t buffer_size,
    jsi_data_delete_cb buffer_delete_cb,
    void *deleter_data);

JSI_API void JSI_CDECL v8_jsi_config_set_script_cache(
    jsi_config config,
    void *script_cache_data,
    v8_jsi_script_cache_load_cb load_cb,
    v8_jsi_script_cache_store_cb store_cb,
    jsi_data_delete_cb script_cache_data_delete_cb,
    void *deleter_data);

/*============================================================================
 * Task runner (foreground-thread dispatch)
 *
 * Mirrors jsr_config_set_task_runner in src/node-api/js_runtime_api.h
 * byte-for-byte. The foreground task runner is how the inspector dispatches
 * incoming debugger messages onto the JS thread (see inspector_agent.cpp).
 * NULL post_task_cb means "no task runner" — V8 falls back to its default
 * platform runner.
 *
 * Lifetime: when the setter is called the config takes ownership of
 * task_runner_data; on runtime construction the runtime takes over (the
 * config's pointer is cleared so it isn't deleted twice). The runtime holds
 * the task runner inside the V8 IsolateData via a shared_ptr; when the
 * runtime is destroyed the shared_ptr drops, the internal adapter destructs,
 * and task_runner_data_delete_cb(task_runner_data, deleter_data) runs
 * exactly once. This matches jsr_config semantics, so the consumer can free
 * the config immediately after v8_create_runtime returns.
 *============================================================================*/

typedef void (JSI_CDECL *v8_jsi_task_run_cb)(void *task_data);

typedef void (JSI_CDECL *v8_jsi_task_runner_post_task_cb)(
    void *task_runner_data,
    void *task_data,
    v8_jsi_task_run_cb task_run_cb,
    jsi_data_delete_cb task_data_delete_cb,
    void *deleter_data);

JSI_API void JSI_CDECL v8_jsi_config_set_task_runner(
    jsi_config config,
    void *task_runner_data,
    v8_jsi_task_runner_post_task_cb post_task_cb,
    jsi_data_delete_cb task_runner_data_delete_cb,
    void *deleter_data);

/*============================================================================
 * Inspector control
 *
 * The inspector is wired into v8_create_runtime when enable_inspector is true
 * on the config (see v8_jsi_config_enable_inspector). v8_open_inspector starts
 * a previously-quiet inspector on a runtime that was created with the flag
 * set. Mirrors the legacy openInspector(jsi::Runtime &) entry point but takes
 * an opaque jsi_runtime so it does not depend on a JSI Runtime layout (no CRT
 * boundary leaks).
 *
 * Pure C export — does not go through query_interface.
 *============================================================================*/

JSI_API void JSI_CDECL v8_open_inspector(jsi_runtime *runtime);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* V8_JSI_CONFIG_H */

/*============================================================================
 * Note: v8_attach_node_api lives in src/jsi_abi/v8_node_api_attach.h. It is
 * kept out of this header so consumers of the v8 jsi config do not have to
 * pull in the Node-API headers (napi_env / napi_status types).
 *============================================================================*/

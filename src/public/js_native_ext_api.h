#pragma once

#define NAPI_EXPERIMENTAL
#include "js_native_api.h"

EXTERN_C_START

typedef enum {
  napi_ext_env_attribute_none = 0x00000000,
  napi_ext_env_attribute_disable_gc_api = 0x00000001,
} napi_ext_env_attributes;

typedef struct napi_env_scope__ *napi_env_scope;

NAPI_EXTERN napi_status
napi_ext_create_env(napi_ext_env_attributes attributes, napi_env *env);

NAPI_EXTERN napi_status napi_ext_delete_env(napi_env env);

NAPI_EXTERN napi_status
napi_ext_open_env_scope(napi_env env, napi_env_scope *result);

NAPI_EXTERN napi_status napi_ext_close_env_scope(napi_env env, napi_env_scope scope);

NAPI_EXTERN napi_status napi_ext_run_script(
    napi_env env,
    napi_value script,
    const char *source_url,
    napi_value *result);

NAPI_EXTERN napi_status napi_ext_collect_garbage(napi_env env);

// TODO: [vmoroz] redesign the unhandled promise handing
NAPI_EXTERN napi_status napi_ext_get_unhandled_promise_rejections(
    napi_env env,
    napi_value *buf,
    size_t bufsize,
    size_t startAt,
    size_t *result);

NAPI_EXTERN napi_status
napi_ext_clean_unhandled_promise_rejections(napi_env env, size_t *result);

// TODO: [vmoroz] add APIs for byte code
// TODO: [vmoroz] add APIs for scope
// TODO: [vmoroz] add APIs for perf/memory use

EXTERN_C_END
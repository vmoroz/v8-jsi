#pragma once

#include "js_native_api.h"

#define JS_API extern "C" NAPI_EXTERN napi_status

JS_API js_run_script(
    napi_env env,
    napi_value script,
    const char *source_url,
    napi_value *result);

JS_API napi_host_get_unhandled_promise_rejections(
    napi_env env,
    napi_value *buf,
    size_t bufsize,
    size_t startAt,
    size_t *result);

JS_API napi_host_clean_unhandled_promise_rejections(
    napi_env env,
    size_t *result);

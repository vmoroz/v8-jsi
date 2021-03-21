#pragma once

#include "js_native_api.h"

#define JS_API extern "C" NAPI_EXTERN napi_status

JS_API js_run_script(
    napi_env env,
    napi_value script,
    const char *source_url,
    napi_value *result);
#pragma once

#include "js_native_api.h"

NAPI_EXTERN bool __cdecl napi_test_enable_gc_api(bool enableGC);
NAPI_EXTERN void __cdecl napi_test_run_gc(napi_env env);
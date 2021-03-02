#pragma once

#include "js_native_api.h"

NAPI_EXTERN napi_env __cdecl v8_create_env();
NAPI_EXTERN void __cdecl v8_delete_env(napi_env env);
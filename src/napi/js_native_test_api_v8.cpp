#include "js_native_api_v8.h"
#include "js_native_test_api.h"
#include "src/flags/flags.h"
#include "v8.h"

bool __cdecl napi_test_enable_gc_api(bool enableGC) {
  bool previousValue = v8::internal::FLAG_expose_gc;
  v8::internal::FLAG_expose_gc = enableGC;
  return previousValue;
}

void __cdecl napi_test_run_gc(napi_env env) {
  env->isolate->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
}
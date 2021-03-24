// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_function_init
#include "js-native-api/test_function/test.js.h"
#include "js-native-api/test_function/test_function.c"

using namespace napitest;

TEST_P(NapiTestBase, test_function) {
  ScopedExposeGC exposeGC;

  napi_value global{};
  napi_get_global(env, &global);
  napi_set_named_property(env, global, "global", global);

  napi_value common = GetModule("../../common");
  napi_value afterGCRun{};
  napi_get_named_property(env, common, "afterGCRun", &afterGCRun);

  auto runGCCallback = [](napi_env env, napi_callback_info info) -> napi_value {
    napi_value afterGCRun {};
    napi_get_cb_info(
        env, info, nullptr, nullptr, nullptr, (void **)&afterGCRun);
    napi_test_run_gc(env);
    napi_value undefined{};
    napi_get_undefined(env, &undefined);

    napi_call_function(env, undefined, afterGCRun, 0, nullptr, nullptr);

    return undefined;
  };
  napi_value runGC{};
  napi_create_function(
      env, "gc", NAPI_AUTO_LENGTH, runGCCallback, afterGCRun, &runGC);
  napi_set_named_property(env, global, "gc", runGC);

  AddNativeModule(
      "./build/x86/test_function",
      [](napi_env env, napi_value exports) { Init(env, exports); });

  RunTestScript(test_function_test_js);
}

INSTANTIATE_TEST_SUITE_P(
    NapiEnv,
    NapiTestBase,
    ::testing::ValuesIn(NapiEnvProviders()));

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_general_init
#include "js-native-api/test_general/test.js.h"
#include "js-native-api/test_general/testEnvCleanup.js.h"
#include "js-native-api/test_general/testFinalizer.js.h"
#include "js-native-api/test_general/testGlobals.js.h"
#include "js-native-api/test_general/testInstanceOf.js.h"
#include "js-native-api/test_general/testNapiRun.js.h"
#include "js-native-api/test_general/testNapiStatus.js.h"
#include "js-native-api/test_general/test_general.c"

using namespace napitest;

// TODO: [vmoroz] make it work
// TEST_P(NapiTestBase, test_general) {
// auto testContext = NapiTestContext(this, env);
//   AddNativeModule(
//       "./build/x86/test_general",
//       [](napi_env env, napi_value exports) { return Init(env, exports); });
//   RunTestScript(test_general_test_js);
// }

TEST_P(NapiTestBase, test_general_NapiStatus) {
  auto testContext = NapiTestContext(this, env);
  AddNativeModule(
      "./build/x86/test_general",
      [](napi_env env, napi_value exports) { return Init(env, exports); });
  RunTestScript(test_general_testNapiStatus_js);
}

TEST_P(NapiTestBase, test_general_NapiRun) {
  auto testContext = NapiTestContext(this, env);
  AddNativeModule(
      "./build/x86/test_general",
      [](napi_env env, napi_value exports) { return Init(env, exports); });
  RunTestScript(test_general_testNapiRun_js);
}

// TODO: [vmoroz] make it work
// TEST_P(NapiTestBase, test_general_InstanceOf) {
// auto testContext = NapiTestContext(this, env);
//   AddNativeModule(
//       "./build/x86/test_general",
//       [](napi_env env, napi_value exports) { return Init(env, exports); });
//   RunTestScript(test_general_testInstanceOf_js);
// }

TEST_P(NapiTestBase, test_general_Globals) {
  auto testContext = NapiTestContext(this, env);
  AddNativeModule(
      "./build/x86/test_general",
      [](napi_env env, napi_value exports) { return Init(env, exports); });
  RunTestScript(test_general_testGlobals_js);
}

// TODO: [vmoroz] make it work
// TEST_P(NapiTestBase, test_general_Finalizer) {
// auto testContext = NapiTestContext(this, env);
//   AddNativeModule(
//       "./build/x86/test_general",
//       [](napi_env env, napi_value exports) { return Init(env, exports); });
//   RunTestScript(test_general_testFinalizer_js);
// }

// TODO: [vmoroz] make it work
// TEST_P(NapiTestBase, test_general_EnvCleanup) {
// auto testContext = NapiTestContext(this, env);
//   AddNativeModule(
//       "./build/x86/test_general",
//       [](napi_env env, napi_value exports) { return Init(env, exports); });
//   RunTestScript(test_general_testEnvCleanup_js);
// }

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_general_init
#include "js-native-api/test_general/testNapiStatus.js.h"
#include "js-native-api/test_general/testNapiRun.js.h"
#include "js-native-api/test_general/testInstanceOf.js.h"
#include "js-native-api/test_general/testGlobals.js.h"
#include "js-native-api/test_general/testFinalizer.js.h"
#include "js-native-api/test_general/testEnvCleanup.js.h"
#include "js-native-api/test_general/test.js.h"
#include "js-native-api/test_general/test_general.c"

using namespace napitest;

TEST_P(NapiTestBase, test_general) {
  AddNativeModule(
      "./build/x86/test_general",
      [](napi_env env, napi_value exports) { return Init(env, exports); });

  RunTestScript(test_general_test_js);
}

TEST_P(NapiTestBase, test_general_NapiStatus) {
  AddNativeModule(
      "./build/x86/test_general",
      [](napi_env env, napi_value exports) { return Init(env, exports); });

  RunTestScript(test_general_testNapiStatus_js);
}


TEST_P(NapiTestBase, test_general_NapiRun) {
  AddNativeModule(
      "./build/x86/test_general",
      [](napi_env env, napi_value exports) { return Init(env, exports); });

  RunTestScript(test_general_testNapiRun_js);
}

//TODO: [vmoroz] make it work
// TEST_P(NapiTestBase, test_general_InstanceOf) {
//   AddNativeModule(
//       "./build/x86/test_general",
//       [](napi_env env, napi_value exports) { return Init(env, exports); });

//   RunTestScript(test_general_testInstanceOf_js);
// }

TEST_P(NapiTestBase, test_general_Globals) {
  AddNativeModule(
      "./build/x86/test_general",
      [](napi_env env, napi_value exports) { return Init(env, exports); });

  RunTestScript(test_general_testGlobals_js);
}

//TODO: [vmoroz] make it work
// TEST_P(NapiTestBase, test_general_Finalizer) {
//   AddNativeModule(
//       "./build/x86/test_general",
//       [](napi_env env, napi_value exports) { return Init(env, exports); });

//   RunTestScript(test_general_testFinalizer_js);
// }

//TODO: [vmoroz] make it work
// TEST_P(NapiTestBase, test_general_EnvCleanup) {
//   AddNativeModule(
//       "./build/x86/test_general",
//       [](napi_env env, napi_value exports) { return Init(env, exports); });

//   RunTestScript(test_general_testEnvCleanup_js);
// }

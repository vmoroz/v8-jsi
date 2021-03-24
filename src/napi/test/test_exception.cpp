// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_exception_init
#include "js-native-api/test_exception/test.js.h"
#include "js-native-api/test_exception/testFinalizerException.js.h"
#include "js-native-api/test_exception/test_exception.c"

using namespace napitest;

//TODO: [vmoroz] fix
// TEST_P(NapiTestBase, test_exception) {
//   AddNativeModule(
//       "./build/x86/test_exception",
//       [](napi_env env, napi_value exports) { return Init(env, exports); });

//   RunTestScript(test_exception_test_js);
// }

//TODO: [vmoroz] fix
// TEST_P(NapiTestBase, test_exception) {
//   AddNativeModule(
//       "./build/x86/test_exception",
//       [](napi_env env, napi_value exports) { return Init(env, exports); });

//   RunTestScript(test_exception_testFinalizerException_js);
// }

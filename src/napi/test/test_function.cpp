// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_function_init
#include "js-native-api/test_function/test.js.h"
#include "js-native-api/test_function/test_function.c"

using namespace napitest;

//TODO: [vmoroz] Fix
// TEST_P(NapiTestBase, test_function) {
//   auto testContext = NapiTestContext(this, env);
//   AddNativeModule(
//       "./build/x86/test_function",
//       [](napi_env env, napi_value exports) { return Init(env, exports); });
//   RunTestScript(test_function_test_js);
// }

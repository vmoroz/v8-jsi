// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_promise_init
#include "js-native-api/test_promise/test.js.h"
#include "js-native-api/test_promise/test_promise.c"

using namespace napitest;

//TODO: [vmoroz] Fix
// TEST_P(NapiTestBase, test_promise) {
//   AddNativeModule(
//       "./build/x86/test_promise",
//       [](napi_env env, napi_value exports) { return Init(env, exports); });

//   RunTestScript(test_promise_test_js);
// }
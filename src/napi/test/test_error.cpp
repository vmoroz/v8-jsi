// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_error_init
#include "js-native-api/test_error/test.js.h"
#include "js-native-api/test_error/test_error.c"

using namespace napitest;

//TODO: [vmoroz] fix
// TEST_P(NapiTestBase, test_error) {
//   AddNativeModule(
//       "./build/x86/test_error",
//       [](napi_env env, napi_value exports) { return Init(env, exports); });

//   RunTestScript(test_error_test_js);
// }

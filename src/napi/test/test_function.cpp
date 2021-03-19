// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#define NAPI_EXPERIMENTAL
#include "../js_native_test_api.h"
#include "js-native-api/common.h"
#include "napitest.h"

//#include "js-native-api/test_function/test_function.c.h"

using namespace napitest;

TEST_P(NapiTestBase, CallFunction) {
  try {
    RunScript(R"(

      const assert = require('assert');
      assert.ok(true);

    )");
    // auto result = RunScript(GetJSAssert());
    // EXPECT_JS_STRICT_EQ(result, "100");
  } catch (std::exception const &ex) {
    FAIL() << "Exception thrown: " << ex.what();
    return;
  }
}

INSTANTIATE_TEST_SUITE_P(
    NapiEnv,
    NapiTestBase,
    ::testing::ValuesIn(NapiEnvProviders()));

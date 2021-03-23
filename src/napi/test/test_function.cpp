// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#define NAPI_EXPERIMENTAL
#include "../js_native_test_api.h"
#include "js-native-api/common.h"
#include "napitest.h"

//#include "js-native-api/test_function/test_function.c.h"

using namespace napitest;

TEST_P(NapiTestBase, test_function) {
  RUN_TEST_SCRIPT(R"(
    const assert = require('assert');
    assert.ok(true);
  )");
}

INSTANTIATE_TEST_SUITE_P(
    NapiEnv,
    NapiTestBase,
    ::testing::ValuesIn(NapiEnvProviders()));

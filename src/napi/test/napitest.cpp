// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#define NAPI_EXPERIMENTAL
#include "napitest.h"

using namespace napitest;

struct NapiTest : NapiTestBase {};

TEST_P(NapiTest, RunScriptTest) {
    EXPECT_EQ(1, 1);
}

INSTANTIATE_TEST_SUITE_P(NapiEnv, NapiTest, ::testing::ValuesIn(NapiEnvFactories()));

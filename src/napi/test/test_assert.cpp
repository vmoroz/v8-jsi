// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

using namespace napitest;

TEST_P(NapiTestBase, test_assert) {
  RUN_TEST_SCRIPT("require('assert').ok(true);");

  RUN_TEST_SCRIPT("require('assert').ok(false);")
      .Throws([](NapiTestException const &ex) noexcept {
        EXPECT_TRUE(ex.AssertionError());
        EXPECT_EQ(ex.AssertionError()->Expected, "true");
        EXPECT_EQ(ex.AssertionError()->Actual, "false");
      });

  RUN_TEST_SCRIPT("require('assert').ok();")
      .Throws([](NapiTestException const &ex) noexcept {
        EXPECT_TRUE(ex.AssertionError());
        EXPECT_EQ(ex.AssertionError()->Expected, "true");
        EXPECT_EQ(ex.AssertionError()->Actual, "undefined");
      });
}

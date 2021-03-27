// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "js_engine_api.h"
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

  RUN_TEST_SCRIPT(R"({
    const assert = require('assert');
    const fn = assert.mustNotCall();
    fn(1, 2, 3); // must cause an AssertionException
    })")
      .Throws([](NapiTestException const &ex) noexcept {
        EXPECT_TRUE(ex.AssertionError());
        // TODO: [vmoroz] clean error reporting
      });

  RUN_TEST_SCRIPT("require('assert').mustNotCall();");

  RUN_TEST_SCRIPT("require('assert').mustCall();")
      .Throws([](NapiTestException const &ex) noexcept {
        EXPECT_TRUE(ex.AssertionError());
        // TODO: [vmoroz] clean error reporting
      });

  RUN_TEST_SCRIPT(R"({
    const assert = require('assert');
    const fn = assert.mustCall();
    fn(1, 2, 3);
    })");

  RUN_TEST_SCRIPT(R"({
    const assert = require('assert');
    const fn = assert.mustCall((x, y) => x + y);
    assert.strictEqual(fn(1, 2), 3);
    })");

  RUN_TEST_SCRIPT(R"({
    const assert = require('assert');
    let resolvePromise;
    const promise = new Promise((resolve) => {resolvePromise = resolve;});
    promise.then(() => {
      assert.fail('Continuation must fail');
    });
    resolvePromise();
    })")
      .Throws([](NapiTestException const &ex) noexcept {
        EXPECT_TRUE(ex.AssertionError());
        // TODO: [vmoroz] clean error reporting
      });
}

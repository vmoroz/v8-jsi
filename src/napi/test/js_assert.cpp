// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "js_assert.h"

namespace napitest {

const char *GetJSAssert() noexcept {
  return R"JavaScript(
(function() {
  'use strict';
  const exports = {};

  function AssertException(method, expected, actual) {
    this.method = 'assert.' + method;
    this.expected = expected;
    this.actual = actual;
  }

  exports.ok = (expr) => {
    if (!expr) {
      throw new AssertException('ok', 'true', 'false');
    }
  };

  return exports;
})();
  )JavaScript";
}

}

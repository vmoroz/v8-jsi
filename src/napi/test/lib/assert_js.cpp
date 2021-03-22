// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "modules.h"

namespace napitest {
namespace module {

const char *assert_js = R"JavaScript(

'use strict';

const AssertionError = require('assertion_error');

const assert = module.exports = ok;
assert.AssertionError = AssertionError;

function innerOk(fn, argLen, value, message) {
  if (!value) {
    if (argLen === 0) {
      message = 'No value argument passed to `assert.ok()`';
    } else if (message == null) {
      message = 'The expression evaluated to a falsy value';
    }

    throw new AssertionError({
      message,
      actual: value,
      expected: true,
      method: fn
    });
  }
}

// Pure assertion tests whether a value is truthy, as determined by !!value.
function ok(...args) {
  innerOk(ok, args.length, ...args);
}
assert.ok = ok;

)JavaScript";

} // namespace module
} // namespace napitest

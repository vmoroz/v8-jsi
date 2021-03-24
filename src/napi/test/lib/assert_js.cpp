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

function innerComparison(method, compare, defaultMessage, argLen, actual, expected, message) {
  if (!compare(actual, expected)) {
    if (argLen < 2) {
      message = `'assert.${method.name}' expects two or more arguments.`;
    } else if (message == null) {
      message = defaultMessage;
    }
    throw new AssertionError({message, actual, expected, method});
  }
}

assert.strictEqual = function strictEqual(...args) {
  innerComparison(strictEqual, Object.is,
    'Values are not strict equal.', args.length, ...args);
}

assert.deepStrictEqual = function deepStrictEqual(...args) {
  innerComparison(deepStrictEqual, isDeepStrictEqual,
    'Values are not deep strict equal.', args.length, ...args);
}

assert.notDeepStrictEqual = function notDeepStrictEqual(...args) {
  innerComparison(notDeepStrictEqual, negate(isDeepStrictEqual),
    'Values must not be deep strict equal.', args.length, ...args);
}

function innerThrows(method, argLen, fn, expected, message) {
  let actual = 'Did not throw';
  function succeeds() {
    try {
      fn();
      return false;
    } catch (error) {
      actual = `${error.name}: ${error.message}`;
      return expected ? expected.test(actual) : true;
    }
  }

  if (argLen < 1 || typeof fn !== 'function') {
    message = `'assert.${method.name}' expects a function parameter.`;
  } else if (message == null) {
    if (expected) {
      message = `'assert.${method.name}' failed to throw an exception that matches '${expected}'.`;
    } else {
      message = `'assert.${method.name}' failed to throw an exception.`;
    }
  }

  if (!succeeds()) {
    throw new AssertionError({message, actual, expected, method});
  }
}

assert.throws = function throws(...args) {
  innerThrows(throws, args.length, ...args);
}

function negate(compare) {
  return (...args) => !compare(...args);
}

function isDeepStrictEqual(left, right) {
  function check(left, right) {
    if (left === right) {
      return true;
    }
    if (typeof left !== typeof right) {
      return false;
    }
    if (Array.isArray(left)) {
      return Array.isArray(right) && checkArray(left, right);
    }
    if (typeof left === 'number') {
      return isNaN(left) && isNaN(right);
    }
    if (typeof left === 'object') {
      return (typeof right === 'object') && checkObject(left, right);
    }
    return false;
  }

  function checkArray(left, right) {
    if (left.length !== right.length) {
      return false;
    }
    for (let i = 0; i < left.length; ++i) {
      if (!check(left[i], right[i])) {
        return false;
      }
    }
    return true;
  }

  function checkObject(left, right) {
    const leftNames = Object.getOwnPropertyNames(left);
    const rightNames = Object.getOwnPropertyNames(right);
    if (leftNames.length !== rightNames.length) {
      return false;
    }
    for (let i = 0; i < leftNames.length; ++i) {
      if (!check(left[leftNames[i]], right[leftNames[i]])) {
        return false;
      }
    }
    const leftSymbols = Object.getOwnPropertySymbols(left);
    const rightSymbols = Object.getOwnPropertySymbols(right);
    if (leftSymbols.length !== rightSymbols.length) {
      return false;
    }
    for (let i = 0; i < leftSymbols.length; ++i) {
      if (!check(left[leftSymbols[i]], right[leftSymbols[i]])) {
        return false;
      }
    }
    return check(Object.getPrototypeOf(left), Object.getPrototypeOf(right));
  }

  return check(left, right);
}

)JavaScript";

} // namespace module
} // namespace napitest

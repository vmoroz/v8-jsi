// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "modules.h"

namespace napitest {
namespace module {

const char *assertion_error_js = R"JavaScript(

'use strict';

class AssertionError extends Error {
  constructor(options) {
    const {
      message,
      actual,
      expected,
      method,
    } = options;

    super(String(message));

    this.name = 'AssertionError';
    setAssertionSource(this, method);
    this.actual = String(actual);
    this.expected = String(expected);
  }

  toString() {
    return `${this.name}: ${this.message}`;
  }
}

function setAssertionSource(error, method) {
  let result = { sourceFile: '<Unknown>', sourceLine: 0 };
  const stackArray = error.stack.split('\n');
  const methodNamePattern = `.${method.name} (`;
  let methodNameFound = false;
  for (const stackFrame of stackArray) {
    if (methodNameFound) {
      const stackFrameParts = stackFrame.split(':');
      if (stackFrameParts.length >= 2) {
        let sourceFile = stackFrameParts[0];
        if (sourceFile.startsWith('    at ')) {
          sourceFile = sourceFile.substr(7);
        }
        result = { sourceFile, sourceLine: Number(stackFrameParts[1]) };
      }
      break;
    } else {
      methodNameFound = stackFrame.indexOf(methodNamePattern) >= 0;
    }
  }
  Object.assign(error, result);
}

module.exports = AssertionError;

)JavaScript";

} // namespace module
} // namespace napitest

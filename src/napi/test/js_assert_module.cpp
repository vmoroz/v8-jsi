// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "js_assert_module.h"

namespace napitest {

const char *js_assert_module = R"JavaScript(

'use strict';

class AssertError extends Error {
  constructor(assertInfo) {
    const method = assertInfo.method ? 'assert.' + assertInfo.method : 'Unknown';
    const expected = assertInfo.expected || '';
    const actual = assertInfo.actual || '';
    const message = `'${method}' method failed. Expected: ${expected}. Actual: ${actual}.`;
    super(message);
    this.name = this.constructor.name;
    Object.assign(this, {method, expected, actual});
  }
}

exports.ok = (expr) => {
  if (!expr) {
    throw new AssertError({method: 'ok', expected: 'true', actual: 'false'});
  }
};

)JavaScript";

}

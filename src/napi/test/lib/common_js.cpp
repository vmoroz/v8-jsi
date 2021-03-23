// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "modules.h"

namespace napitest {
namespace module {

const char *common_js = R"JavaScript(

'use strict';

const buildType = 'x86';

let expectedGCCallCount = 0;
let actualGCCallCount = 0;
let callStack = '';

function mustCall() {
  expectedGCCallCount = 1;
  actualGCCallCount = 0;
  callStack = (() => {
    try {
      throw new Error();
    } catch(ex) {
      return ex.stack;
    }
  })();
  return function() { ++actualGCCallCount;};
}

function afterGCRun() {
  assert.strictEqual(actualGCCallCount, expectedGCCallCount, 'failed GC calls: ' + callStack);
}

Object.assign(module.exports, {
  buildType,
  mustCall,
  afterGCRun,
});

)JavaScript";

} // namespace module
} // namespace napitest

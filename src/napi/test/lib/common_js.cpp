// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "modules.h"

namespace napitest {
namespace module {
  
DEFINE_TEST_SCRIPT(common_js, R"JavaScript(

'use strict';

const { mustCall, mustCallAtLeast, mustNotCall } = require('assert');

const buildType = 'x86';

function gcUntil(name, condition) {
  if (typeof name === 'function') {
    condition = name;
    name = undefined;
  }
  return new Promise((resolve, reject) => {
    let count = 0;
    function gcAndCheck() {
      setImmediate(() => {
        count++;
        global.gc();
        if (condition()) {
          resolve();
        } else if (count < 10) {
          gcAndCheck();
        } else {
          reject(name === undefined ? undefined : 'Test ' + name + ' failed');
        }
      });
    }
    gcAndCheck();
  });
}

Object.assign(module.exports, {
  buildType,
  gcUntil,
  mustCall,
  mustCallAtLeast,
  mustNotCall,
});

)JavaScript");

} // namespace module
} // namespace napitest

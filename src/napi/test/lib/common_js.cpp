// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "modules.h"

namespace napitest {
namespace module {

const char *common_js = R"JavaScript(

'use strict';

const { mustCall, mustCallAtLeast, mustNotCall } = require('assert');

const buildType = 'x86';

Object.assign(module.exports, {
  buildType,
  mustCall,
  mustCallAtLeast,
  mustNotCall,
});

)JavaScript";

} // namespace module
} // namespace napitest

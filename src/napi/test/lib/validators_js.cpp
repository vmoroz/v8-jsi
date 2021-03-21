// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "modules.h"

namespace napitest {
namespace module {

const char *validators_js = R"JavaScript(

'use strict';

const {
  hideStackFrames,
  codes: {
    ERR_INVALID_ARG_TYPE,
  }
} = require('errors');

const validateObject = hideStackFrames(
  (value, name, {
    nullable = false,
    allowArray = false,
    allowFunction = false,
  } = {}) => {
    if ((!nullable && value === null) ||
        (!allowArray && Array.isArray(value)) ||
        (typeof value !== 'object' && (
          !allowFunction || typeof value !== 'function'
        ))) {
      throw new ERR_INVALID_ARG_TYPE(name, 'Object', value);
    }
  });

module.exports = {
  validateObject,
};

)JavaScript";

} // namespace module
} // namespace napitest

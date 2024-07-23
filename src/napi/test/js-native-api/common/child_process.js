// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

'use strict';

exports.spawnSync = function(command, args) {
  return process.__spawnSync__(command, args);
};

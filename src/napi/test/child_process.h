// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once
#ifndef CHILD_PROCESS_H_
#define CHILD_PROCESS_H_

#include <string>
#include <string_view>
#include <vector>

struct ProcessResult {
  uint32_t status;
  std::string std_output;
  std::string std_error;
};

ProcessResult spawnSync(std::string_view command, std::vector<std::string> args);

#endif // !CHILD_PROCESS_H_
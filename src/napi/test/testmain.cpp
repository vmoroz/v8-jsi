// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <gtest/gtest.h>
#include <thread>
#include "public/v8_api.h"

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

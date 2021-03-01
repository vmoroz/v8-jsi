// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// These tests are for NAPI and are not JS engine specific

#pragma once

#include <functional>
#include <vector>
#include <vector>

#include <gtest/gtest.h>
//#include <napi/js_native_api_ext.h>

struct napi_env__ {};
using napi_env = napi_env__*;

namespace napitest {

using NapiEnvFactory = std::function<napi_env()>;

std::vector<NapiEnvFactory> NapiEnvFactories();

struct NapiTestBase : ::testing::TestWithParam<NapiEnvFactory> {};

} // namespace napitest
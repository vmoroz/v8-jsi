// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#define NAPI_EXPERIMENTAL
#include "../js_native_test_api.h"
#include "js-native-api/common.h"
#include "napitest.h"

#include "js-native-api/test_function/test_function.c.h"

using namespace napitest;

TEST_P(NapiTestBase, test_function) {
  napi_value exports{};
  napi_create_object(env, &exports);
  Init(env, exports);

  napi_ref moduleRef{};
  napi_create_reference(env, exports, 1, &moduleRef);
  AddModule("./build/x86/test_function", moduleRef);

  RUN_TEST_SCRIPT(R"(
'use strict';
// Flags: --expose-gc

const common = require('../../common');
const assert = require('assert');

// Testing api calls for function
const test_function = require(`./build/${common.buildType}/test_function`);

function func1() {
  return 1;
}
assert.strictEqual(test_function.TestCall(func1), 1);

function func2() {
//   console.log('hello world!');
  return null;
}
assert.strictEqual(test_function.TestCall(func2), null);

function func3(input) {
  return input + 1;
}
assert.strictEqual(test_function.TestCall(func3, 1), 2);

function func4(input) {
  return func3(input);
}
assert.strictEqual(test_function.TestCall(func4, 1), 2);

assert.strictEqual(test_function.TestName.name, 'Name');
assert.strictEqual(test_function.TestNameShort.name, 'Name_');

// let tracked_function = test_function.MakeTrackedFunction(common.mustCall());
// assert(!!tracked_function);
// tracked_function = null;
// global.gc();

assert.deepStrictEqual(test_function.TestCreateFunctionParameters(), {
  envIsNull: 'Invalid argument',
  nameIsNull: 'napi_ok',
  cbIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument'
});
  )");
}

INSTANTIATE_TEST_SUITE_P(
    NapiEnv,
    NapiTestBase,
    ::testing::ValuesIn(NapiEnvProviders()));

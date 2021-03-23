// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#define NAPI_EXPERIMENTAL
#include "../js_native_test_api.h"
#include "js-native-api/common.h"
#include "napitest.h"

#include "js-native-api/test_function/test_function.c.h"

using namespace napitest;

struct ScopedExposeGC {
  ScopedExposeGC() : m_wasExposed(napi_test_enable_gc_api(true)) {}
  ~ScopedExposeGC() {
    napi_test_enable_gc_api(m_wasExposed);
  }

 private:
  const bool m_wasExposed{false};
};

TEST_P(NapiTestBase, test_function) {
  ScopedExposeGC exposeGC;

  napi_value exports{};
  napi_create_object(env, &exports);
  Init(env, exports);

  napi_ref moduleRef{};
  napi_create_reference(env, exports, 1, &moduleRef);
  AddModule("./build/x86/test_function", moduleRef);

  napi_value global{};
  napi_get_global(env, &global);
  napi_set_named_property(env, global, "global", global);

  napi_value common = GetModule("../../common");
  napi_value afterGCRun{};
  napi_get_named_property(env, common, "afterGCRun", &afterGCRun);

  auto runGCCallback = [](napi_env env, napi_callback_info info) -> napi_value {
    napi_value afterGCRun {};
    napi_get_cb_info(
        env, info, nullptr, nullptr, nullptr, (void **)&afterGCRun);
    napi_test_run_gc(env);
    napi_value undefined{};
    napi_get_undefined(env, &undefined);

    napi_call_function(env, undefined, afterGCRun, 0, nullptr, nullptr);

    return undefined;
  };
  napi_value runGC{};
  napi_create_function(
      env, "gc", NAPI_AUTO_LENGTH, runGCCallback, afterGCRun, &runGC);
  napi_set_named_property(env, global, "gc", runGC);

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
  console.log('hello world!');
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

let tracked_function = test_function.MakeTrackedFunction(common.mustCall());
assert(!!tracked_function);
tracked_function = null;
global.gc();

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

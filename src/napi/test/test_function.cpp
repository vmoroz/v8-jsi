// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#define NAPI_EXPERIMENTAL
#include "../js_native_test_api.h"
#include "js-native-api/common.h"
#include "napitest.h"

//#include "js-native-api/test_function/test_function.c.h"

using namespace napitest;

TEST_P(NapiTestBase, test_function) {
  const int testScriptLine = __LINE__;
  const char *testScript = R"(
    const assert = require('assert');
    assert.ok(false);
  )";

  try {
    RunScript("TestScript", testScript);
  } catch (NapiTestException const &ex) {
    if (ex.AssertionError()) {
      auto sourceFile = ex.AssertionError()->SourceFile;
      auto sourceLine = ex.AssertionError()->SourceLine;
      auto sourceCode = std::string("<Source is unavailable>");
      if (sourceFile == "TestScript") {
        sourceFile = "napi/test/test_function.cpp" ;//__FILE__;
        sourceCode = GetSourceCodeSliceForError(testScript, sourceLine, 2);
        sourceLine += testScriptLine;
      } else if (sourceFile.empty()) {
        sourceFile = "<Unknown>";
      }

      FAIL() << "Exception: " << ex.ScriptError()->Name << "\n"
             << "  Message: " << ex.ScriptError()->Message << "\n"
             << " Expected: " << ex.AssertionError()->Expected << "\n"
             << "   Actual: " << ex.AssertionError()->Actual << "\n"
             << "     File: " << sourceFile << ":" << sourceLine << "\n"
             << sourceCode << "\n"
             << "Callstack: " << ex.ScriptError()->Stack;
    } else if (ex.ScriptError()) {
      FAIL() << "Exception: " << ex.ScriptError()->Name << "\n"
             << "  Message: " << ex.ScriptError()->Message << "\n"
             << "Callstack: " << ex.ScriptError()->Stack;
    } else {
      FAIL() << "Exception: NapiTestException\n"
             << "     Code: " << ex.ErrorCode() << "\n"
             << "  Message: " << ex.what() << "\n"
             << "     Expr: " << ex.Expr();
    }
  } catch (std::exception const &ex) {
    FAIL() << "Exception thrown: " << ex.what();
  } catch (...) {
    FAIL() << "Unexpected test exception.";
  }
}

INSTANTIATE_TEST_SUITE_P(
    NapiEnv,
    NapiTestBase,
    ::testing::ValuesIn(NapiEnvProviders()));

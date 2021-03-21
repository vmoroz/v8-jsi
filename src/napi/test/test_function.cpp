// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#define NAPI_EXPERIMENTAL
#include <iostream>
#include <regex>
#include <sstream>
#include "../js_native_test_api.h"
#include "js-native-api/common.h"
#include "napitest.h"

//#include "js-native-api/test_function/test_function.c.h"

using namespace napitest;

TEST_P(NapiTestBase, CallFunction) {
  const int scriptLineNo = __LINE__;
  const char *testScript = R"(
    const assert = require('assert');
    assert.ok(false);
  )";

  try {
    RunScript("TestScript", testScript);

    // auto result = RunScript(GetJSAssert());
    // EXPECT_JS_STRICT_EQ(result, "100");
  } catch (NapiTestException const &ex) {
    if (ex.AssertionError()) {
      auto sourceLinePattern = std::regex("TestScript:(\\d+):");
      std::smatch matches;
      if (std::regex_search(
              ex.ScriptError()->Stack, matches, sourceLinePattern)) {
        auto lineStr = matches[1].str();
        auto lineNo = std::stoi(lineStr);

        auto sourceStream = std::istringstream(testScript);
        std::string sourceLine;
        std::string sourceCode;
        const int extraLineCount = 2;
        int currentLine = 1;

        while (std::getline(sourceStream, sourceLine, '\n')) {
          if (currentLine > lineNo + extraLineCount)
            break;
          if (currentLine >= lineNo - extraLineCount) {
            sourceCode += "\n";
            sourceCode += currentLine == lineNo ? "===> " : "     ";
            sourceCode += sourceLine;
          }
          ++currentLine;
        }

        FAIL() << " Message: " << ex.ScriptError()->Message << "\n"
               << "  Actual: " << ex.AssertionError()->Actual << "\n"
               << "Expected: " << ex.AssertionError()->Expected << "\n"
               << "    File: " << __FILE__ << "(" << (scriptLineNo + lineNo) << "):\n"
               << sourceCode;
      } else {
        FAIL() << "No line number found";
      }
    } else if (ex.ScriptError()) {
      FAIL() << "Stack: " << ex.ScriptError()->Stack;
    } else {
      FAIL() << "Exception thrown: " << ex.what();
    }
    return;
  } catch (std::exception const &ex) {
    FAIL() << "Exception thrown: " << ex.what();
    return;
  }
}

INSTANTIATE_TEST_SUITE_P(
    NapiEnv,
    NapiTestBase,
    ::testing::ValuesIn(NapiEnvProviders()));

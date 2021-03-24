// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once
#ifndef NAPI_LIB_MODULES_H_
#define NAPI_LIB_MODULES_H_

#include <functional>
#include <map>
#include <string>

#define DEFINE_TEST_SCRIPT(cppId, script) \
  const auto cppId = napitest::TestScriptInfo{script, __FILE__, (__LINE__ - napitest::GetEndOfLineCount(script))};

namespace napitest {

struct TestScriptInfo {
  char const *script;
  char const *file;
  int32_t line;
};

inline int32_t GetEndOfLineCount(char const *script) noexcept {
  return std::count(script, script + strlen(script), '\n');
}

} // namespace napitest

namespace napitest {
namespace module {

extern const char *assert_js;
extern const char *assertion_error_js;
extern const char *common_js;
extern const char *errors_js;
extern const char *inspect_js;
extern const char *validators_js;

inline std::map<std::string, char const *, std::less<>>
GetModuleScripts() noexcept {
  std::map<std::string, char const *, std::less<>> moduleScripts;
  moduleScripts.try_emplace("assert", assert_js);
  moduleScripts.try_emplace("assertion_error", assertion_error_js);
  moduleScripts.try_emplace("../../common", common_js);
  moduleScripts.try_emplace("errors", errors_js);
  moduleScripts.try_emplace("inspect", inspect_js);
  moduleScripts.try_emplace("validators", validators_js);
  return moduleScripts;
}

} // namespace module
} // namespace napitest

#endif // NAPI_LIB_MODULES_H_

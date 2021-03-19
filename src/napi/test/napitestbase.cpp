// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"
#include <algorithm>
#include <limits>

namespace napitest {

NapiTestBase::NapiTestBase()
    : provider(GetParam()), env(provider->CreateEnv()) {}

NapiTestBase::~NapiTestBase() {
  provider->DeleteEnv();
}

std::string NapiTestBase::GetExceptionMessage() {
//   bool isPending;
//   napi_is_exception_pending((env), &isPending);
//   if (isPending) {
//     napi_value error{}, errorMessage{};
//     size_t messageSize{};
//     if (napi_get_and_clear_last_exception(env, &error) == napi_ok) {
//       napi_get_named_property(env, error, "message", &errorMessage);
//       napi_get_value_string_utf8(env, errorMessage, nullptr, 0, &messageSize);
//       std::string messageStr(messageSize, '\0');
//       napi_get_value_string_utf8(
//           env, errorMessage, &messageStr[0], messageSize + 1, nullptr);
//       return messageStr;
//     }
//   }
//   return "No exception pending";
return {};
}

napi_value NapiTestBase::RunScript(const char *code) {
  napi_value script{}, scriptResult{};
  THROW_IF_NOT_OK(
      napi_create_string_utf8(env, code, NAPI_AUTO_LENGTH, &script));
  THROW_IF_NOT_OK(napi_run_script(env, script, &scriptResult));
  return scriptResult;
}

} // namespace napitest
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <gtest/gtest.h>

#include <child_process.h>
#include <filesystem>
#include <string_view>

namespace fs = std::filesystem;

namespace node_api_tests {

struct NodeApiTestFixture : ::testing::Test {
  explicit NodeApiTestFixture(fs::path testProcess, fs::path jsFilePath)
      : m_testProcess(std::move(testProcess)), m_jsFilePath(std::move(jsFilePath)) {}
  static void SetUpTestSuite() {}
  static void TearDownTestSuite() {}
  void SetUp() override {}
  void TearDown() override {}

  void TestBody() override {
    ProcessResult result = spawnSync(m_testProcess.string(), {"--js", m_jsFilePath.string()});
    EXPECT_EQ(result.status, 0);
  }

 private:
  fs::path m_testProcess;
  fs::path m_jsFilePath;
};

int evaluateJSFile(const char *jsFilePath) {
  // Evaluate the JS file
  return 0;
}

void registerNodeApiTests(const char *exePathStr) {
  fs::path exePath = fs::path(exePathStr);
  fs::path rootJsPath = fs::path(exePath).replace_filename("jsi") / "napi" / "test" / "js-native-api";
  for (const fs::directory_entry &dir_entry : fs::recursive_directory_iterator(rootJsPath)) {
    if (dir_entry.is_regular_file() && dir_entry.path().extension() == ".js") {
      fs::path jsFilePath = dir_entry.path();
      std::string testSuiteName = "js_native_api";//jsFilePath.parent_path().parent_path().filename().string();
      std::string testName = jsFilePath.parent_path().filename().string() + "/" + jsFilePath.filename().string();
      ::testing::RegisterTest(
          testSuiteName.c_str(), testName.c_str(), nullptr, nullptr, jsFilePath.string().c_str(), 1, [exePath, jsFilePath]() {
            return new NodeApiTestFixture(exePath, jsFilePath);
          });
    }
  }
}

} // namespace node_api_tests

int main(int argc, char **argv) {
  if (argc >= 3 && std::string_view(argv[1]) == "--js") {
    return node_api_tests::evaluateJSFile(argv[2]);
  } else {
    ::testing::InitGoogleTest(&argc, argv);
    node_api_tests::registerNodeApiTests(argv[0]);
    return RUN_ALL_TESTS();
  }
}

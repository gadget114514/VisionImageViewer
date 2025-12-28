#include "ArchiveHelper.h"
#include "MoveHelper.h"
#include "PluginManager.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <windows.h>

namespace fs = std::filesystem;

// Mock AppLog for tests
void AppLog(const std::wstring &msg) {
  std::wcout << L"[Log] " << msg << std::endl;
}

// Simple Test Framework
int g_pass = 0;
int g_fail = 0;

#define TEST_ASSERT(cond)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "[-] Test Failed: " << #cond << " at line " << __LINE__       \
              << std::endl;                                                    \
    g_fail++;                                                                  \
  } else {                                                                     \
    std::cout << "[+] Test Passed: " << #cond << std::endl;                    \
    g_pass++;                                                                  \
  }

void TestNaturalSort() {
  std::cout << "\n--- Testing Natural Sort (StrCmpLogicalW) ---\n";
  // StrCmpLogicalW is used for human-friendly sorting (2 < 10)
  TEST_ASSERT(StrCmpLogicalW(L"image2.jpg", L"image10.jpg") < 0);
  TEST_ASSERT(StrCmpLogicalW(L"2.jpg", L"10.jpg") < 0);
  TEST_ASSERT(StrCmpLogicalW(L"a1.txt", L"a01.txt") !=
              0); // They are different strings
  TEST_ASSERT(StrCmpLogicalW(L"photo_1.jpg", L"photo_2.jpg") < 0);
}

void TestStringConversion() {
  std::cout << "\n--- Testing UTF-8 Conversion ---\n";
  std::wstring original = L"Hello \u65e5\u672c\u8a9e 123";

  // Test GetImageFiles with a non-existent path to see if it handles conversion
  // without crash
  auto result = ArchiveHelper::GetImageFiles(original);
  TEST_ASSERT(result.empty()); // Should be empty for non-existent file
}

void TestMoveHelper() {
  std::cout << "\n--- Testing MoveHelper Logic ---\n";
  MoveHelper &helper = MoveHelper::Instance();

  // Test Folder Setting Slots
  for (int i = 0; i < 10; ++i) {
    std::wstring path = L"C:\\Temp\\Folder_" + std::to_wstring(i);
    helper.SetFolder(i, path);
    TEST_ASSERT(helper.GetFolder(i) == path);
  }

  // Verify default empty for out of range
  TEST_ASSERT(helper.GetFolder(99) == L"");
}

void TestPluginManager() {
  std::cout << "\n--- Testing PluginManager Logic ---\n";
  PluginManager pm;

  // Register internal plugins
  // RegisterInternalPlugins is public/called by LoadPlugins
  pm.LoadPlugins();

  // We can't check m_plugins directly, but we can check if it handles internal
  // types Internal loaders use WIC. We can test if LoadImage returns null for
  // invalid file nicely
  auto *bmp = pm.LoadImage(L"non_existent.bmp", nullptr);
  TEST_ASSERT(bmp == nullptr);
}

int main() {
  std::cout << "Starting VisionImageViewer Automated Test Suite...\n";

  try {
    TestNaturalSort();
    TestStringConversion();
    TestMoveHelper();
    TestPluginManager();

  } catch (const std::exception &e) {
    std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
    return 1;
  }

  std::cout << "\n======================================\n";
  std::cout << "TEST SUMMARY\n";
  std::cout << "Passed: " << g_pass << "\n";
  std::cout << "Failed: " << g_fail << "\n";
  std::cout << "======================================\n";

  return (g_fail == 0) ? 0 : 1;
}

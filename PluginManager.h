#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <wincodec.h>
#include <windows.h>

// Type definitions for Susie API
#if defined(_WIN64)
#define IMP_LONG_PTR INT_PTR
#define IMP_DWORD_PTR UINT_PTR
#else
#define IMP_LONG_PTR long
#define IMP_DWORD_PTR DWORD
#endif

// Generic Plugin Entry
struct PluginEntry {
  std::wstring name;
  std::wstring path; // Path for dll, or "INTERNAL"
  bool isInternal;
  bool isEnabled;
  std::vector<std::wstring> supportedExtensions;

  // Susie Specific (Unicode)
  HMODULE hModule = NULL;
  int(WINAPI *GetPluginInfoW)(int infono, LPWSTR buf, int buflen) = nullptr;
  int(WINAPI *IsSupportedW)(LPCWSTR filename, IMP_DWORD_PTR dwBuffer) = nullptr;
  int(WINAPI *GetPictureW)(LPCWSTR buf, IMP_LONG_PTR len, unsigned int flag,
                           HLOCAL *pHBInfo, HLOCAL *pHBm, void *lpInfo,
                           IMP_LONG_PTR lpPrgressCallback) = nullptr;

  // Susie Specific (Narrow)
  int(WINAPI *GetPluginInfo)(int infono, LPSTR buf, int buflen) = nullptr;
  int(WINAPI *IsSupported)(LPCSTR filename, IMP_DWORD_PTR dwBuffer) = nullptr;
  int(WINAPI *GetPicture)(LPCSTR buf, IMP_LONG_PTR len, unsigned int flag,
                          HLOCAL *pHBInfo, HLOCAL *pHBm, void *lpInfo,
                          IMP_LONG_PTR lpPrgressCallback) = nullptr;

  bool is64Bit; // for display only
};

class PluginManager {
public:
  PluginManager();
  ~PluginManager();

  void LoadPlugins();

  // Tries loaded plugins in order. Returns WIC Bitmap if successful.
  IWICBitmap *LoadImage(const std::wstring &filename,
                        IWICImagingFactory *pFactory);

  // Management
  std::vector<PluginEntry> &GetPlugins() {
    return m_plugins;
  } // Non-const ref for UI mod
  void MovePluginUp(int index);
  void MovePluginDown(int index);
  void SetPluginEnabled(int index, bool enabled);
  void LoadSettings(const std::wstring &iniPath);
  void SaveSettings(const std::wstring &iniPath);

private:
  std::vector<PluginEntry> m_plugins;
  std::vector<std::pair<std::wstring, bool>> m_savedStates; // Name, Enabled
  std::wstring GetExeDirectory();
  void ScanSusieDirectory(const std::wstring &directory);
  void ParseSusieExtensions(PluginEntry &plugin, const std::wstring &infoStr);

  // Internal Loaders
  void RegisterInternalPlugins();
  IWICBitmap *LoadInternal(const std::wstring &filename,
                           IWICImagingFactory *pFactory);

  // Helper to Create IWICBitmap from raw DIB handles
  IWICBitmap *CreateWicBitmapFromDib(IWICImagingFactory *pFactory, HLOCAL hInfo,
                                     HLOCAL hBits);
  bool InitializeSusieFunctions(PluginEntry &plugin);
};

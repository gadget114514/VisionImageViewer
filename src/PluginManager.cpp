#include "PluginManager.h"
#include <algorithm>
#include <filesystem>
#include <shlobj.h>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

extern void AppLog(const std::wstring &msg);

namespace {
std::wstring GetPluginIniPath() {
  PWSTR pszPath = NULL;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &pszPath))) {
    std::filesystem::path p(pszPath);
    p /= L"VisionImageViewer";
    if (!std::filesystem::exists(p)) {
      std::filesystem::create_directories(p);
    }
    p /= L"VisionImageViewer.ini";
    CoTaskMemFree(pszPath);
    return p.wstring();
  }
  return L"";
}
} // namespace

static int CALLBACK SusieProgress(int nNum, int nDenom, IMP_LONG_PTR lData) {
  return 0; // 0=continue, 1=abort
}

PluginManager::PluginManager() {}

PluginManager::~PluginManager() {
  for (auto &p : m_plugins) {
    if (p.hModule) {
      FreeLibrary(p.hModule);
      p.hModule = NULL;
    }
  }
}

std::wstring PluginManager::GetExeDirectory() {
  wchar_t buffer[MAX_PATH];
  GetModuleFileNameW(NULL, buffer, MAX_PATH);
  std::filesystem::path p(buffer);
  return p.parent_path().wstring();
}

void PluginManager::LoadPlugins() {
  LoadSettings(GetPluginIniPath());

  // 1. Scan Susie Plugins (High Priority by default?)
  // Or Internal First? Request implies "plugin priority can be decided",
  // "cancel internal". So Internal should be in the list.

  std::wstring pluginDir = GetExeDirectory();
  AppLog(L"Scanning plugins in: " + pluginDir);
  ScanSusieDirectory(pluginDir); // Checks root

  std::wstring subDir =
      (std::filesystem::path(pluginDir) / L"plugins").wstring();
  ScanSusieDirectory(subDir);

  // 2. Register Internal Plugins
  RegisterInternalPlugins();
}

void PluginManager::RegisterInternalPlugins() {
  auto checkEnabled = [&](PluginEntry &p) {
    for (const auto &s : m_savedStates) {
      if (s.first == p.name) {
        p.isEnabled = s.second;
        break;
      }
    }
  };

  // PNG
  PluginEntry png;
  png.name = L"Internal PNG Loader";
  png.path = L"INTERNAL";
  png.isInternal = true;
  png.isEnabled = true;
  checkEnabled(png);
  png.supportedExtensions = {L".png"};
  m_plugins.push_back(png);

  // JPG
  PluginEntry jpg;
  jpg.name = L"Internal JPEG Loader";
  jpg.path = L"INTERNAL";
  jpg.isInternal = true;
  jpg.isEnabled = true;
  checkEnabled(jpg);
  jpg.supportedExtensions = {L".jpg", L".jpeg"};
  m_plugins.push_back(jpg);

  // Standard WIC Fallback (BMP, GIF, TIFF, ICO)
  PluginEntry wic;
  wic.name = L"Internal General Loader (BMP/GIF/TIF)";
  wic.path = L"INTERNAL";
  wic.isInternal = true;
  wic.isEnabled = true;
  checkEnabled(wic);
  wic.supportedExtensions = {L".bmp", L".gif", L".tif", L".tiff", L".ico"};
  m_plugins.push_back(wic);

  for (const auto &p : m_plugins) {
    if (p.isInternal) {
      std::wstring exts;
      for (size_t i = 0; i < p.supportedExtensions.size(); ++i) {
        exts += p.supportedExtensions[i] +
                (i == p.supportedExtensions.size() - 1 ? L"" : L", ");
      }
      AppLog(L"Internal Plugin Registered: " + p.name + L" (" + exts + L")");
    }
  }
}

void PluginManager::ScanSusieDirectory(const std::wstring &directory) {
  AppLog(L"Scanning directory: " + directory);
  if (!std::filesystem::exists(directory)) {
    AppLog(L"Directory does not exist.");
    return;
  }

  try {
    for (const auto &entry : std::filesystem::directory_iterator(directory)) {
      std::wstring ext = entry.path().extension().wstring();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

      if (ext == L".sph") {
        HMODULE hMod = LoadLibraryW(entry.path().c_str());
        if (hMod) {
          PluginEntry plugin;
          plugin.hModule = hMod;
          plugin.path = entry.path().wstring();
          plugin.isInternal = false;
          plugin.isEnabled = true; // default enabled
#if defined(_WIN64)
          plugin.is64Bit = true;
#else
          plugin.is64Bit = false;
#endif
          // Check saved state
          for (const auto &s : m_savedStates) {
            // Match primarily by name if available, or path?
            // Since we load name AFTER init, using path might be safer
            // initially. But RegisterInternal uses Name. Let's defer check
            // until after InitializeSusieFunctions gets the name.
          }

          if (InitializeSusieFunctions(plugin)) {
            // Now check saved state by name
            for (const auto &s : m_savedStates) {
              if (s.first == plugin.name) {
                plugin.isEnabled = s.second;
                break;
              }
            }

            // Only keep loaded if enabled? No, we keep all in list so user can
            // enable/disable. But if disabled, we should unload DLL now?
            // "SetPluginEnabled" logic handles loading/unloading.
            // If initially disabled, we should FreeLibrary now and clear
            // hModule, but keep entry in list.

            if (!plugin.isEnabled) {
              FreeLibrary(hMod);
              plugin.hModule = NULL;
              // Clear pointers
              plugin.GetPluginInfoW = nullptr;
              plugin.IsSupportedW = nullptr;
              plugin.GetPictureW = nullptr;
              plugin.GetPluginInfo = nullptr;
              plugin.IsSupported = nullptr;
              plugin.GetPicture = nullptr;
              AppLog(L"Plugin loaded but disabled by settings: " + plugin.name);
            } else {
              std::wstring exts;
              for (size_t i = 0; i < plugin.supportedExtensions.size(); ++i) {
                exts +=
                    plugin.supportedExtensions[i] +
                    (i == plugin.supportedExtensions.size() - 1 ? L"" : L", ");
              }
              AppLog(L"Loaded plugin: " + plugin.name + L" [" + exts + L"]");
            }
            m_plugins.push_back(plugin);
          } else {
            AppLog(L"Plugin " + entry.path().filename().wstring() +
                   L" is missing required exports.");
            FreeLibrary(hMod);
          }
        } else {
          DWORD err = GetLastError();
          AppLog(L"Failed to load library: " +
                 entry.path().filename().wstring() + L" Error: " +
                 std::to_wstring(err));
        }
      }
    }
  } catch (const std::exception &e) {
    std::string err = e.what();
    std::wstring werr(err.begin(), err.end());
    AppLog(L"Error scanning directory: " + werr);
  }
}

void PluginManager::ParseSusieExtensions(PluginEntry &plugin,
                                         const std::wstring &infoStr) {
  // Common formats: "*.jpg;*.jpeg", "*.jpg/*.jpeg", "*.jpg *.jpeg"
  std::wstring temp;
  auto addExt = [&](const std::wstring &s) {
    if (!s.empty()) {
      std::wstring ext = s;
      // Remove star if present
      size_t star = ext.find(L'*');
      if (star != std::wstring::npos) {
        ext.erase(0, star + 1);
      }
      // Ensure it starts with a dot if it's just 'jpg'
      if (!ext.empty() && ext[0] != L'.') {
        ext = L"." + ext;
      }
      if (!ext.empty()) {
        // Check if already exists
        bool exists = false;
        for (const auto &existing : plugin.supportedExtensions) {
          if (existing == ext) {
            exists = true;
            break;
          }
        }
        if (!exists) {
          plugin.supportedExtensions.push_back(ext);
        }
      }
    }
  };

  for (wchar_t c : infoStr) {
    if (c == L';' || c == L'/' || c == L' ' || c == L',' || c == L'|') {
      if (!temp.empty()) {
        addExt(temp);
        temp.clear();
      }
    } else {
      temp += towlower(c);
    }
  }
  addExt(temp);
}

IWICBitmap *PluginManager::LoadImage(const std::wstring &filename,
                                     IWICImagingFactory *pFactory) {
  std::wstring ext = std::filesystem::path(filename).extension().wstring();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

  for (const auto &plugin : m_plugins) {
    if (!plugin.isEnabled)
      continue;

    // Check extension match
    bool match = false;
    for (const auto &supported : plugin.supportedExtensions) {
      if (supported == ext) {
        match = true;
        break;
      }
    }
    if (!match)
      continue;

    if (plugin.isInternal) {
      AppLog(L"Trying Internal Plugin: " + plugin.name + L" for " + filename);
      // WIC Load
      IWICBitmapDecoder *pDecoder = NULL;
      HRESULT hr = pFactory->CreateDecoderFromFilename(
          filename.c_str(), NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
          &pDecoder);
      if (SUCCEEDED(hr)) {
        IWICBitmapFrameDecode *pFrame = NULL;
        hr = pDecoder->GetFrame(0, &pFrame);
        if (SUCCEEDED(hr)) {
          IWICBitmap *pBitmap = NULL;
          hr = pFactory->CreateBitmapFromSource(pFrame, WICBitmapCacheOnLoad,
                                                &pBitmap);
          pFrame->Release();
          pDecoder->Release();
          if (SUCCEEDED(hr)) {
            AppLog(L"Successfully loaded with: " + plugin.name);
            return pBitmap;
          }
        }
        pDecoder->Release();
      }
    } else {
      AppLog(L"Trying Susie Plugin: " + plugin.name + L" for " + filename);

      bool supported = false;
      std::vector<char> header(2048, 0);
      DWORD bytesRead = 0;

      // 1. Read file header for IsSupported (spec says 2KB)
      HANDLE hFile = CreateFileW(filename.c_str(), GENERIC_READ,
                                 FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
      if (hFile != INVALID_HANDLE_VALUE) {
        ReadFile(hFile, header.data(), 2048, &bytesRead, NULL);
        CloseHandle(hFile);
      }

      // 2. Try IsSupported (pass header pointer)
      if (plugin.IsSupportedW) {
        supported = plugin.IsSupportedW(filename.c_str(),
                                        (IMP_DWORD_PTR)header.data()) != 0;
      } else if (plugin.IsSupported) {
        // In 64-bit Susie plugins, narrow APIs usually expect Unicode (UTF-16)
        // because of TCHAR alignment in the spec.
        supported = plugin.IsSupported((LPCSTR)filename.c_str(),
                                       (IMP_DWORD_PTR)header.data()) != 0;
        if (!supported) {
          // Fallback to narrow ACP
          int len = WideCharToMultiByte(CP_ACP, 0, filename.c_str(), -1, NULL,
                                        0, NULL, NULL);
          if (len > 0) {
            std::vector<char> nfile(len);
            WideCharToMultiByte(CP_ACP, 0, filename.c_str(), -1, nfile.data(),
                                len, NULL, NULL);
            supported = plugin.IsSupported(nfile.data(),
                                           (IMP_DWORD_PTR)header.data()) != 0;
          }
        }
      }

      if (supported) {
        HLOCAL hInfo = NULL;
        HLOCAL hBits = NULL;
        int result = -1;

        if (plugin.GetPictureW) {
          result = plugin.GetPictureW(filename.c_str(), 0, 0, &hInfo, &hBits,
                                      (void *)SusieProgress, 0);
        } else if (plugin.GetPicture) {
          // Try Unicode first (LPCWSTR cast to LPCSTR)
          result = plugin.GetPicture((LPCSTR)filename.c_str(), 0, 0, &hInfo,
                                     &hBits, (void *)SusieProgress, 0);
          if (result != 0) {
            // Fallback to narrow ACP
            int len = WideCharToMultiByte(CP_ACP, 0, filename.c_str(), -1, NULL,
                                          0, NULL, NULL);
            if (len > 0) {
              std::vector<char> nfile(len);
              WideCharToMultiByte(CP_ACP, 0, filename.c_str(), -1, nfile.data(),
                                  len, NULL, NULL);
              result = plugin.GetPicture(nfile.data(), 0, 0, &hInfo, &hBits,
                                         (void *)SusieProgress, 0);
            }
          }
        }

        if (result == 0) {
          IWICBitmap *pBitmap = CreateWicBitmapFromDib(pFactory, hInfo, hBits);
          if (hInfo)
            LocalFree(hInfo);
          if (hBits)
            LocalFree(hBits);
          if (pBitmap) {
            AppLog(L"Successfully loaded with Susie: " + plugin.name);
            return pBitmap;
          } else {
            AppLog(L"Susie Plugin " + plugin.name +
                   L": Failed to convert DIB to WIC Bitmap.");
          }
        } else {
          std::wstring errMsg;
          switch (result) {
          case -1:
            errMsg = L"Not implemented";
            break;
          case 1:
            errMsg = L"Aborted by callback";
            break;
          case 2:
            errMsg = L"Unknown format";
            break;
          case 3:
            errMsg = L"Corrupted data";
            break;
          case 4:
            errMsg = L"Memory allocation error";
            break;
          case 5:
            errMsg = L"Lock error";
            break;
          case 6:
            errMsg = L"Read error";
            break;
          case 8:
            errMsg = L"Internal error";
            break;
          case 9:
            errMsg = L"Write error";
            break;
          default:
            errMsg = L"Other error: " + std::to_wstring(result);
            break;
          }
          AppLog(L"Susie Plugin " + plugin.name + L" GetPicture error: " +
                 errMsg);
        }
      }
    }
  }
  return nullptr;
}

IWICBitmap *PluginManager::CreateWicBitmapFromDib(IWICImagingFactory *pFactory,
                                                  HLOCAL hInfo, HLOCAL hBits) {
  if (!hInfo || !hBits)
    return nullptr;

  // Susie spec says plugins use LocalAlloc, so we use LocalLock.
  BITMAPINFO *pbmi = (BITMAPINFO *)LocalLock(hInfo);
  BYTE *pSrcBits = (BYTE *)LocalLock(hBits);

  IWICBitmap *pBitmap = NULL;
  if (pbmi && pSrcBits) {
    int width = pbmi->bmiHeader.biWidth;
    int height = pbmi->bmiHeader.biHeight;
    bool bottomUp = height > 0;
    height = abs(height);
    int bpp = pbmi->bmiHeader.biBitCount;
    int stride = ((width * bpp + 31) / 32) * 4;

    WICPixelFormatGUID format = GUID_WICPixelFormatUndefined;
    if (bpp == 24)
      format = GUID_WICPixelFormat24bppBGR;
    else if (bpp == 32)
      format = GUID_WICPixelFormat32bppBGRA;
    else if (bpp == 8)
      format = GUID_WICPixelFormat8bppIndexed;

    if (format != GUID_WICPixelFormatUndefined) {
      if (bottomUp) {
        pFactory->CreateBitmap(width, height, format, WICBitmapCacheOnLoad,
                               &pBitmap);
        if (pBitmap) {
          IWICBitmapLock *pLock = NULL;
          WICRect rect = {0, 0, width, height};
          if (SUCCEEDED(pBitmap->Lock(&rect, WICBitmapLockWrite, &pLock))) {
            UINT cbStride = 0;
            UINT cbBufferSize = 0;
            BYTE *pDstBits = NULL;
            pLock->GetStride(&cbStride);
            pLock->GetDataPointer(&cbBufferSize, &pDstBits);
            if (pDstBits) {
              UINT rowSize = (width * bpp + 7) / 8;
              for (int y = 0; y < height; ++y) {
                // Susie/DIB is bottom-up. WIC is top-down.
                memcpy(pDstBits + y * cbStride,
                       pSrcBits + (height - 1 - y) * stride,
                       (cbStride < rowSize ? cbStride : rowSize));
              }
            }
            pLock->Release();
          }
        }
      } else {
        pFactory->CreateBitmapFromMemory(width, height, format, stride,
                                         stride * height, pSrcBits, &pBitmap);
      }

      // If Indexed, we need to set palette
      if (pBitmap && bpp == 8) {
        IWICPalette *pPalette = NULL;
        pFactory->CreatePalette(&pPalette);
        if (pPalette) {
          int colors = pbmi->bmiHeader.biClrUsed;
          if (colors == 0)
            colors = 256;
          WICColor wicColors[256];
          for (int i = 0; i < colors; ++i) {
            wicColors[i] = 0xFF000000 | (pbmi->bmiColors[i].rgbRed << 16) |
                           (pbmi->bmiColors[i].rgbGreen << 8) |
                           pbmi->bmiColors[i].rgbBlue;
          }
          pPalette->InitializeCustom(wicColors, colors);
          pBitmap->SetPalette(pPalette);
          pPalette->Release();
        }
      }
    }
  }

  if (pbmi)
    LocalUnlock(hInfo);
  if (pSrcBits)
    LocalUnlock(hBits);
  return pBitmap;
}

void PluginManager::MovePluginUp(int index) {
  if (index > 0 && index < m_plugins.size()) {
    std::swap(m_plugins[index], m_plugins[index - 1]);
  }
}

void PluginManager::MovePluginDown(int index) {
  if (index >= 0 && index < m_plugins.size() - 1) {
    std::swap(m_plugins[index], m_plugins[index + 1]);
  }
}

void PluginManager::SetPluginEnabled(int index, bool enabled) {
  if (index >= 0 && index < m_plugins.size()) {
    auto &plugin = m_plugins[index];
    if (plugin.isEnabled == enabled)
      return;

    plugin.isEnabled = enabled;

    if (!plugin.isInternal) {
      if (enabled) {
        // Load DLL
        if (!plugin.hModule) {
          HMODULE hMod = LoadLibraryW(plugin.path.c_str());
          if (hMod) {
            plugin.hModule = hMod;
            if (InitializeSusieFunctions(plugin)) {
              AppLog(L"Enabled and Loaded plugin: " + plugin.name);
            } else {
              AppLog(L"Failed to initialize plugin after enabling: " +
                     plugin.name);
              FreeLibrary(hMod);
              plugin.hModule = NULL;
              plugin.isEnabled = false;
            }
          } else {
            AppLog(L"Failed to load DLL when enabling plugin: " + plugin.path);
            plugin.isEnabled = false;
          }
        }
      } else {
        // Unload DLL
        if (plugin.hModule) {
          FreeLibrary(plugin.hModule);
          plugin.hModule = NULL;
          plugin.GetPluginInfoW = nullptr;
          plugin.IsSupportedW = nullptr;
          plugin.GetPictureW = nullptr;
          plugin.GetPluginInfo = nullptr;
          plugin.IsSupported = nullptr;
          plugin.GetPicture = nullptr;
          AppLog(L"Disabled and Unloaded plugin: " + plugin.name);
        }
      }
    } else {
      AppLog(L"Internal plugin " + plugin.name +
             (enabled ? L" enabled." : L" disabled."));
    }
  }
}

bool PluginManager::InitializeSusieFunctions(PluginEntry &plugin) {
  if (!plugin.hModule)
    return false;

  // Try Unicode first
  plugin.GetPluginInfoW = (int(WINAPI *)(int, LPWSTR, int))GetProcAddress(
      plugin.hModule, "GetPluginInfoW");
  plugin.IsSupportedW = (int(WINAPI *)(LPCWSTR, IMP_DWORD_PTR))GetProcAddress(
      plugin.hModule, "IsSupportedW");
  plugin.GetPictureW =
      (int(WINAPI *)(LPCWSTR, IMP_LONG_PTR, unsigned int, HLOCAL *, HLOCAL *,
                     void *, IMP_LONG_PTR))GetProcAddress(plugin.hModule,
                                                          "GetPictureW");

  // Try Narrow if Unicode not found
  if (!plugin.GetPluginInfoW) {
    plugin.GetPluginInfo = (int(WINAPI *)(int, LPSTR, int))GetProcAddress(
        plugin.hModule, "GetPluginInfo");
  }
  if (!plugin.IsSupportedW) {
    plugin.IsSupported = (int(WINAPI *)(LPCSTR, IMP_DWORD_PTR))GetProcAddress(
        plugin.hModule, "IsSupported");
  }
  if (!plugin.GetPictureW) {
    plugin.GetPicture =
        (int(WINAPI *)(LPCSTR, IMP_LONG_PTR, unsigned int, HLOCAL *, HLOCAL *,
                       void *, IMP_LONG_PTR))GetProcAddress(plugin.hModule,
                                                            "GetPicture");
  }

  bool hasUnicode =
      plugin.GetPluginInfoW && plugin.IsSupportedW && plugin.GetPictureW;
  bool hasNarrow =
      plugin.GetPluginInfo && plugin.IsSupported && plugin.GetPicture;

  if (hasUnicode) {
    wchar_t buf[1024];
    // Filter by "00IN"
    if (plugin.GetPluginInfoW(0, buf, 1024) > 0) {
      if (wcsstr(buf, L"00IN") == nullptr)
        return false;
    }

    for (int n = 2; plugin.GetPluginInfoW(n, buf, 1024) != 0; n += 2) {
      ParseSusieExtensions(plugin, buf);
    }
    if (plugin.name.empty()) {
      if (plugin.GetPluginInfoW(1, buf, 1024) != 0)
        plugin.name = buf;
    }
    return true;
  } else if (hasNarrow) {
    char buf[1024];
    // Check if it's an image plugin and detect Unicode-in-Narrow
    bool isUnicodeInNarrow = false;
    if (plugin.GetPluginInfo(0, buf, 1024) > 0) {
      // If buf[1] == 0, it's probably Unicode
      if (buf[1] == 0 && buf[3] == 0)
        isUnicodeInNarrow = true;

      if (isUnicodeInNarrow) {
        if (wcsstr((wchar_t *)buf, L"00IN") == nullptr)
          return false;
      } else {
        if (strstr(buf, "00IN") == nullptr)
          return false;
      }
    }

    for (int n = 2; plugin.GetPluginInfo(n, buf, 1024) != 0; n += 2) {
      if (isUnicodeInNarrow) {
        ParseSusieExtensions(plugin, (wchar_t *)buf);
      } else {
        int len = MultiByteToWideChar(CP_ACP, 0, buf, -1, NULL, 0);
        if (len > 0) {
          std::vector<wchar_t> wbuf(len);
          MultiByteToWideChar(CP_ACP, 0, buf, -1, wbuf.data(), len);
          ParseSusieExtensions(plugin, wbuf.data());
        }
      }
    }
    if (plugin.name.empty()) {
      if (plugin.GetPluginInfo(1, buf, 1024) != 0) {
        if (isUnicodeInNarrow) {
          plugin.name = (wchar_t *)buf;
        } else {
          int len = MultiByteToWideChar(CP_ACP, 0, buf, -1, NULL, 0);
          if (len > 0) {
            std::vector<wchar_t> wbuf(len);
            MultiByteToWideChar(CP_ACP, 0, buf, -1, wbuf.data(), len);
            plugin.name = wbuf.data();
          }
        }
      }
    }
    return true;
  }

  return false;
}

void PluginManager::LoadSettings(const std::wstring &iniPath) {
  if (iniPath.empty())
    return;

  m_savedStates.clear();
  const int bufSize = 32768; // 32KB buffer
  std::vector<wchar_t> buffer(bufSize);
  DWORD length = GetPrivateProfileSectionW(L"Plugins", buffer.data(), bufSize,
                                           iniPath.c_str());
  if (length > 0) {
    wchar_t *p = buffer.data();
    while (*p) {
      std::wstring entry = p;
      size_t eq = entry.find(L'=');
      if (eq != std::wstring::npos) {
        std::wstring name = entry.substr(0, eq);
        std::wstring val = entry.substr(eq + 1);
        bool enabled = (val == L"1");
        m_savedStates.push_back({name, enabled});
      }
      p += entry.length() + 1;
    }
  }
}

void PluginManager::SaveSettings(const std::wstring &iniPath) {
  if (iniPath.empty())
    return;

  for (const auto &plugin : m_plugins) {
    std::wstring val = plugin.isEnabled ? L"1" : L"0";
    WritePrivateProfileStringW(L"Plugins", plugin.name.c_str(), val.c_str(),
                               iniPath.c_str());
  }
}

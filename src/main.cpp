#ifndef UNICODE
#define UNICODE
#endif

#include "ExplorerView.h"

#include "ImageViewerWindow.h" // Changed include
#include "MoveHelper.h"
#include "PluginManager.h"
#include "TinyEXIF/TinyEXIF.h"
#include "resource.h"
#include <commctrl.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <filesystem>
#include <fstream>
#include <knownfolders.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <string>
#include <vector>
#include <windows.h>

#include <windows.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "dwrite.lib")

// Global variables
HINSTANCE g_hInst = NULL;
HWND g_hWnd = NULL;
HWND g_hLog = NULL; // Log Window
PluginManager g_pluginManager;
ExplorerView g_explorerView;
// ImageView g_imageView; // Removed
// ImageView g_imageView; // Removed

void AppLog(const std::wstring &msg) {
  if (!g_hLog)
    return;
  std::wstring out = msg + L"\r\n";
  int len = GetWindowTextLength(g_hLog);
  SendMessage(g_hLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
  SendMessage(g_hLog, EM_REPLACESEL, 0, (LPARAM)out.c_str());
}

// Resources
ID2D1Factory *pD2DFactory = NULL;
IWICImagingFactory *pWICFactoryMain = NULL;
ID2D1HwndRenderTarget *pRenderTarget = NULL;

// State
// enum AppState { STATE_EXPLORER, STATE_VIEWER }; // Removed
// AppState g_state = STATE_EXPLORER; // Removed
// AppState g_state = STATE_EXPLORER; // Removed

std::wstring GetIniPath() {
  wchar_t buffer[MAX_PATH];
  if (GetModuleFileNameW(NULL, buffer, MAX_PATH) > 0) {
    std::filesystem::path p(buffer);
    p.replace_extension(L".ini");
    return p.wstring();
  }
  return L"";
}

void LoadSettings() {
  std::wstring iniPath = GetIniPath();
  if (iniPath.empty())
    return;

  // Load folders
  for (int i = 0; i <= 9; ++i) {
    wchar_t keyName[32];
    swprintf_s(keyName, L"Folder%d", i);
    wchar_t pathBuf[MAX_PATH] = {0};
    GetPrivateProfileStringW(L"Folders", keyName, L"", pathBuf, MAX_PATH,
                             iniPath.c_str());
    MoveHelper::Instance().SetFolder(i, pathBuf);
  }
  // Load KeyAction
  int action =
      GetPrivateProfileIntW(L"Settings", L"KeyAction", 0, iniPath.c_str());
  MoveHelper::Instance().SetKeyAction((action == 1) ? KA_COPY : KA_MOVE);

  // Load Last Folder
  wchar_t lastFolder[MAX_PATH] = {0};
  GetPrivateProfileStringW(L"Settings", L"LastFolder", L"", lastFolder,
                           MAX_PATH, iniPath.c_str());
  if (wcslen(lastFolder) > 0) {
    AppLog(L"Restoring last folder: " + std::wstring(lastFolder));
    g_explorerView.NavigateTo(lastFolder);
  } else {
    AppLog(L"No last folder saved.");
  }

  // Load Magnifier
  int magEnabled =
      GetPrivateProfileIntW(L"Settings", L"MagEnabled", 0, iniPath.c_str());
  int magSize =
      GetPrivateProfileIntW(L"Settings", L"MagSize", 200, iniPath.c_str());
  int magZoom =
      GetPrivateProfileIntW(L"Settings", L"MagZoom", 2, iniPath.c_str());
  auto &iv = ImageViewerWindow::Instance().GetImageView();
  iv.SetMagnifier(magEnabled != 0);
  iv.SetMagnifierSettings((float)magSize, (float)magZoom);
}

void SaveSettings() {
  std::wstring iniPath = GetIniPath();
  if (iniPath.empty())
    return;

  // Save folders
  for (int i = 0; i <= 9; ++i) {
    wchar_t keyName[32];
    swprintf_s(keyName, L"Folder%d", i);
    WritePrivateProfileStringW(L"Folders", keyName,
                               MoveHelper::Instance().GetFolder(i).c_str(),
                               iniPath.c_str());
  }

  // Save KeyAction
  int action = (MoveHelper::Instance().GetKeyAction() == KA_COPY) ? 1 : 0;
  wchar_t buf[16];
  swprintf_s(buf, L"%d", action);
  WritePrivateProfileStringW(L"Settings", L"KeyAction", buf, iniPath.c_str());

  // Save Plugin Settings
  g_pluginManager.SaveSettings(iniPath);

  // Save Last Folder
  WritePrivateProfileStringW(L"Settings", L"LastFolder",
                             g_explorerView.GetCurrentFolder().c_str(),
                             iniPath.c_str());

  // Save Magnifier
  auto &iv = ImageViewerWindow::Instance().GetImageView();
  swprintf_s(buf, L"%d", iv.IsMagnifierEnabled() ? 1 : 0);
  WritePrivateProfileStringW(L"Settings", L"MagEnabled", buf, iniPath.c_str());

  swprintf_s(buf, L"%.0f", iv.GetMagnifierSize());
  WritePrivateProfileStringW(L"Settings", L"MagSize", buf, iniPath.c_str());

  swprintf_s(buf, L"%.0f", iv.GetMagnifierZoom());
  WritePrivateProfileStringW(L"Settings", L"MagZoom", buf, iniPath.c_str());
}

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK ConfigDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK PluginListDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK AboutDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK PluginSettingsDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK PropertiesDlgProc(HWND, UINT, WPARAM,
                                   LPARAM); // Forward declaration

template <class T> void SafeRelease(T **ppT) {
  if (*ppT) {
    (*ppT)->Release();
    *ppT = NULL;
  }
}

void CreateDeviceResources(HWND hwnd);
void DiscardDeviceResources();
void OnPaint(HWND hwnd);
void OnResize(HWND hwnd);
// void ToggleView(bool toViewer); // Removed

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
  g_hInst = hInstance;
  HRESULT hr =
      CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

  if (SUCCEEDED(hr)) {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);
    CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                     IID_PPV_ARGS(&pWICFactoryMain));
    InitCommonControls(); // For ExplorerView

    const wchar_t CLASS_NAME[] = L"VisionImageViewerWindow";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = MAKEINTRESOURCE(IDR_MAINMENU);

    RegisterClassW(&wc);

    wchar_t szTitle[128];
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, 128);

    g_hWnd = CreateWindowExW(0, CLASS_NAME, szTitle,
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                             CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

    if (g_hWnd) {
      // Create Log Window
      g_hLog = CreateWindowEx(0, L"EDIT", NULL,
                              WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                  ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                              0, 0, 0, 0, g_hWnd, (HMENU)8001, hInstance, NULL);
      SendMessage(g_hLog, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),
                  0);
      AppLog(L"App Started.");

      // Load Plugins after log is ready
      g_pluginManager.LoadPlugins();

      // Init Views
      g_explorerView.Create(g_hWnd, g_hInst);

      // Init Image Viewer Window Singleton
      ImageViewerWindow::Instance().Init(g_hInst, pD2DFactory, pWICFactoryMain,
                                         &g_pluginManager);

      // Load Settings (must affect views and logs)
      LoadSettings();

      ShowWindow(g_hWnd, nCmdShow);
      UpdateWindow(g_hWnd);
      OnResize(g_hWnd); // Force layout

      // Callbacks
      g_explorerView.OnFileOpen = [](const std::wstring &path,
                                     const std::vector<std::wstring> &list) {
        AppLog(L"Opening: " + path);
        ImageViewerWindow::Instance().Show(path, list);
      };

      MSG msg = {};
      while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }

    SafeRelease(&pRenderTarget);
    SafeRelease(&pD2DFactory);
    SafeRelease(&pWICFactoryMain);
    CoUninitialize();
  }
  return 0;
}

// ToggleView removed

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  // ExplorerView might handle some notifications
  if (g_explorerView.HandleMessage(uMsg, wParam, lParam)) {
    // If it returns TRUE for some specific messages it might mean it's
    // handled, but usually we want to continue for menus etc.
  }

  switch (uMsg) {
  case WM_INITMENUPOPUP: {
    HMENU hMenu = (HMENU)wParam;
    // View modes
    ExplorerViewMode mode = g_explorerView.GetViewMode();
    CheckMenuItem(hMenu, IDM_VIEW_THUMB,
                  (mode == EV_THUMBNAILS) ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, IDM_VIEW_DETAILS,
                  (mode == EV_DETAILS) ? MF_CHECKED : MF_UNCHECKED);
  } break;

  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDM_VIEW_THUMB:
      g_explorerView.SetViewMode(EV_THUMBNAILS);
      break;
    case IDM_VIEW_DETAILS:
      g_explorerView.SetViewMode(EV_DETAILS);
      break;
    case IDM_VIEW_REFRESH:
      g_explorerView.HandleMessage(WM_COMMAND, wParam, lParam);
      break;
    case IDM_CTX_RENAME:
      g_explorerView.HandleMessage(WM_COMMAND, wParam, lParam);
      break;
    case IDM_FILE_OPEN: {
      IFileOpenDialog *pFileOpen;
      if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL,
                                     CLSCTX_INPROC_SERVER,
                                     IID_PPV_ARGS(&pFileOpen)))) {
        if (SUCCEEDED(pFileOpen->Show(hwnd))) {
          IShellItem *pItem;
          if (SUCCEEDED(pFileOpen->GetResult(&pItem))) {
            PWSTR pszFilePath;
            if (SUCCEEDED(
                    pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))) {
              std::wstring pathStr = pszFilePath;
              AppLog(L"Opening: " + pathStr);
              ImageViewerWindow::Instance().Show(pathStr);
              CoTaskMemFree(pszFilePath);
            }
            pItem->Release();
          }
        }
        pFileOpen->Release();
      }
    } break;

    case IDM_FILE_EXIT:
      SaveSettings();
      DestroyWindow(hwnd);
      break;

    case IDM_CONFIG:
      DialogBox(g_hInst, MAKEINTRESOURCE(IDD_CONFIG), hwnd, ConfigDlgProc);
      break;

    case IDM_CONFIG_FOLDERS:
      if (DialogBox(g_hInst, MAKEINTRESOURCE(IDD_FOLDER_SETTINGS), hwnd,
                    MoveHelper::FolderSettingsDlgProc) == IDOK) {
        SaveSettings();
      }
      break;

    case IDM_HELP_PLUGINS:
      DialogBox(g_hInst, MAKEINTRESOURCE(IDD_PLUGIN_SETTINGS), hwnd,
                PluginSettingsDlgProc);
      break;

    case IDM_HELP_ABOUT:
      DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUT), hwnd, AboutDlgProc);
      break;
    }
    return 0;

  case WM_PAINT:
    OnPaint(hwnd);
    return 0;

  case WM_SIZE:
    OnResize(hwnd);
    return 0;

    // Key handlers for viewer removed
    return 0;

  case WM_DESTROY:
    SaveSettings();
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void OnPaint(HWND hwnd) {
  // Always Explorer + Log
  // Just validate to prevent infinite loop, as ExplorerView and Edit Control
  // do their own painting
  PAINTSTRUCT ps;
  BeginPaint(hwnd, &ps);
  EndPaint(hwnd, &ps);
}

void OnResize(HWND hwnd) {
  RECT rc;
  GetClientRect(hwnd, &rc);
  int width = rc.right - rc.left;
  int height = rc.bottom - rc.top;

  int logHeight = 100;
  int viewHeight = height - logHeight;
  if (viewHeight < 0)
    viewHeight = 0;

  if (g_hLog) {
    MoveWindow(g_hLog, 0, viewHeight, width, logHeight, TRUE);
  }

  // Always resize Explorer View
  g_explorerView.Resize(0, 0, width, viewHeight);
}

void CreateDeviceResources(HWND hwnd) {
  if (!pRenderTarget) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
    pD2DFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd, size), &pRenderTarget);
    // g_imageView.SetRenderTarget(pRenderTarget); // Removed
  }
}

// Dialog Procedures
INT_PTR CALLBACK ConfigDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                               LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG: {
    // Load settings
    SetDlgItemInt(hDlg, IDC_EDIT_THUMB_SIZE, 96, FALSE);
    CheckDlgButton(hDlg, IDC_RADIO_MOVE,
                   (MoveHelper::Instance().GetKeyAction() == KA_MOVE)
                       ? BST_CHECKED
                       : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_RADIO_COPY,
                   (MoveHelper::Instance().GetKeyAction() == KA_COPY)
                       ? BST_CHECKED
                       : BST_UNCHECKED);

    // Magnifier Settings
    auto &iv = ImageViewerWindow::Instance().GetImageView();
    CheckDlgButton(hDlg, IDC_CHECK_ENABLE_MAG,
                   iv.IsMagnifierEnabled() ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(hDlg, IDC_EDIT_MAG_SIZE, (UINT)iv.GetMagnifierSize(), FALSE);
    SetDlgItemInt(hDlg, IDC_EDIT_MAG_ZOOM, (UINT)iv.GetMagnifierZoom(), FALSE);

    return (INT_PTR)TRUE;
  }

  case WM_COMMAND:
    if (LOWORD(wParam) == IDC_BTN_PLUGIN_CONF) {
      DialogBox(g_hInst, MAKEINTRESOURCE(IDD_PLUGIN_SETTINGS), hDlg,
                PluginSettingsDlgProc);
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDM_CONFIG_FOLDERS) {
      if (DialogBox(g_hInst, MAKEINTRESOURCE(IDD_FOLDER_SETTINGS), hDlg,
                    MoveHelper::FolderSettingsDlgProc) == IDOK) {
        SaveSettings();
      }
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDOK) {
      // Save settings
      auto &iv = ImageViewerWindow::Instance().GetImageView();

      bool spread = IsDlgButtonChecked(hDlg, IDC_RADIO_SPREAD) == BST_CHECKED;
      iv.SetViewMode(spread);

      // Magnifier settings
      bool magEnabled =
          IsDlgButtonChecked(hDlg, IDC_CHECK_ENABLE_MAG) == BST_CHECKED;
      iv.SetMagnifier(magEnabled);
      UINT magSize = GetDlgItemInt(hDlg, IDC_EDIT_MAG_SIZE, NULL, FALSE);
      UINT magZoom = GetDlgItemInt(hDlg, IDC_EDIT_MAG_ZOOM, NULL, FALSE);
      iv.SetMagnifierSettings((float)magSize, (float)magZoom);

      KeyAction action =
          (IsDlgButtonChecked(hDlg, IDC_RADIO_MOVE) == BST_CHECKED) ? KA_MOVE
                                                                    : KA_COPY;
      MoveHelper::Instance().SetKeyAction(action);

      SaveSettings();

      EndDialog(hDlg, LOWORD(wParam));
      return (INT_PTR)TRUE;

    } else if (LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, LOWORD(wParam));
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

// Plugin Settings Dialog
void RefreshPluginList(HWND hDlg) {
  HWND hList = GetDlgItem(hDlg, IDC_LIST_PLUGIN_ORDER);
  SendMessage(hList, LB_RESETCONTENT, 0, 0);
  const auto &plugins = g_pluginManager.GetPlugins();
  for (int i = 0; i < (int)plugins.size(); ++i) {
    std::wstring str =
        (plugins[i].isEnabled ? L"[x] " : L"[ ] ") + plugins[i].name;
    if (plugins[i].isInternal)
      str += L" (Internal)";
    SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)str.c_str());
    SendMessage(hList, LB_SETITEMDATA, i, (LPARAM)i); // Map index
  }
}

INT_PTR CALLBACK PluginSettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                       LPARAM lParam) {
  static int s_selIndex = -1;
  switch (message) {
  case WM_INITDIALOG:
    RefreshPluginList(hDlg);
    return (INT_PTR)TRUE;

  case WM_COMMAND:
    if (LOWORD(wParam) == IDC_LIST_PLUGIN_ORDER) {
      if (HIWORD(wParam) == LBN_SELCHANGE) {
        HWND hList = GetDlgItem(hDlg, IDC_LIST_PLUGIN_ORDER);
        int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
        if (sel != LB_ERR) {
          s_selIndex = sel;
          const auto &plugins = g_pluginManager.GetPlugins();
          CheckDlgButton(hDlg, IDC_CHECK_ENABLE,
                         plugins[sel].isEnabled ? BST_CHECKED : BST_UNCHECKED);
        }
      }
    } else if (LOWORD(wParam) == IDC_CHECK_ENABLE) {
      if (s_selIndex != -1) {
        bool checked =
            IsDlgButtonChecked(hDlg, IDC_CHECK_ENABLE) == BST_CHECKED;
        g_pluginManager.SetPluginEnabled(s_selIndex, checked);
        RefreshPluginList(hDlg);
        SendMessage(GetDlgItem(hDlg, IDC_LIST_PLUGIN_ORDER), LB_SETCURSEL,
                    s_selIndex, 0);
      }
    } else if (LOWORD(wParam) == IDC_BTN_UP) {
      if (s_selIndex > 0) {
        g_pluginManager.MovePluginUp(s_selIndex);
        s_selIndex--;
        RefreshPluginList(hDlg);
        SendMessage(GetDlgItem(hDlg, IDC_LIST_PLUGIN_ORDER), LB_SETCURSEL,
                    s_selIndex, 0);
      }
    } else if (LOWORD(wParam) == IDC_BTN_DOWN) {
      const auto &plugins = g_pluginManager.GetPlugins();
      if (s_selIndex != -1 && s_selIndex < (int)plugins.size() - 1) {
        g_pluginManager.MovePluginDown(s_selIndex);
        s_selIndex++;
        RefreshPluginList(hDlg);
        SendMessage(GetDlgItem(hDlg, IDC_LIST_PLUGIN_ORDER), LB_SETCURSEL,
                    s_selIndex, 0);
      }
    } else if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, LOWORD(wParam));
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

INT_PTR CALLBACK PluginListDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                   LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG: {
    HWND hList = GetDlgItem(hDlg, IDC_LIST_PLUGINS);
    const auto &plugins = g_pluginManager.GetPlugins();
    for (const auto &p : plugins) {
      if (!p.isInternal) {
        std::wstring str = p.path + (p.is64Bit ? L" (64-bit)" : L" (32-bit)");
        SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)str.c_str());
      }
    }
  }
    return (INT_PTR)TRUE;
  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, LOWORD(wParam));
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

// Helper
static std::wstring LoadStr(UINT id) {
  wchar_t buf[1024];
  if (LoadStringW(GetModuleHandle(NULL), id, buf, 1024))
    return buf;
  return L"";
}

// EXIF Properties Dialog Protocol
// EXIF Properties Dialog Protocol
// EXIF Properties Dialog Protocol
INT_PTR CALLBACK PropertiesDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                   LPARAM lParam) {
  static std::wstring s_imagePath;
  switch (message) {
  case WM_INITDIALOG: {
    wchar_t *path = (wchar_t *)lParam;
    if (path)
      s_imagePath = path;
    HWND hList = GetDlgItem(hDlg, IDC_LIST_PROPERTIES);
    SendMessage(hList, LB_RESETCONTENT, 0, 0);

    // Helpers
    auto AddHeader = [&](const wchar_t *title) {
      SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)L"");
      std::wstring header = L"[" + std::wstring(title) + L"]";
      SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)header.c_str());
    };

    auto AddLine = [&](const wchar_t *label, const std::wstring &val) {
      if (!val.empty()) {
        std::wstring line = std::wstring(label) + L": " + val;
        SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)line.c_str());
      }
    };

    auto ToWString = [](const std::string &str) -> std::wstring {
      if (str.empty())
        return L"";
      int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
      if (len <= 0)
        return L"";
      std::vector<wchar_t> buf(len);
      MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buf.data(), len);
      return buf.data();
    };

    // Load File Data for TinyEXIF
    TinyEXIF::EXIFInfo exif;
    {
      // Read file into buffer to support Unicode path via _wfopen
      FILE *f = _wfopen(s_imagePath.c_str(), L"rb");
      if (f) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (size > 0) {
          std::vector<uint8_t> data(size);
          if (fread(data.data(), 1, size, f) == (size_t)size) {
            exif.parseFrom(data.data(), (unsigned int)size);
          }
        }
        fclose(f);
      }
    }

    std::wstring p_maker, p_model, p_software, p_time;
    std::wstring p_fnumber, p_exposure, p_iso, p_focal, p_flash, p_bias;
    std::wstring p_desc, p_usercomment;
    std::wstring p_lat, p_lon, p_alt;
    std::wstring p_lens, p_focal35;
    std::wstring p_exprog, p_meter, p_wb, p_light;
    std::wstring p_copyright, p_orient;

    // WIC Data Fallback
    UINT wicWidth = 0, wicHeight = 0;
    double wicDpiX = 0, wicDpiY = 0;
    UINT wicBitDepth = 0;
    std::wstring wicFormatName;
    if (pWICFactoryMain) {
      IWICBitmapDecoder *pDecoder = NULL;
      if (SUCCEEDED(pWICFactoryMain->CreateDecoderFromFilename(
              s_imagePath.c_str(), NULL, GENERIC_READ,
              WICDecodeMetadataCacheOnDemand, &pDecoder))) {

        IWICBitmapDecoderInfo *pDecoderInfo = NULL;
        if (SUCCEEDED(pDecoder->GetDecoderInfo(&pDecoderInfo))) {
          UINT actual = 0;
          pDecoderInfo->GetFriendlyName(0, NULL, &actual);
          if (actual > 0) {
            std::vector<wchar_t> nameBuf(actual);
            pDecoderInfo->GetFriendlyName(actual, nameBuf.data(), &actual);
            wicFormatName = nameBuf.data();
          }
          pDecoderInfo->Release();
        }

        IWICBitmapFrameDecode *pFrame = NULL;
        if (SUCCEEDED(pDecoder->GetFrame(0, &pFrame))) {
          pFrame->GetSize(&wicWidth, &wicHeight);
          pFrame->GetResolution(&wicDpiX, &wicDpiY);

          WICPixelFormatGUID pixelFormat;
          if (SUCCEEDED(pFrame->GetPixelFormat(&pixelFormat))) {
            IWICComponentInfo *pComponentInfo = NULL;
            if (SUCCEEDED(pWICFactoryMain->CreateComponentInfo(
                    pixelFormat, &pComponentInfo))) {
              IWICPixelFormatInfo *pPixelFormatInfo = NULL;
              if (SUCCEEDED(pComponentInfo->QueryInterface(
                      IID_IWICPixelFormatInfo, (void **)&pPixelFormatInfo))) {
                pPixelFormatInfo->GetBitsPerPixel(&wicBitDepth);
                pPixelFormatInfo->Release();
              }
              pComponentInfo->Release();
            }
          }
          pFrame->Release();
        }
        pDecoder->Release();
      }
    }

    // Extract basic tags
    if (exif.Fields) {
      p_maker = ToWString(exif.Make);
      p_model = ToWString(exif.Model);
      p_software = ToWString(exif.Software);
      p_time = ToWString(exif.DateTimeOriginal);
      if (p_time.empty())
        p_time = ToWString(exif.DateTime);
      if (!p_time.empty() && !exif.SubSecTimeOriginal.empty()) {
        p_time += L"." + ToWString(exif.SubSecTimeOriginal);
      }

      p_copyright = ToWString(exif.Copyright);

      // Description
      p_desc = ToWString(exif.ImageDescription);

      // Dimensions
      // Use EXIF width/height if available, otherwise 0
      int width = exif.ImageWidth;
      int height = exif.ImageHeight;

      if (exif.FNumber > 0)
        p_fnumber = std::to_wstring(exif.FNumber);
      if (exif.ExposureTime > 0) {
        if (exif.ExposureTime < 1.0) {
          p_exposure = L"1/" + std::to_wstring((int)(1.0 / exif.ExposureTime));
        } else {
          p_exposure = std::to_wstring(exif.ExposureTime);
        }
      }
      if (exif.ISOSpeedRatings > 0)
        p_iso = std::to_wstring(exif.ISOSpeedRatings);
      if (exif.FocalLength > 0)
        p_focal = std::to_wstring(exif.FocalLength) + L" mm";

      // Exposure Bias (Always show, even if 0.00)
      {
        wchar_t buf[64];
        swprintf_s(buf, L"%.2f EV", exif.ExposureBiasValue);
        p_bias = buf;
      }

      // Flash
      if (exif.Flash & 1)
        p_flash = L"\u767a\u5149"; // "発光"
      else
        p_flash = L"\u767a\u5149\u305b\u305a"; // "発光せず"

      // Orientation
      switch (exif.Orientation) {
      case 1:
        p_orient = L"\u6a19\u6e96 (0\u00b0)";
        break; // "標準 (0°)"
      case 3:
        p_orient = L"180\u00b0 \u56de\u8ee2";
        break; // "180° 回転"
      case 6:
        p_orient = L"90\u00b0 \u6642\u8a08\u56de\u308a\u56de\u8ee2";
        break; // "90° 時計回り回転"
      case 8:
        p_orient = L"270\u00b0 \u6642\u8a08\u56de\u308a\u56de\u8ee2";
        break; // "270° 時計回り回転"
      }

      // Exposure Program
      switch (exif.ExposureProgram) {
      case 1:
        p_exprog = L"Manual";
        break;
      case 2:
        p_exprog = L"Normal program";
        break;
      case 3:
        p_exprog = L"Aperture priority";
        break;
      case 4:
        p_exprog = L"Shutter priority";
        break;
      case 5:
        p_exprog = L"Creative program";
        break;
      case 6:
        p_exprog = L"Action program";
        break;
      case 7:
        p_exprog = L"Portrait mode";
        break;
      case 8:
        p_exprog = L"Landscape mode";
        break;
      }

      // Metering Mode
      switch (exif.MeteringMode) {
      case 1:
        p_meter = L"Average";
        break;
      case 2:
        p_meter = L"CenterWeightedAverage";
        break;
      case 3:
        p_meter = L"Spot";
        break;
      case 4:
        p_meter = L"MultiSpot";
        break;
      case 5:
        p_meter = L"Pattern";
        break;
      case 6:
        p_meter = L"Partial";
        break;
      case 255:
        p_meter = L"Other";
        break;
      }

      // White Balance
      if (exif.WhiteBalance == 0)
        p_wb = L"Auto";
      else if (exif.WhiteBalance == 1)
        p_wb = L"Manual";

      // Light Source
      switch (exif.LightSource) {
      case 1:
        p_light = L"Daylight";
        break;
      case 2:
        p_light = L"Fluorescent";
        break;
      case 3:
        p_light = L"Tungsten (incandescent light)";
        break;
      case 4:
        p_light = L"Flash";
        break;
      case 9:
        p_light = L"Fine weather";
        break;
      case 10:
        p_light = L"Cloudy weather";
        break;
      case 11:
        p_light = L"Shade";
        break;
      case 12:
        p_light = L"Daylight fluorescent (D 5700-7100K)";
        break;
      case 13:
        p_light = L"Day white fluorescent (N 4600-5400K)";
        break;
      case 14:
        p_light = L"Cool white fluorescent (W 3900-4500K)";
        break;
      case 15:
        p_light = L"White fluorescent (WW 3200-3700K)";
        break;
      case 20:
        p_light = L"D55";
        break;
      case 21:
        p_light = L"D65";
        break;
      case 22:
        p_light = L"D75";
        break;
      case 23:
        p_light = L"D50";
        break;
      case 24:
        p_light = L"ISO studio tungsten";
        break;
      }

      // GPS
      if (exif.GeoLocation.hasLatLon()) {
        wchar_t buf[64];
        swprintf_s(buf, L"%.6f", exif.GeoLocation.Latitude);
        p_lat = buf;
        swprintf_s(buf, L"%.6f", exif.GeoLocation.Longitude);
        p_lon = buf;
      }
      if (exif.GeoLocation.hasAltitude()) {
        p_alt = std::to_wstring(exif.GeoLocation.Altitude) + L" m";
      }

      // Advanced
      if (exif.LensInfo.FocalLengthIn35mm > 0)
        p_focal35 = std::to_wstring(exif.LensInfo.FocalLengthIn35mm) + L" mm";

      p_lens = ToWString(exif.LensInfo.Model);
    }

    // 1. File (Moved to top)
    AddHeader(L"\u30d5\u30a1\u30a4\u30eb"); // "ファイル"
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetFileAttributesExW(s_imagePath.c_str(), GetFileExInfoStandard,
                             &fileInfo)) {
      std::wstring fname =
          std::filesystem::path(s_imagePath).filename().wstring();
      std::wstring ext =
          std::filesystem::path(s_imagePath).extension().wstring();

      AddLine(L"\u540d\u524d", fname); // "名前"
      AddLine(L"\u7a2e\u985e", ext);   // "種類"
      AddLine(L"\u30d5\u30a9\u30eb\u30c0\u30fc",
              std::filesystem::path(s_imagePath)
                  .parent_path()
                  .wstring()); // "フォルダー"

      LARGE_INTEGER size;
      size.HighPart = fileInfo.nFileSizeHigh;
      size.LowPart = fileInfo.nFileSizeLow;
      wchar_t sizeBuf[64];
      swprintf_s(sizeBuf, L"%lld bytes", size.QuadPart);
      AddLine(L"\u30b5\u30a4\u30ba", sizeBuf); // "サイズ"

      auto FormatDate = [](FILETIME ft) -> std::wstring {
        SYSTEMTIME stUTC, stLocal;
        FileTimeToSystemTime(&ft, &stUTC);
        SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
        wchar_t b[128];
        swprintf_s(b, L"%04d/%02d/%02d %02d:%02d", stLocal.wYear,
                   stLocal.wMonth, stLocal.wDay, stLocal.wHour,
                   stLocal.wMinute);
        return b;
      };

      AddLine(L"\u4f5c\u6210\u65e5\u6642",
              FormatDate(fileInfo.ftCreationTime)); // "作成日時"
      AddLine(L"\u66f4\u65b0\u65e5\u6642",
              FormatDate(fileInfo.ftLastWriteTime)); // "更新日時"
    }

    // 2. Image Format (WIC Data)
    AddHeader(
        L"\u753b\u50cf\u30d5\u30a9\u30fc\u30de\u30c3\u30c8"); // "画像フォーマット"
    if (wicWidth > 0 && wicHeight > 0) {
      wchar_t buf[256];
      swprintf_s(buf, L"%d x %d", wicWidth, wicHeight);
      AddLine(L"\u5927\u304d\u3055 (\u30d4\u30af\u30bb\u30eb)",
              buf); // "大きさ (ピクセル)"
      AddLine(L"\u5e45",
              std::to_wstring(wicWidth) + L" \u30d4\u30af\u30bb\u30eb"); // "幅"
      AddLine(L"\u9ad8\u3055", std::to_wstring(wicHeight) +
                                   L" \u30d4\u30af\u30bb\u30eb"); // "高さ"
    }
    if (wicDpiX > 0 && wicDpiY > 0) {
      wchar_t buf[64];
      swprintf_s(buf, L"%.0f dpi", wicDpiX);
      AddLine(L"\u6c34\u5e73\u89e3\u50cf\u5ea6", buf); // "水平解像度"
      swprintf_s(buf, L"%.0f dpi", wicDpiY);
      AddLine(L"\u5782\u76f4\u89e3\u50cf\u5ea6", buf); // "垂直解像度"
    }
    if (wicBitDepth > 0) {
      AddLine(L"\u30d3\u30c3\u30c8\u306e\u6df1\u3055",
              std::to_wstring(wicBitDepth)); // "ビットの深さ"
    }
    if (!wicFormatName.empty()) {
      AddLine(L"\u30d5\u30a9\u30fc\u30de\u30c3\u30c8",
              wicFormatName); // "フォーマット"
    }

    // 3. EXIF Data (Grouped)
    if (exif.Fields == 0) {
      AddHeader(L"EXIF");
      AddLine(L"\u30e1\u30bf\u30c7\u30fc\u30bf",
              L"\u306a\u3057"); // "メタデータ: なし"
    } else {
      // EXIF: Description
      if (!p_desc.empty()) {
        AddHeader(L"EXIF: \u8aac\u660e"); // "EXIF: 説明"
        AddLine(L"\u30bf\u30a4\u30c8\u30eb", p_desc);
      }
      AddLine(L"\u8457\u4f5c\u6a29", p_copyright); // "著作権"

      // EXIF: Image
      bool hasImageInfo = (exif.ImageWidth > 0) || (exif.BitsPerSample > 0) ||
                          (exif.ColorSpace > 0 && exif.ColorSpace != 0xFFFF) ||
                          (exif.XResolution > 0);
      if (hasImageInfo) {
        AddHeader(L"EXIF: \u30a4\u30e1\u30fc\u30b8"); // "EXIF: イメージ"
        if (exif.ImageWidth > 0 && exif.ImageHeight > 0) {
          wchar_t buf[256];
          swprintf_s(buf, L"%d x %d", exif.ImageWidth, exif.ImageHeight);
          AddLine(L"\u5927\u304d\u3055 (\u30d4\u30af\u30bb\u30eb)", buf);
          AddLine(L"\u5e45", std::to_wstring(exif.ImageWidth) +
                                 L" \u30d4\u30af\u30bb\u30eb");
          AddLine(L"\u9ad8\u3055", std::to_wstring(exif.ImageHeight) +
                                       L" \u30d4\u30af\u30bb\u30eb");
        }
        if (exif.BitsPerSample > 0) {
          AddLine(L"\u30d3\u30c3\u30c8\u306e\u6df1\u3055",
                  std::to_wstring(exif.BitsPerSample));
        }
        if (exif.ColorSpace == 1)
          AddLine(L"\u8272\u7a7a\u9593", L"sRGB");
        else if (exif.ColorSpace == 2)
          AddLine(L"\u8272\u7a7a\u9593", L"Adobe RGB");
        else if (exif.ColorSpace == 0xFFFF)
          AddLine(L"\u8272\u7a7a\u9593", L"Uncalibrated");

        AddLine(L"\u65b9\u5411", p_orient); // "方向"

        if (exif.XResolution > 0 && exif.YResolution > 0) {
          std::wstring unit = L"";
          if (exif.ResolutionUnit == 2)
            unit = L" dpi";
          else if (exif.ResolutionUnit == 3)
            unit = L" dpcm";
          wchar_t buf[64];
          swprintf_s(buf, L"%.0f%s", exif.XResolution, unit.c_str());
          AddLine(L"\u6c34\u5e73\u89e3\u50cf\u5ea6", buf);
          swprintf_s(buf, L"%.0f%s", exif.YResolution, unit.c_str());
          AddLine(L"\u5782\u76f4\u89e3\u50cf\u5ea6", buf);
        }
      }

      // EXIF: Camera
      bool hasCameraInfo = !p_maker.empty() || !p_model.empty() ||
                           !p_fnumber.empty() || !p_exposure.empty() ||
                           !p_iso.empty() || !p_exprog.empty() ||
                           !p_bias.empty();
      if (hasCameraInfo) {
        AddHeader(L"EXIF: \u30ab\u30e1\u30e9"); // "EXIF: カメラ"
        AddLine(L"\u4f5c\u6210\u8005", p_maker);
        AddLine(L"\u30e2\u30c7\u30eb", p_model);
        AddLine(L"\u7d5e\u308a\u5024", p_fnumber);
        AddLine(L"\u9732\u51fa\u6642\u9593", p_exposure);
        AddLine(L"\u9732\u51fa\u30d7\u30ed\u30b0\u30e9\u30e0",
                p_exprog); // "露出プログラム"
        AddLine(L"\u9732\u51fa\u88dc\u6b63", p_bias);
        AddLine(L"ISO\u611f\u5ea6", p_iso);
        AddLine(L"\u6e2c\u5149\u30e2\u30fc\u30c9", p_meter); // "測光モード"
        AddLine(L"\u30db\u30ef\u30a4\u30c8\u30d0\u30e9\u30f3\u30b9",
                p_wb);                     // "ホワイトバランス"
        AddLine(L"\u5149\u6e90", p_light); // "光源"
        AddLine(L"\u7126\u70b9\u8ddd\u96e2", p_focal);
        AddLine(L"\u30d5\u30e9\u30c3\u30b7\u30e5", p_flash);
        AddLine(L"\u64ae\u5f71\u65e5\u6642", p_time);
        AddLine(L"\u30bd\u30d5\u30c8\u30a6\u30a7\u30a2", p_software);
      }

      // EXIF: Advanced
      if (!p_lens.empty() || !p_focal35.empty()) {
        AddHeader(L"EXIF: \u8a73\u7d30\u306a\u5199\u771f");
        AddLine(L"\u30ec\u30f3\u30ba\u306e\u30e2\u30c7\u30eb", p_lens);
        AddLine(L"35mm\u7126\u70b9\u8ddd\u96e2", p_focal35);
      }

      // EXIF: GPS
      if (!p_lat.empty() || !p_lon.empty()) {
        AddHeader(L"EXIF: GPS");
        AddLine(L"\u7def\u5ea6", p_lat);
        AddLine(L"\u7d4c\u5ea6", p_lon);
        AddLine(L"\u9ad8\u5ea6", p_alt);
      }
    }
  }
    return (INT_PTR)TRUE;
  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, LOWORD(wParam));
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

// ... handlers ...

INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                              LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG:
    return (INT_PTR)TRUE;
  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, LOWORD(wParam));
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

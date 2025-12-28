#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "MoveHelper.h"
#include "resource.h"
#include <filesystem>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <vector>

#pragma comment(lib, "shell32.lib")

MoveHelper::ConfirmState MoveHelper::s_confirmState;

MoveHelper &MoveHelper::Instance() {
  static MoveHelper instance;
  return instance;
}

MoveHelper::MoveHelper() {}

void MoveHelper::SetKeyAction(KeyAction action) { m_keyAction = action; }
KeyAction MoveHelper::GetKeyAction() const { return m_keyAction; }

void MoveHelper::SetFolder(int key, const std::wstring &path) {
  m_folderMap[key] = path;
}

std::wstring MoveHelper::GetFolder(int key) {
  if (m_folderMap.find(key) != m_folderMap.end())
    return m_folderMap[key];
  return L"";
}

// Helper to load string
static std::wstring LoadStr(UINT id) {
  wchar_t buf[1024];
  if (LoadStringW(GetModuleHandle(NULL), id, buf, 1024))
    return buf;
  return L"";
}

bool MoveHelper::RequestMove(HWND hParent,
                             const std::vector<std::wstring> &filePaths,
                             int key, IWICImagingFactory *pFactory) {
  if (filePaths.empty())
    return false;

  std::wstring destFolder = GetFolder(key);
  if (destFolder.empty()) {
    MessageBoxW(hParent, LoadStr(IDS_ERROR_NO_FOLDER).c_str(), L"Info", MB_OK);
    return false;
  }

  if (!std::filesystem::exists(destFolder)) {
    MessageBoxW(hParent, LoadStr(IDS_ERROR_PATH).c_str(), L"Error", MB_OK);
    return false;
  }

  // Prepare Confirmation State
  s_confirmState.sourcePaths = filePaths;
  s_confirmState.destFolder = destFolder;
  s_confirmState.pFactory = pFactory;
  s_confirmState.hIcon = NULL;

  // Show Dialog
  if (DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_MOVE_CONFIRM),
                      hParent, MoveConfirmDlgProc, 0) == IDOK) {

    std::wstring from;
    for (const auto &path : filePaths) {
      from += path;
      from.push_back(0);
    }
    from.push_back(0);

    std::wstring to = destFolder;
    to.push_back(0);
    to.push_back(0);

    SHFILEOPSTRUCTW fos = {0};
    fos.hwnd = hParent;
    fos.wFunc = FO_MOVE;
    fos.pFrom = from.c_str();
    fos.pTo = to.c_str();
    fos.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;

    return (SHFileOperationW(&fos) == 0 && !fos.fAnyOperationsAborted);
  }
  return false;
}

bool MoveHelper::RequestCopy(HWND hParent,
                             const std::vector<std::wstring> &filePaths,
                             int key, IWICImagingFactory *pFactory) {
  if (filePaths.empty())
    return false;

  std::wstring destFolder = GetFolder(key);
  if (destFolder.empty()) {
    MessageBoxW(hParent, LoadStr(IDS_ERROR_NO_FOLDER).c_str(), L"Info", MB_OK);
    return false;
  }

  if (!std::filesystem::exists(destFolder)) {
    MessageBoxW(hParent, LoadStr(IDS_ERROR_PATH).c_str(), L"Error", MB_OK);
    return false;
  }

  s_confirmState.sourcePaths = filePaths;
  s_confirmState.destFolder = destFolder;
  s_confirmState.pFactory = pFactory;
  s_confirmState.hIcon = NULL;

  if (DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_MOVE_CONFIRM),
                      hParent, MoveConfirmDlgProc, 0) == IDOK) {

    std::wstring from;
    for (const auto &path : filePaths) {
      from += path;
      from.push_back(0);
    }
    from.push_back(0);

    std::wstring to = destFolder;
    to.push_back(0);
    to.push_back(0);

    SHFILEOPSTRUCTW fos = {0};
    fos.hwnd = hParent;
    fos.wFunc = FO_COPY;
    fos.pFrom = from.c_str();
    fos.pTo = to.c_str();
    fos.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;

    return (SHFileOperationW(&fos) == 0 && !fos.fAnyOperationsAborted);
  }
  return false;
}

INT_PTR CALLBACK MoveHelper::MoveConfirmDlgProc(HWND hDlg, UINT message,
                                                WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG: {
    SetWindowTextW(hDlg, LoadStr(IDS_CONFIRM_TITLE).c_str());

    std::wstring fromText;
    if (s_confirmState.sourcePaths.size() == 1) {
      fromText = s_confirmState.sourcePaths[0];
    } else {
      fromText = std::to_wstring(s_confirmState.sourcePaths.size()) +
                 L" items (starts with: " +
                 std::filesystem::path(s_confirmState.sourcePaths[0])
                     .filename()
                     .wstring() +
                 L")";
    }
    SetDlgItemTextW(hDlg, IDC_STATIC_FROM, fromText.c_str());
    SetDlgItemTextW(hDlg, IDC_STATIC_TO, s_confirmState.destFolder.c_str());

    // Generate Thumbnail of the first item
    HBITMAP hBmp = GenerateThumbnail(s_confirmState.sourcePaths[0],
                                     s_confirmState.pFactory, 64, 64);
    if (hBmp) {
      SendDlgItemMessageW(hDlg, IDC_STATIC_THUMB, STM_SETIMAGE, IMAGE_BITMAP,
                          (LPARAM)hBmp);
    }
  }
    return (INT_PTR)TRUE;

  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK) {
      EndDialog(hDlg, IDOK);
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, IDCANCEL);
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

bool MoveHelper::PerformMove(HWND hParent, const std::wstring &src,
                             const std::wstring &dst) {
  // Shell Move with Undo
  std::vector<wchar_t> s(src.begin(), src.end());
  s.push_back(0);
  s.push_back(0);
  std::vector<wchar_t> d(dst.begin(), dst.end());
  d.push_back(0);
  d.push_back(0);

  SHFILEOPSTRUCTW fos = {0};
  fos.hwnd = hParent;
  fos.wFunc = FO_MOVE;
  fos.pFrom = s.data();
  fos.pTo = d.data();
  fos.fFlags = FOF_ALLOWUNDO | FOF_SIMPLEPROGRESS |
               FOF_NOCONFIRMATION; // We already confirmed.

  int result = SHFileOperationW(&fos);
  return (result == 0 && !fos.fAnyOperationsAborted);
}

// Simplified thumbnail gen (Reuse ImageView logic ideally, but here just quick
// WIC load)
HBITMAP MoveHelper::GenerateThumbnail(const std::wstring &path,
                                      IWICImagingFactory *pFactory, int width,
                                      int height) {
  if (!pFactory)
    return NULL;

  IWICBitmapDecoder *pDecoder = NULL;
  HRESULT hr = pFactory->CreateDecoderFromFilename(
      path.c_str(), NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
      &pDecoder);
  if (FAILED(hr))
    return NULL;

  IWICBitmapFrameDecode *pFrame = NULL;
  pDecoder->GetFrame(0, &pFrame);
  if (!pFrame) {
    pDecoder->Release();
    return NULL;
  }

  // Scale
  IWICBitmapScaler *pScaler = NULL;
  pFactory->CreateBitmapScaler(&pScaler);
  pScaler->Initialize(pFrame, width, height, WICBitmapInterpolationModeLinear);

  // Get Pixels? Need HBITMAP.
  // Convert to 32bppPBGRA for convenience? Or BGR.
  // Create DIB Section?
  // Quickest way without GDI+ or D2D here (since we need HBITMAP for dialog):
  // Use CreateDIBSection and copy pixels.

  IWICFormatConverter *pConverter = NULL;
  pFactory->CreateFormatConverter(&pConverter);
  pConverter->Initialize(pScaler, GUID_WICPixelFormat32bppPBGRA,
                         WICBitmapDitherTypeNone, NULL, 0.f,
                         WICBitmapPaletteTypeMedianCut);

  UINT w = width, h = height;
  // pScaler->GetSize(&w, &h);

  BITMAPINFO bmi = {0};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -(LONG)h; // Top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void *pBits = NULL;
  HBITMAP hBitmap =
      CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);

  if (hBitmap && pBits) {
    UINT stride = w * 4;
    pConverter->CopyPixels(NULL, stride, stride * h, (BYTE *)pBits);
  }

  pConverter->Release();
  pScaler->Release();
  pFrame->Release();
  pDecoder->Release();

  return hBitmap;
}

static std::wstring s_renameFrom;
static std::wstring s_renameTo;

static INT_PTR CALLBACK RenameDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                      LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG: {
    std::filesystem::path p(s_renameFrom);
    SetWindowTextW(hDlg, LoadStr(IDS_RENAME_TITLE).c_str());
    SetDlgItemTextW(hDlg, IDC_EDIT_RENAME, p.filename().c_str());
  }
    return (INT_PTR)TRUE;
  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK) {
      wchar_t buf[MAX_PATH];
      GetDlgItemTextW(hDlg, IDC_EDIT_RENAME, buf, MAX_PATH);
      s_renameTo = buf;
      EndDialog(hDlg, IDOK);
    } else if (LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, IDCANCEL);
    }
    return (INT_PTR)TRUE;
  }
  return (INT_PTR)FALSE;
}

bool MoveHelper::RenameFile(HWND hParent, const std::wstring &filePath) {
  s_renameFrom = filePath;
  s_renameTo.clear();

  if (DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_RENAME),
                      hParent, RenameDlgProc, 0) == IDOK) {
    if (s_renameTo.empty())
      return false;

    std::filesystem::path src(filePath);
    std::filesystem::path dst = src.parent_path() / s_renameTo;

    std::wstring s = filePath;
    s.push_back(0);
    std::wstring d = dst.wstring();
    d.push_back(0);

    SHFILEOPSTRUCTW fos = {0};
    fos.hwnd = hParent;
    fos.wFunc = FO_RENAME;
    fos.pFrom = s.c_str();
    fos.pTo = d.c_str();
    fos.fFlags = FOF_ALLOWUNDO;

    return (SHFileOperationW(&fos) == 0 && !fos.fAnyOperationsAborted);
  }
  return false;
}

bool MoveHelper::DeleteFiles(HWND hParent,
                             const std::vector<std::wstring> &filePaths) {
  if (filePaths.empty())
    return false;

  wchar_t buf[1024];
  if (filePaths.size() == 1) {
    std::filesystem::path p(filePaths[0]);
    swprintf(buf, 1024, LoadStr(IDS_DELETE_CONFIRM).c_str(),
             p.filename().c_str());
  } else {
    swprintf(buf, 1024, L"Are you sure you want to delete these %d items?",
             (int)filePaths.size());
  }

  if (MessageBoxW(hParent, buf, LoadStr(IDS_DELETE_TITLE).c_str(),
                  MB_YESNO | MB_ICONQUESTION) != IDYES) {
    return false;
  }

  std::wstring from;
  for (const auto &path : filePaths) {
    from += path;
    from.push_back(0);
  }
  from.push_back(0);

  SHFILEOPSTRUCTW fos = {0};
  fos.hwnd = hParent;
  fos.wFunc = FO_DELETE;
  fos.pFrom = from.c_str();
  fos.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;

  return (SHFileOperationW(&fos) == 0 && !fos.fAnyOperationsAborted);
}

bool MoveHelper::CutFile(HWND hParent, const std::wstring &filePath) {
  if (!OpenClipboard(hParent))
    return false;
  EmptyClipboard();

  size_t size = sizeof(DROPFILES) + (filePath.size() + 2) * sizeof(wchar_t);
  HGLOBAL hGlobal = GlobalAlloc(GHND, size);
  if (hGlobal) {
    DROPFILES *pDrop = (DROPFILES *)GlobalLock(hGlobal);
    pDrop->pFiles = sizeof(DROPFILES);
    pDrop->fWide = TRUE;
    wcscpy((wchar_t *)(pDrop + 1), filePath.c_str());
    GlobalUnlock(hGlobal);
    SetClipboardData(CF_HDROP, hGlobal);
  }
  CloseClipboard();
  return true;
}

INT_PTR CALLBACK MoveHelper::FolderSettingsDlgProc(HWND hDlg, UINT message,
                                                   WPARAM wParam,
                                                   LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG:
    for (int i = 0; i <= 9; ++i) {
      int id = (i == 0) ? IDC_EDIT_FOLDER_0 : (IDC_EDIT_FOLDER_1 + (i - 1));
      SetDlgItemTextW(hDlg, id, MoveHelper::Instance().GetFolder(i).c_str());
    }
    return (INT_PTR)TRUE;

  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK) {
      wchar_t buf[MAX_PATH];
      for (int i = 0; i <= 9; ++i) {
        int id = (i == 0) ? IDC_EDIT_FOLDER_0 : (IDC_EDIT_FOLDER_1 + (i - 1));
        GetDlgItemTextW(hDlg, id, buf, MAX_PATH);
        MoveHelper::Instance().SetFolder(i, buf);
      }
      EndDialog(hDlg, IDOK);
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, IDCANCEL);
      return (INT_PTR)TRUE;
    }
    if (LOWORD(wParam) >= IDC_BTN_BROWSE_0 &&
        LOWORD(wParam) <= IDC_BTN_BROWSE_0 + 9) {
      int key = LOWORD(wParam) - IDC_BTN_BROWSE_0;
      IFileDialog *pfd;
      if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL,
                                     CLSCTX_INPROC_SERVER,
                                     IID_PPV_ARGS(&pfd)))) {
        DWORD dwOptions;
        pfd->GetOptions(&dwOptions);
        pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);
        if (SUCCEEDED(pfd->Show(hDlg))) {
          IShellItem *psi;
          if (SUCCEEDED(pfd->GetResult(&psi))) {
            PWSTR pszPath;
            if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
              int id = (key == 0) ? IDC_EDIT_FOLDER_0
                                  : (IDC_EDIT_FOLDER_1 + (key - 1));
              SetDlgItemTextW(hDlg, id, pszPath);
              CoTaskMemFree(pszPath);
            }
            psi->Release();
          }
        }
        pfd->Release();
      }
    }
    break;
  }
  return (INT_PTR)FALSE;
}

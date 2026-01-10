#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "ExplorerView.h"
#include "ImageViewerWindow.h"
#include "PluginManager.h"

extern PluginManager g_pluginManager;
#include "MoveHelper.h"
#include "resource.h"
#include <algorithm>
#include <chrono>
#include <commctrl.h>
#include <ctime>
#include <filesystem>
#include <shlwapi.h>
#include <thread>
#include <windows.h>

extern INT_PTR CALLBACK PropertiesDlgProc(HWND hDlg, UINT message,
                                          WPARAM wParam, LPARAM lParam);

ExplorerView::ExplorerView()
    : m_hParent(NULL), m_hTree(NULL), m_hList(NULL), m_hSplitter(NULL),
      m_currentMode(EV_THUMBNAILS), m_thumbnailSize(96), m_splitterPos(200),
      m_draggingSplitter(false), m_hImageListThumb(NULL), m_hImageListSys(NULL),
      m_cancelThumb(false) {}

ExplorerView::~ExplorerView() {
  m_cancelThumb = true;
  if (m_thumbThread.joinable())
    m_thumbThread.join();

  if (m_hImageListThumb)
    ImageList_Destroy(m_hImageListThumb);
}

void ExplorerView::Create(HWND hParent, HINSTANCE hInst) {
  m_hParent = hParent;
  m_hInst = hInst;

  // Initialize common controls
  INITCOMMONCONTROLSEX icex = {sizeof(icex)};
  icex.dwICC =
      ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&icex);

  // Create TreeView
  m_hTree =
      CreateWindowExW(0, WC_TREEVIEWW, L"",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES |
                          TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
                      0, 0, m_splitterPos, 0, hParent, (HMENU)100, hInst, NULL);

  // Create Splitter (Visual Gutter)
  m_hSplitter =
      CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_NOTIFY,
                      m_splitterPos, 0, 5, 0, hParent, (HMENU)102, hInst, NULL);
  SetWindowLongPtr(m_hSplitter, GWLP_USERDATA, (LONG_PTR)this);

  // Create ListView
  m_hList = CreateWindowExW(
      0, WC_LISTVIEWW, L"",
      WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_ICON | LVS_AUTOARRANGE |
          LVS_SHAREIMAGELISTS | LVS_SHOWSELALWAYS | LVS_EDITLABELS,
      m_splitterPos + 5, 0, 0, 0, hParent, (HMENU)101, hInst, NULL);

  ListView_SetExtendedListViewStyle(m_hList,
                                    LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);

  // Init System Image List
  SHFILEINFOW sfi = {};
  m_hImageListSys = (HIMAGELIST)SHGetFileInfoW(
      L"C:\\", 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
  TreeView_SetImageList(m_hTree, m_hImageListSys, TVSIL_NORMAL);
  // ListView will use thumb list by default in Thumbnail mode,
  // but we can set smaller sys list for details mode later if needed.

  // Init Tree
  PopulateTree(L""); // Populate Roots
  SetupColumns();
}

void ExplorerView::Resize(int x, int y, int width, int height) {
  if (m_hTree && m_hList && m_hSplitter) {
    MoveWindow(m_hTree, 0, 0, m_splitterPos, height, TRUE);
    MoveWindow(m_hSplitter, m_splitterPos, 0, 5, height, TRUE);
    MoveWindow(m_hList, m_splitterPos + 5, 0, width - m_splitterPos - 5, height,
               TRUE);
  }
}

void ExplorerView::SetViewMode(ExplorerViewMode mode) {
  m_currentMode = mode;
  DWORD style = GetWindowLong(m_hList, GWL_STYLE);
  style &= ~LVS_TYPEMASK;

  if (mode == EV_THUMBNAILS) {
    style |= LVS_ICON;
  } else {
    style |= LVS_REPORT;
  }
  SetWindowLong(m_hList, GWL_STYLE, style);

  // Re-populate to refresh view style
  if (!m_currentFolder.empty()) {
    PopulateList(m_currentFolder);
  }
}

void ExplorerView::SetThumbnailSize(int size) {
  m_thumbnailSize = size;
  // Re-create ImageList and refresh
  if (!m_currentFolder.empty()) {
    PopulateList(m_currentFolder);
  }
}

void ExplorerView::SetupColumns() {
  LVCOLUMN lvc;
  lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
  lvc.fmt = LVCFMT_LEFT;

  std::wstring cols[] = {L"Name", L"Size", L"Date Modified"};
  int widths[] = {200, 80, 120};

  for (int i = 0; i < 3; i++) {
    lvc.iSubItem = i;
    lvc.cx = widths[i];
    lvc.pszText = const_cast<LPWSTR>(cols[i].c_str());
    ListView_InsertColumn(m_hList, i, &lvc);
  }
}

HTREEITEM ExplorerView::FindTreeItem(HTREEITEM hParent,
                                     const std::wstring &name) {
  HTREEITEM hItem = (hParent == NULL) ? TreeView_GetRoot(m_hTree)
                                      : TreeView_GetChild(m_hTree, hParent);
  while (hItem) {
    wchar_t text[256];
    TVITEMW tvi = {};
    tvi.mask = TVIF_TEXT | TVIF_HANDLE;
    tvi.hItem = hItem;
    tvi.pszText = text;
    tvi.cchTextMax = 256;
    TreeView_GetItem(m_hTree, &tvi);

    if (lstrcmpiW(text, name.c_str()) == 0) {
      return hItem;
    }
    hItem = TreeView_GetNextSibling(m_hTree, hItem);
  }
  return NULL;
}

extern void AppLog(const std::wstring &msg);

void ExplorerView::NavigateTo(const std::wstring &path) {
  AppLog(L"Navigating to: " + path);
  if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) {
    AppLog(L"Path does not exist or is not a directory.");
    return;
  }

  // 1. Split path into components
  std::vector<std::wstring> parts;
  std::filesystem::path p(path);

  // Root name (e.g. "C:")
  std::wstring root = p.root_name().wstring();
  if (root.empty()) {
    AppLog(L"Root is empty.");
    return;
  }
  AppLog(L"Parsed Root: " + root);

  // Components after root
  // std::filesystem::path iteration includes root_name, root_directory, etc.
  // We want relative parts.
  for (const auto &part : p) {
    if (part == p.root_name())
      continue;
    if (part == L"\\")
      continue; // root dir
    if (part == L"/")
      continue;
    if (part.wstring() == L"\\")
      continue;
    if (part.wstring().empty())
      continue;
    parts.push_back(part.wstring());
  }

  // 2. Find Root in Tree
  // Tree might have "C:\" or "C:" or "Local Disk (C:)" depending on
  // implementation. Currently PopulateTree adds "A:\", "B:\", etc. So we look
  // for "C:\"
  std::wstring rootSearch = root;
  if (rootSearch.back() != L'\\')
    rootSearch += L"\\";

  AppLog(L"Searching for root node: " + rootSearch);
  HTREEITEM hCurrent = FindTreeItem(NULL, rootSearch);
  if (!hCurrent) {
    // Try without backslash
    AppLog(L"Root not found, trying: " + root);
    hCurrent = FindTreeItem(NULL, root);
  }

  if (!hCurrent) {
    AppLog(L"Root not found in tree. Falling back to PopulateList.");
    // Try to find by just starting with, in case of "Local Disk (C:)" style?
    // Current PopulateTree is strictly "X:\"
    PopulateList(path);
    return;
  }
  AppLog(L"Root found.");

  // 3. Traverse
  TreeView_EnsureVisible(m_hTree, hCurrent); // Make root visible?

  for (const auto &part : parts) {
    AppLog(L"Looking for child: " + part);
    // Extract current path to ensure we add children for the right folder
    std::wstring currentPath = GetPathFromTreeItem(hCurrent);

    // Expand to load children
    // If it's a fresh node, it might have a dummy child or children=1 but no
    // actual child items yet. The TVN_ITEMEXPANDING might not fire if we use
    // TVE_EXPAND programmatically without flag? Or if we need to force reload.
    // Let's manually ensure children are loaded.
    AddChildFolders(hCurrent, currentPath);
    TreeView_Expand(m_hTree, hCurrent, TVE_EXPAND);

    // Find next part
    HTREEITEM hChild = FindTreeItem(hCurrent, part);
    if (!hChild) {
      // Failed to find part
      AppLog(L"Child not found: " + part);
      break;
    }
    hCurrent = hChild;
  }

  // 4. Select final
  TreeView_SelectItem(m_hTree, hCurrent);
  TreeView_EnsureVisible(m_hTree, hCurrent);
  // Selection change triggers PopulateList via TVN_SELCHANGED
}

void ExplorerView::PopulateTree(const std::wstring &rootPath) {
  TreeView_DeleteAllItems(m_hTree);

  // Add Drives
  DWORD drives = GetLogicalDrives();
  for (int i = 0; i < 26; i++) {
    if (drives & (1 << i)) {
      std::wstring drive = L"A:\\";
      drive[0] += i;

      SHFILEINFOW sfi = {};
      SHGetFileInfoW(drive.c_str(), 0, &sfi, sizeof(sfi),
                     SHGFI_SYSICONINDEX | SHGFI_ICON | SHGFI_SMALLICON);

      TVINSERTSTRUCTW tvis = {};
      tvis.hParent = TVI_ROOT;
      tvis.hInsertAfter = TVI_LAST;
      tvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE |
                       TVIF_SELECTEDIMAGE;
      tvis.item.pszText = const_cast<LPWSTR>(drive.c_str());
      tvis.item.cChildren = 1; // Force + button
      tvis.item.iImage = sfi.iIcon;
      tvis.item.iSelectedImage = sfi.iIcon;

      HTREEITEM hItem = TreeView_InsertItem(m_hTree, &tvis);
    }
  }
}

void ExplorerView::AddChildFolders(HTREEITEM hParent,
                                   const std::wstring &path) {
  // Remove dummy or existing
  HTREEITEM child = TreeView_GetChild(m_hTree, hParent);
  while (child) {
    HTREEITEM next = TreeView_GetNextSibling(m_hTree, child);
    TreeView_DeleteItem(m_hTree, child);
    child = next;
  }

  try {
    if (!std::filesystem::exists(path))
      return;
    std::vector<std::filesystem::directory_entry> entries;
    for (const auto &entry : std::filesystem::directory_iterator(path)) {
      if (entry.is_directory()) {
        entries.push_back(entry);
      }
    }

    // Sort Folders Ascent (Natural Sort)
    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
      return StrCmpLogicalW(a.path().filename().c_str(),
                            b.path().filename().c_str()) < 0;
    });

    for (const auto &entry : entries) {
      std::wstring filename = entry.path().filename().wstring();
      std::wstring fullPath = entry.path().wstring();

      SHFILEINFOW sfi = {};
      SHGetFileInfoW(fullPath.c_str(), 0, &sfi, sizeof(sfi),
                     SHGFI_SYSICONINDEX | SHGFI_ICON | SHGFI_SMALLICON);

      TVINSERTSTRUCTW tvis = {};
      tvis.hParent = hParent;
      tvis.hInsertAfter = TVI_LAST;
      tvis.item.mask =
          TVIF_TEXT | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
      tvis.item.pszText = const_cast<LPWSTR>(filename.c_str());
      tvis.item.iImage = sfi.iIcon;
      tvis.item.iSelectedImage = sfi.iIcon;

      // Check if it has subfolders for + button
      bool hasSub = false;
      try {
        for (const auto &sub :
             std::filesystem::directory_iterator(entry.path())) {
          if (sub.is_directory()) {
            hasSub = true;
            break;
          }
        }
      } catch (...) {
      }

      tvis.item.cChildren = hasSub ? 1 : 0;
      TreeView_InsertItem(m_hTree, &tvis);
    }
  } catch (...) {
  }
}

void ExplorerView::PopulateList(const std::wstring &folderPath) {
  ListView_DeleteAllItems(m_hList);
  m_currentFolder = folderPath;
  m_currentFileList.clear();

  if (m_hImageListThumb) {
    ImageList_Destroy(m_hImageListThumb);
  }
  m_hImageListThumb =
      ImageList_Create(m_thumbnailSize, m_thumbnailSize, ILC_COLOR32, 0, 10);
  ListView_SetImageList(m_hList, m_hImageListThumb, LVSIL_NORMAL);

  if (!std::filesystem::exists(folderPath))
    return;

  std::vector<std::filesystem::directory_entry> entries;
  try {
    for (const auto &entry : std::filesystem::directory_iterator(folderPath)) {
      if (!entry.is_directory()) {
        std::wstring ext = entry.path().extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == L".jpg" || ext == L".jpeg" || ext == L".png" ||
            ext == L".bmp" || ext == L".gif" || ext == L".zip" ||
            ext == L".cbz" || ext == L".rar" || ext == L".cbr") {
          entries.push_back(entry);
        }
      }
    }
  } catch (...) {
    return;
  }

  ImageList_SetImageCount(m_hImageListThumb, (int)entries.size());

  // Sort natural
  std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
    return StrCmpLogicalW(a.path().filename().c_str(),
                          b.path().filename().c_str()) < 0;
  });

  int index = 0;
  for (const auto &entry : entries) {
    std::wstring filename = entry.path().filename().wstring();
    m_currentFileList.push_back(entry.path().wstring());

    LVITEMW lvi = {};
    lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
    lvi.iItem = index;
    lvi.iSubItem = 0;
    lvi.pszText = const_cast<LPWSTR>(filename.c_str());
    lvi.iImage = index;
    lvi.lParam = index;

    ListView_InsertItem(m_hList, &lvi);

    // Metadata
    try {
      auto fsize = std::filesystem::file_size(entry.path());
      wchar_t sizeBuf[32];
      if (fsize < 1024)
        swprintf_s(sizeBuf, L"%llu B", (unsigned long long)fsize);
      else if (fsize < 1024 * 1024)
        swprintf_s(sizeBuf, L"%llu KB", (unsigned long long)(fsize / 1024));
      else
        swprintf_s(sizeBuf, L"%.2f MB", (double)fsize / (1024 * 1024));
      ListView_SetItemText(m_hList, index, 1, sizeBuf);

      auto ftime = std::filesystem::last_write_time(entry.path());
      auto sctp =
          std::chrono::time_point_cast<std::chrono::system_clock::duration>(
              ftime - std::filesystem::file_time_type::clock::now() +
              std::chrono::system_clock::now());
      std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
      struct tm gmt;
      localtime_s(&gmt, &tt);
      wchar_t dateBuf[64];
      swprintf_s(dateBuf, L"%04d-%02d-%02d %02d:%02d", gmt.tm_year + 1900,
                 gmt.tm_mon + 1, gmt.tm_mday, gmt.tm_hour, gmt.tm_min);
      ListView_SetItemText(m_hList, index, 2, dateBuf);
    } catch (...) {
    }

    index++;
  }

  // Background thumbs
  m_cancelThumb = true;
  if (m_thumbThread.joinable())
    m_thumbThread.join();

  m_cancelThumb = false;
  m_thumbThread = std::thread(&ExplorerView::GenerateThumbnailsThread, this);
}

std::wstring ExplorerView::GetPathFromTreeItem(HTREEITEM hItem) {
  // Build path from bottom up
  if (!hItem)
    return L"";

  std::wstring path = L"";
  HTREEITEM current = hItem;
  while (current) {
    WCHAR text[MAX_PATH];
    TVITEMW tvi = {};
    tvi.hItem = current;
    tvi.mask = TVIF_TEXT;
    tvi.pszText = text;
    tvi.cchTextMax = MAX_PATH;

    TreeView_GetItem(m_hTree, &tvi);

    std::wstring part = text;
    if (path.empty())
      path = part;
    else {
      if (part.back() == L'\\' || path.front() == L'\\')
        path = part + path;
      else
        path = part + L"\\" + path;
    }

    current = TreeView_GetParent(m_hTree, current);
  }
  return path;
}

void ExplorerView::GenerateThumbnailsThread() {
  // This runs in a background thread
  std::wstring currentFolder;
  {
    // Copy to local to avoid concurrent access if needed,
    // though m_currentFolder is primarily changed in PopulateList which
    // finishes before this starts
    currentFolder = m_currentFolder;
  }
  std::filesystem::path folderPath(currentFolder);
  std::filesystem::path cacheDir = folderPath / ".cache";

  try {
    if (!std::filesystem::exists(cacheDir)) {
      std::filesystem::create_directories(cacheDir);
      SetFileAttributesW(cacheDir.c_str(), FILE_ATTRIBUTE_HIDDEN);
    }
  } catch (...) {
    return;
  }

  // Initialize WIC for this thread
  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (FAILED(hr))
    return;

  IWICImagingFactory *pFactory = NULL;
  CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                   IID_PPV_ARGS(&pFactory));

  if (pFactory) {
    for (int i = 0; i < (int)m_currentFileList.size(); ++i) {
      if (m_cancelThumb)
        break;

      std::wstring filePath = m_currentFileList[i];
      std::filesystem::path p(filePath);
      std::wstring thumbPath =
          (cacheDir / (p.filename().wstring() + L".thumb")).wstring();

      HICON hIcon = NULL;
      bool thumbLoaded = false;

      // Check Cache
      if (std::filesystem::exists(thumbPath)) {
        // Load from cache
        IWICBitmapDecoder *pDecoder = NULL;
        if (SUCCEEDED(pFactory->CreateDecoderFromFilename(
                thumbPath.c_str(), NULL, GENERIC_READ,
                WICDecodeMetadataCacheOnDemand, &pDecoder))) {
          thumbLoaded = true;
          pDecoder->Release();
        }
      }

      if (!thumbLoaded) {
        // Generate
        IWICBitmapDecoder *pDecoder = NULL;
        bool isArchive = ArchiveHelper::IsArchive(filePath);

        if (isArchive) {
          // For archive, maybe empty for now or extract first image
          // ...
        } else {
          IWICBitmap *pWic = g_pluginManager.LoadImage(filePath, pFactory);
          if (pWic) {
            IWICBitmapScaler *pScaler = NULL;
            pFactory->CreateBitmapScaler(&pScaler);
            if (pScaler) {
              pScaler->Initialize(pWic, m_thumbnailSize, m_thumbnailSize,
                                  WICBitmapInterpolationModeFant);

              // Save to cache
              IWICBitmapEncoder *pEncoder = NULL;
              pFactory->CreateEncoder(GUID_ContainerFormatPng, NULL, &pEncoder);
              if (pEncoder) {
                IStream *pStream = NULL;
                SHCreateStreamOnFileW(thumbPath.c_str(),
                                      STGM_CREATE | STGM_WRITE, &pStream);
                if (pStream) {
                  pEncoder->Initialize(pStream, WICBitmapEncoderNoCache);
                  IWICBitmapFrameEncode *pFrameEncode = NULL;
                  pEncoder->CreateNewFrame(&pFrameEncode, NULL);
                  if (pFrameEncode) {
                    pFrameEncode->Initialize(NULL);
                    pFrameEncode->SetSize(m_thumbnailSize, m_thumbnailSize);
                    pFrameEncode->WriteSource(pScaler, NULL);
                    pFrameEncode->Commit();
                    pFrameEncode->Release();
                  }
                  pEncoder->Commit();
                  pStream->Release();
                }
                pEncoder->Release();
              }
              thumbLoaded = true;
              pScaler->Release();
            }
            pWic->Release();
          }

          // Metadata extraction for Date Taken and Resolution has been removed
        }
      }

      if (thumbLoaded) {
        HBITMAP hBitmap = NULL;
        IWICBitmapDecoder *pTmDecoder = NULL;
        if (SUCCEEDED(pFactory->CreateDecoderFromFilename(
                thumbPath.c_str(), NULL, GENERIC_READ,
                WICDecodeMetadataCacheOnDemand, &pTmDecoder))) {
          IWICBitmapFrameDecode *pFrame = NULL;
          if (SUCCEEDED(pTmDecoder->GetFrame(0, &pFrame))) {
            IWICFormatConverter *pConverter = NULL;
            pFactory->CreateFormatConverter(&pConverter);
            if (pConverter) {
              pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppPBGRA,
                                     WICBitmapDitherTypeNone, NULL, 0.0,
                                     WICBitmapPaletteTypeMedianCut);

              UINT w, h;
              pConverter->GetSize(&w, &h);
              BITMAPINFO bmi = {0};
              bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
              bmi.bmiHeader.biWidth = w;
              bmi.bmiHeader.biHeight = -(int)h;
              bmi.bmiHeader.biPlanes = 1;
              bmi.bmiHeader.biBitCount = 32;
              bmi.bmiHeader.biCompression = BI_RGB;

              void *pBits = NULL;
              HDC hdc = GetDC(NULL);
              hBitmap =
                  CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
              if (hBitmap) {
                pConverter->CopyPixels(NULL, w * 4, w * 4 * h, (BYTE *)pBits);
              }
              ReleaseDC(NULL, hdc);
              pConverter->Release();
            }
            pFrame->Release();
          }
          pTmDecoder->Release();
        }

        if (hBitmap) {
          ImageList_Replace(m_hImageListThumb, i, hBitmap, NULL);
          DeleteObject(hBitmap);
          ListView_RedrawItems(m_hList, i, i);
        }
      }
    }
  }

  if (pFactory)
    pFactory->Release();
  CoUninitialize();
}

LRESULT ExplorerView::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_SETCURSOR: {
    HWND hTarget = (HWND)wParam;
    if (hTarget == m_hSplitter) {
      SetCursor(LoadCursor(NULL, IDC_SIZEWE));
      return TRUE;
    }
  } break;

  case WM_KEYDOWN: {
    if (wParam >= '0' && wParam <= '9') {
      int key = (wParam == '0') ? 0 : (wParam - '0');
      auto paths = GetSelectedPaths();
      KeyAction action = MoveHelper::Instance().GetKeyAction();
      if (action == KA_MOVE) {
        if (MoveHelper::Instance().RequestMove(
                m_hParent, paths, key,
                ImageViewerWindow::Instance().GetFactory())) {
          PopulateList(m_currentFolder);
        }
      } else {
        MoveHelper::Instance().RequestCopy(
            m_hParent, paths, key, ImageViewerWindow::Instance().GetFactory());
      }
      return 0;
    }
  } break;

  case WM_LBUTTONDOWN: {
    int xPos = LOWORD(lParam);
    if (xPos >= m_splitterPos && xPos <= m_splitterPos + 5) {
      m_draggingSplitter = true;
      SetCapture(m_hParent);
      return 0;
    }
  } break;

  case WM_LBUTTONUP:
    if (m_draggingSplitter) {
      m_draggingSplitter = false;
      ReleaseCapture();
      return 0;
    }
    break;

  case WM_MOUSEMOVE:
    if (m_draggingSplitter) {
      int xPos = LOWORD(lParam);
      RECT rc;
      GetClientRect(m_hParent, &rc);
      if (xPos > 50 && xPos < rc.right - 50) {
        m_splitterPos = xPos;
        Resize(0, 0, rc.right, rc.bottom);
      }
      return 0;
    }
    break;

  case WM_NOTIFY: {
    LPNMHDR pnmh = (LPNMHDR)lParam;

    if (pnmh->hwndFrom == m_hTree) {
      if (pnmh->code == TVN_SELCHANGED) {
        LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
        std::wstring path = GetPathFromTreeItem(pnmtv->itemNew.hItem);
        if (!path.empty()) {
          PopulateList(path);
        }
      } else if (pnmh->code == TVN_ITEMEXPANDING) {
        LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
        if (pnmtv->action == TVE_EXPAND) {
          std::wstring path = GetPathFromTreeItem(pnmtv->itemNew.hItem);
          AddChildFolders(pnmtv->itemNew.hItem, path);
        }
      } else if (pnmh->code == NM_RCLICK) {
        TVHITTESTINFO ht = {0};
        GetCursorPos(&ht.pt);
        ScreenToClient(m_hTree, &ht.pt);
        HTREEITEM hItem = TreeView_HitTest(m_hTree, &ht);
        if (hItem) {
          TreeView_SelectItem(m_hTree, hItem);
          POINT pt;
          GetCursorPos(&pt);
          HMENU hMenu = LoadMenu(GetModuleHandle(NULL),
                                 MAKEINTRESOURCE(IDR_CONTEXT_MENU));
          if (hMenu) {
            HMENU hSub = GetSubMenu(hMenu, 0);
            TrackPopupMenu(hSub, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hParent,
                           NULL);
            DestroyMenu(hMenu);
          }
        }
      }
    }

    if (pnmh->hwndFrom == m_hList) {
      if (pnmh->code == NM_DBLCLK) {
        LPNMITEMACTIVATE pnmia = (LPNMITEMACTIVATE)lParam;
        if (pnmia->iItem != -1) {
          int index = pnmia->iItem;
          if (index >= 0 && index < (int)m_currentFileList.size()) {
            if (OnFileOpen) {
              OnFileOpen(m_currentFileList[index], m_currentFileList);
            }
          }
        }
      } else if (pnmh->code == NM_RCLICK) {
        POINT pt;
        GetCursorPos(&pt);
        HMENU hMenu =
            LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_CONTEXT_MENU));
        if (hMenu) {
          HMENU hSub = GetSubMenu(hMenu, 0);
          HMENU hMoveTo = GetSubMenu(hSub, 2); // "Move to" is index 2
          HMENU hCopyTo = GetSubMenu(hSub, 3); // "Copy to" is index 3

          auto PopulateSubMenu = [&](HMENU hSubMenu, UINT baseId) {
            if (hSubMenu) {
              while (GetMenuItemCount(hSubMenu) > 0) {
                DeleteMenu(hSubMenu, 0, MF_BYPOSITION);
              }
              for (int i = 1; i <= 10; ++i) {
                int key = i % 10;
                std::wstring path = MoveHelper::Instance().GetFolder(key);
                std::wstring label = std::to_wstring(key) + L": ";
                if (path.empty()) {
                  label += L"(Not Set)";
                } else {
                  std::filesystem::path p(path);
                  std::wstring folderName = p.filename().wstring();
                  if (folderName.empty())
                    folderName = path;
                  label += folderName;
                }
                UINT id = (key == 0) ? baseId + 9 : baseId + key - 1;
                AppendMenuW(hSubMenu, MF_STRING, id, label.c_str());
              }
            }
          };

          PopulateSubMenu(hMoveTo, IDM_MOVETO_1);
          PopulateSubMenu(hCopyTo, IDM_COPYTO_1);

          TrackPopupMenu(hSub, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hParent, NULL);
          DestroyMenu(hMenu);
        }
      } else if (pnmh->code == LVN_ENDLABELEDITW) {
        NMLVDISPINFOW *pdi = (NMLVDISPINFOW *)lParam;
        if (pdi->item.pszText) {
          int index = (int)pdi->item.lParam;
          if (index >= 0 && index < (int)m_currentFileList.size()) {
            std::filesystem::path oldPath(m_currentFileList[index]);
            std::filesystem::path newPath =
                oldPath.parent_path() / pdi->item.pszText;
            try {
              std::filesystem::rename(oldPath, newPath);
              m_currentFileList[index] = newPath.wstring();
              return TRUE; // Accept the label change
            } catch (...) {
              return FALSE;
            }
          }
        }
        return FALSE;
      }
    }
  } break;

  case WM_COMMAND: {
    switch (LOWORD(wParam)) {
    case IDM_VIEW_REFRESH:
      if (!m_currentFolder.empty()) {
        PopulateList(m_currentFolder);
      }
      return 0;
    case IDM_CTX_OPEN_EXPLORER: {
      int sel = ListView_GetNextItem(m_hList, -1, LVNI_SELECTED);
      std::wstring path;
      bool isFile = false;
      if (sel != -1) {
        if (sel >= 0 && sel < (int)m_currentFileList.size()) {
          path = m_currentFileList[sel];
          isFile = true;
        }
      } else {
        path = m_currentFolder;
      }

      if (!path.empty()) {
        if (isFile) {
          std::wstring cmd = L"/select,\"" + path + L"\"";
          ShellExecuteW(NULL, L"open", L"explorer.exe", cmd.c_str(), NULL,
                        SW_SHOWNORMAL);
        } else {
          ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
      }
      return 0;
    }
    case IDM_CTX_RENAME: {
      int sel = ListView_GetNextItem(m_hList, -1, LVNI_SELECTED);
      if (sel != -1) {
        SetFocus(m_hList);
        ListView_EditLabel(m_hList, sel);
      }
      return 0;
    }
    case IDM_CTX_DELETE: {
      auto paths = GetSelectedPaths();
      if (!paths.empty()) {
        if (MoveHelper::Instance().DeleteFiles(m_hParent, paths)) {
          PopulateList(m_currentFolder);
        }
      }
      return 0;
    }
    case IDM_VIEW_BACK:
      // This is handled by main.cpp to switch windows usually,
      // but if the menu is tracked by parent, it might not reach here.
      // We will ensure main.cpp forwards context menu commands.
      break;

    case IDS_CTX_PROPERTIES: {
      int sel = ListView_GetNextItem(m_hList, -1, LVNI_SELECTED);
      std::wstring path;
      if (sel != -1 && sel < (int)m_currentFileList.size()) {
        path = m_currentFileList[sel];
        DialogBoxParam(m_hInst, MAKEINTRESOURCE(IDD_PROPERTIES), m_hParent,
                       PropertiesDlgProc, (LPARAM)path.c_str());
      }
      return 0;
    }

    default: {
      WORD cmd = LOWORD(wParam);
      if (cmd >= IDM_MOVETO_1 && cmd <= IDM_MOVETO_0) {
        int key = (cmd == IDM_MOVETO_0) ? 0 : (cmd - IDM_MOVETO_1 + 1);
        auto paths = GetSelectedPaths();
        if (MoveHelper::Instance().RequestMove(
                m_hParent, paths, key,
                ImageViewerWindow::Instance().GetFactory())) {
          PopulateList(m_currentFolder);
        }
        return 0;
      } else if (cmd >= IDM_COPYTO_1 && cmd <= IDM_COPYTO_0) {
        int key = (cmd == IDM_COPYTO_0) ? 0 : (cmd - IDM_COPYTO_1 + 1);
        auto paths = GetSelectedPaths();
        // Copy doesn't need refresh unless we copy to same folder..
        // But usually copying to another folder.
        MoveHelper::Instance().RequestCopy(
            m_hParent, paths, key, ImageViewerWindow::Instance().GetFactory());
        return 0;
      }
      break;
    }
    } // switch LOWORD(wParam)
  } break; // case WM_COMMAND
  } // switch uMsg
  return 0;
}

std::vector<std::wstring> ExplorerView::GetSelectedPaths() {
  std::vector<std::wstring> paths;
  int iItem = -1;
  while ((iItem = ListView_GetNextItem(m_hList, iItem, LVNI_SELECTED)) != -1) {
    if (iItem >= 0 && iItem < (int)m_currentFileList.size()) {
      paths.push_back(m_currentFileList[iItem]);
    }
  }
  return paths;
}

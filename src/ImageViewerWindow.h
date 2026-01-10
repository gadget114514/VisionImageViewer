#include "ArchiveHelper.h"
#include "ImageView.h"
#include "PluginManager.h"
#include "resource.h"
#include <algorithm>
#include <d2d1.h>
#include <d2d1helper.h>
#include <functional>
#include <string>
#include <vector>
#include <wincodec.h>
#include <windows.h>

// Forward declaration from main.cpp
void AppLog(const std::wstring &msg);

class ImageViewerWindow {
public:
  static ImageViewerWindow &Instance() {
    static ImageViewerWindow instance;
    return instance;
  }

  void Init(HINSTANCE hInst, ID2D1Factory *pD2D, IWICImagingFactory *pWIC,
            PluginManager *pPM) {
    m_hInst = hInst;
    m_pD2DFactory = pD2D;
    m_pWICFactory = pWIC;
    m_pPluginManager = pPM;
    RegisterWindowClass();
  }

  void Show(const std::wstring &filePath,
            const std::vector<std::wstring> &fileList = {}) {
    m_currentFiles = fileList;
    m_currentIndex = -1;
    // Find index
    if (!fileList.empty()) {
      auto it =
          std::find(m_currentFiles.begin(), m_currentFiles.end(), filePath);
      if (it != m_currentFiles.end()) {
        m_currentIndex = (int)std::distance(m_currentFiles.begin(), it);
      }
    }
    LoadImageInternal(filePath);
  }

  void LoadImageInternal(const std::wstring &filePath) {
    if (!m_hWnd) {
      CreateImageViewWindow();
    }

    if (!m_hWnd) {
      return;
    }

    ShowWindow(m_hWnd, SW_SHOW);
    SetForegroundWindow(m_hWnd);

    // Init D2D for this window if needed
    CreateDeviceResources();
    m_imageView.Init(m_pD2DFactory, m_pRenderTarget);
    m_imageView.SetFactory(m_pWICFactory);

    if (m_imageView.LoadImageFile(filePath, m_pWICFactory, m_pPluginManager)) {
      // Success
      RECT rc;
      GetClientRect(m_hWnd, &rc);
      D2D1_RECT_F d2dRc = D2D1::RectF((float)rc.left, (float)rc.top,
                                      (float)rc.right, (float)rc.bottom);
      m_imageView.ZoomFit(d2dRc); // Auto fit on load

      // If spread mode and from folder, load the next one as secondary
      if (m_imageView.IsSpreadMode() && !ArchiveHelper::IsArchive(filePath)) {
        if (m_currentIndex != -1 &&
            m_currentIndex + 1 < (int)m_currentFiles.size()) {
          m_imageView.LoadSecondaryImageFile(m_currentFiles[m_currentIndex + 1],
                                             m_pWICFactory, m_pPluginManager);
          m_imageView.ZoomFit(d2dRc); // Re-fit for spread
        }
      }

      // Update Title
      UpdateWindowTitle(filePath);
    } else {
      MessageBoxW(m_hWnd, L"Failed to load image.", L"Error", MB_ICONERROR);
      DestroyWindow(m_hWnd);
    }
    UpdateWindow(m_hWnd);
  }

  HWND GetHwnd() const { return m_hWnd; }
  ImageView &GetImageView() { return m_imageView; }
  IWICImagingFactory *GetFactory() { return m_pWICFactory; }

private:
  ImageViewerWindow() : m_hWnd(NULL), m_pRenderTarget(NULL) {
    m_imageView.OnRequestRepaint = [this]() {
      if (m_hWnd) {
        InvalidateRect(m_hWnd, NULL, FALSE);
        UpdateWindowTitle(m_imageView.GetCurrentFilePath());
      }
    };
    m_imageView.OnNavigate = [this](bool next) {
      if (next)
        LoadNext();
      else
        LoadPrev();
    };
  }

  ~ImageViewerWindow() {
    if (m_pRenderTarget)
      m_pRenderTarget->Release();
  }

  HINSTANCE m_hInst;
  HWND m_hWnd;
  ID2D1Factory *m_pD2DFactory;
  IWICImagingFactory *m_pWICFactory;
  ID2D1HwndRenderTarget *m_pRenderTarget;
  PluginManager *m_pPluginManager;
  ImageView m_imageView;

  std::vector<std::wstring> m_currentFiles;
  int m_currentIndex;

  // Fullscreen vars
  bool m_isFullscreen = false;
  RECT m_rcSaved = {};
  DWORD m_dwStyleSaved = 0;

  void UpdateWindowTitle(const std::wstring &filePath) {
    if (!m_hWnd)
      return;

    float scale = m_imageView.GetScale();
    D2D1_SIZE_F size = m_imageView.GetImageSize();

    wchar_t buf[256];
    if (m_imageView.IsSpreadMode()) {
      D2D1_SIZE_F size2 = m_imageView.GetSecondaryImageSize();
      swprintf_s(buf, L"[%d%%] %dx%d + %dx%d - %s", (int)(scale * 100 + 0.5f),
                 (int)size.width, (int)size.height, (int)size2.width,
                 (int)size2.height, filePath.c_str());
    } else {
      swprintf_s(buf, L"[%d%%] %dx%d - %s", (int)(scale * 100 + 0.5f),
                 (int)size.width, (int)size.height, filePath.c_str());
    }
    SetWindowTextW(m_hWnd, buf);
  }

  void ToggleFullscreen() {
    if (!m_isFullscreen) {
      // Go Fullscreen
      m_dwStyleSaved = GetWindowLong(m_hWnd, GWL_STYLE);
      GetWindowRect(m_hWnd, &m_rcSaved);

      SetWindowLong(m_hWnd, GWL_STYLE,
                    m_dwStyleSaved & ~(WS_CAPTION | WS_THICKFRAME));
      MONITORINFO mi = {sizeof(mi)};
      GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY), &mi);
      SetWindowPos(m_hWnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                   mi.rcMonitor.right - mi.rcMonitor.left,
                   mi.rcMonitor.bottom - mi.rcMonitor.top,
                   SWP_NOZORDER | SWP_FRAMECHANGED);
      m_isFullscreen = true;
    } else {
      // Exit Fullscreen
      SetWindowLong(m_hWnd, GWL_STYLE, m_dwStyleSaved);
      SetWindowPos(m_hWnd, NULL, m_rcSaved.left, m_rcSaved.top,
                   m_rcSaved.right - m_rcSaved.left,
                   m_rcSaved.bottom - m_rcSaved.top,
                   SWP_NOZORDER | SWP_FRAMECHANGED);
      m_isFullscreen = false;
    }

    // Invoke Fit to Window after structural change
    RECT rc;
    if (GetClientRect(m_hWnd, &rc)) {
      D2D1_RECT_F d2dRc = D2D1::RectF((float)rc.left, (float)rc.top,
                                      (float)rc.right, (float)rc.bottom);
      m_imageView.ZoomFit(d2dRc);
    }
    InvalidateRect(m_hWnd, NULL, FALSE);
  }

  void ShrinkWindowToImage() {
    if (!m_hWnd || m_isFullscreen)
      return;

    D2D1_SIZE_F imgSize = m_imageView.GetImageSize();
    if (m_imageView.IsSpreadMode()) {
      D2D1_SIZE_F s2 = m_imageView.GetSecondaryImageSize();
      imgSize.width += s2.width;
      imgSize.height = (std::max)(imgSize.height, s2.height);
    }

    float scale = m_imageView.GetScale();
    imgSize.width *= scale;
    imgSize.height *= scale;

    if (imgSize.width <= 0 || imgSize.height <= 0)
      return;

    // Get monitor work area
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &mi);
    int screenW = mi.rcWork.right - mi.rcWork.left;
    int screenH = mi.rcWork.bottom - mi.rcWork.top;

    // If scaled image is smaller than screen, resize window
    if (imgSize.width < screenW && imgSize.height < screenH) {
      RECT rc = {0, 0, (int)(imgSize.width + 0.5f),
                 (int)(imgSize.height + 0.5f)};
      AdjustWindowRectEx(&rc, GetWindowLong(m_hWnd, GWL_STYLE),
                         (GetMenu(m_hWnd) != NULL),
                         GetWindowLong(m_hWnd, GWL_EXSTYLE));

      int newW = rc.right - rc.left;
      int newH = rc.bottom - rc.top;

      // Keep it centered or at least visible
      RECT curRc;
      GetWindowRect(m_hWnd, &curRc);
      int x = curRc.left;
      int y = curRc.top;

      SetWindowPos(m_hWnd, NULL, x, y, newW, newH,
                   SWP_NOZORDER | SWP_NOMOVE | SWP_FRAMECHANGED);

      // Reset offsets to ensure image is perfectly framed (matching the
      // outline)
      m_imageView.ResetOffsets();
      if (m_imageView.OnRequestRepaint)
        m_imageView.OnRequestRepaint();
    }
  }

  void LoadNext() {
    if (m_currentFiles.empty())
      return;
    m_currentIndex += (m_imageView.IsSpreadMode() ? 2 : 1);
    if (m_currentIndex >= (int)m_currentFiles.size())
      m_currentIndex = 0; // Loop
    LoadImageInternal(m_currentFiles[m_currentIndex]);
  }

  void LoadPrev() {
    if (m_currentFiles.empty())
      return;
    m_currentIndex -= (m_imageView.IsSpreadMode() ? 2 : 1);
    if (m_currentIndex < 0) {
      if (m_imageView.IsSpreadMode()) {
        m_currentIndex = ((int)m_currentFiles.size() - 1) & ~1;
        if (m_currentIndex < 0)
          m_currentIndex = 0;
      } else {
        m_currentIndex = (int)m_currentFiles.size() - 1;
      }
    }
    LoadImageInternal(m_currentFiles[m_currentIndex]);
  }

  void CreateImageViewWindow() {
    RegisterWindowClass();
    m_hWnd = CreateWindowExW(0, L"ImageViewerWindow", L"Image Viewer",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             800, 600, NULL, NULL, m_hInst, this);
  }

  void RegisterWindowClass() {
    static bool registered = false;
    if (registered)
      return;
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = m_hInst;
    wc.lpszClassName = L"ImageViewerWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = MAKEINTRESOURCEW(IDR_VIEWERMENU);
    RegisterClassW(&wc);
    registered = true;
  }

  void CreateDeviceResources() {
    if (!m_pRenderTarget && m_hWnd) {
      RECT rc;
      GetClientRect(m_hWnd, &rc);
      D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
      m_pD2DFactory->CreateHwndRenderTarget(
          D2D1::RenderTargetProperties(),
          D2D1::HwndRenderTargetProperties(m_hWnd, size), &m_pRenderTarget);
      m_imageView.SetRenderTarget(m_pRenderTarget);
    }
  }

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                  LPARAM lParam) {
    ImageViewerWindow *pThis = nullptr;
    if (uMsg == WM_CREATE) {
      CREATESTRUCT *pCreate = (CREATESTRUCT *)lParam;
      pThis = (ImageViewerWindow *)pCreate->lpCreateParams;
      SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
      pThis = (ImageViewerWindow *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
      return pThis->HandleMessage(hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }

  LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT:
      if (m_pRenderTarget) {
        m_pRenderTarget->BeginDraw();
        m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
        RECT rc;
        GetClientRect(hwnd, &rc);
        D2D1_RECT_F d2dRc = D2D1::RectF((float)rc.left, (float)rc.top,
                                        (float)rc.right, (float)rc.bottom);
        m_imageView.Render(d2dRc);
        m_pRenderTarget->EndDraw();
        ValidateRect(hwnd, NULL);
      } else {
        CreateDeviceResources();
      }
      return 0;

    case WM_SIZE:
      if (m_pRenderTarget) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        m_pRenderTarget->Resize(
            D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));
      }
      return 0;

    case WM_DESTROY:
      m_hWnd = NULL;
      if (m_pRenderTarget) {
        m_pRenderTarget->Release();
        m_pRenderTarget = nullptr;
      }
      return 0;

    case WM_KEYDOWN:
      if (wParam == VK_ESCAPE) {
        if (this->m_isFullscreen) {
          this->ToggleFullscreen();
        } else {
          DestroyWindow(hwnd);
        }
      } else {
        m_imageView.OnKeyDown(wParam);
      }
      return 0;

    case WM_INITMENUPOPUP: {
      HMENU hMenu = (HMENU)wParam;
      if (hMenu) {
        // Update magnifier checkmark
        CheckMenuItem(
            hMenu, IDM_VIEW_MAGNIFIER,
            MF_BYCOMMAND |
                (m_imageView.IsMagnifierEnabled() ? MF_CHECKED : MF_UNCHECKED));
      }
    } break;

    case WM_COMMAND:
      switch (LOWORD(wParam)) {
      case IDM_FILE_EXIT:
        DestroyWindow(hwnd);
        break;
      case IDM_VIEW_FULLSCREEN:
        this->ToggleFullscreen();
        break;
      case IDM_VIEW_SHRINK_TO_FIT:
        this->ShrinkWindowToImage();
        break;
      case IDM_VIEW_ZOOM_FIT: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        D2D1_RECT_F d2dRc = D2D1::RectF((float)rc.left, (float)rc.top,
                                        (float)rc.right, (float)rc.bottom);
        m_imageView.ZoomFit(d2dRc);
      } break;
      case IDM_VIEW_ZOOM_WIDTH: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        D2D1_RECT_F d2dRc = D2D1::RectF((float)rc.left, (float)rc.top,
                                        (float)rc.right, (float)rc.bottom);
        m_imageView.ZoomFitWidth(d2dRc);
      } break;
      case IDM_VIEW_ZOOM_HEIGHT: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        D2D1_RECT_F d2dRc = D2D1::RectF((float)rc.left, (float)rc.top,
                                        (float)rc.right, (float)rc.bottom);
        m_imageView.ZoomFitHeight(d2dRc);
      } break;
      case IDM_VIEW_ZOOM_100:
        m_imageView.SetScale(1.0f);
        break;
      case IDM_VIEW_ZOOM_50:
        m_imageView.SetScale(0.5f);
        break;
      case IDM_VIEW_ZOOM_200:
        m_imageView.SetScale(2.0f);
        break;
      case IDM_VIEW_ZOOM_300:
        m_imageView.SetScale(3.0f);
        break;
      case IDM_VIEW_ZOOM_400:
        m_imageView.SetScale(4.0f);
        break;
      case IDM_VIEW_ZOOM_500:
        m_imageView.SetScale(5.0f);
        break;
      case IDM_VIEW_MAGNIFIER: {
        bool enabled = !m_imageView.IsMagnifierEnabled();
        m_imageView.SetMagnifier(enabled);
        HMENU hMenu = GetMenu(m_hWnd);
        if (hMenu) {
          CheckMenuItem(hMenu, IDM_VIEW_MAGNIFIER,
                        MF_BYCOMMAND | (enabled ? MF_CHECKED : MF_UNCHECKED));
        }
      } break;
      case IDM_VIEW_NEXT:
        LoadNext();
        break;
      case IDM_VIEW_PREV:
        LoadPrev();
        break;
      }
      return 0;

    case WM_RBUTTONDOWN:
      if (m_imageView.IsMagnifierEnabled()) {
        m_imageView.OnRightMouseDown((float)LOWORD(lParam),
                                     (float)HIWORD(lParam));
        SetCapture(hwnd);
      } else {
        // Show context menu
        HMENU hMenu = LoadMenuW(m_hInst, MAKEINTRESOURCEW(IDR_CONTEXT_MENU));
        if (hMenu) {
          HMENU hSub = GetSubMenu(hMenu, 0);
          // Update magnifier checkmark in context menu
          CheckMenuItem(hSub, IDM_VIEW_MAGNIFIER,
                        MF_BYCOMMAND |
                            (m_imageView.IsMagnifierEnabled() ? MF_CHECKED
                                                              : MF_UNCHECKED));
          POINT pt = {LOWORD(lParam), HIWORD(lParam)};
          ClientToScreen(hwnd, &pt);
          TrackPopupMenu(hSub, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0,
                         hwnd, NULL);
          DestroyMenu(hMenu);
        }
      }
      return 0;

    case WM_RBUTTONUP:
      if (m_imageView.IsMagnifierEnabled()) {
        m_imageView.OnRightMouseUp();
        ReleaseCapture();
      }
      return 0;

    case WM_LBUTTONDOWN:
      m_imageView.OnMouseDown((float)LOWORD(lParam), (float)HIWORD(lParam));
      SetCapture(hwnd);
      return 0;
    case WM_MOUSEMOVE:
      m_imageView.OnMouseMove((float)LOWORD(lParam), (float)HIWORD(lParam));
      return 0;
    case WM_LBUTTONUP: {
      RECT rc;
      GetClientRect(hwnd, &rc);
      m_imageView.OnMouseUp((float)LOWORD(lParam), (float)rc.right);
      ReleaseCapture();
    }
      return 0;
    case WM_MOUSEWHEEL: {
      float delta = GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
      POINT pt = {LOWORD(lParam), HIWORD(lParam)};
      ScreenToClient(hwnd, &pt);
      m_imageView.OnWheel(delta, (float)pt.x, (float)pt.y);
    }
      return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
};

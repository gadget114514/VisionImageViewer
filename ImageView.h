#pragma once
#include <d2d1.h>
#include <functional>
#include <string>
#include <vector>
#include <wincodec.h>
#include <windows.h>

class ImageView {
public:
  ImageView();
  ~ImageView();

  void Init(ID2D1Factory *pD2DFactory, ID2D1HwndRenderTarget *pRenderTarget);
  void SetRenderTarget(ID2D1HwndRenderTarget *pRenderTarget);
  void SetFactory(IWICImagingFactory *pFactory) {
    m_pWICFactoryCached = pFactory;
  }

  // Loads an image from path. Returns true if successful.
  bool LoadImageFile(const std::wstring &path, IWICImagingFactory *pWICFactory,
                     void *pluginManager);
  void ReloadCurrent();
  bool LoadSecondaryImageFile(const std::wstring &path,
                              IWICImagingFactory *pWICFactory,
                              void *pluginManager);

  void SetScale(float scale);
  void ResetOffsets();
  float GetScale() const { return m_scale; }
  D2D1_SIZE_F GetImageSize() const {
    return m_pBitmap ? m_pBitmap->GetSize() : D2D1::SizeF(0, 0);
  }
  D2D1_SIZE_F GetSecondaryImageSize() const {
    return m_pBitmapSecondary ? m_pBitmapSecondary->GetSize()
                              : D2D1::SizeF(0, 0);
  }
  void ZoomFit(const D2D1_RECT_F &clientRect);
  void ZoomFitWidth(const D2D1_RECT_F &clientRect);
  void ZoomFitHeight(const D2D1_RECT_F &clientRect);

  void Render(const D2D1_RECT_F &clientRect);

  // Input Handling
  void OnWheel(float delta, float x, float y);
  void OnKeyDown(WPARAM key);
  void OnMouseDown(float x, float y);
  void OnMouseMove(float x, float y);
  void OnMouseUp(float x, float width); // Correct signature
  void OnRightMouseDown(float x, float y);
  void OnRightMouseUp();

  std::function<void(bool /*next*/)> OnNavigate;
  std::function<void()> OnRequestRepaint;

  std::wstring GetCurrentFilePath() const { return m_currentPath; }

  void SetViewMode(bool spread) { m_isSpread = spread; }
  bool IsSpreadMode() const { return m_isSpread; }

  void SetMagnifier(bool enable) { m_magEnabled = enable; }
  bool IsMagnifierEnabled() const { return m_magEnabled; }
  void SetMagnifierSettings(float size, float zoom) {
    m_magSize = size;
    m_magZoom = zoom;
  }
  float GetMagnifierSize() const { return m_magSize; }
  float GetMagnifierZoom() const { return m_magZoom; }

private:
  ID2D1Factory *m_pD2DFactory;
  ID2D1HwndRenderTarget *m_pRenderTarget;
  IWICImagingFactory *m_pWICFactoryCached;
  ID2D1Bitmap *m_pBitmap;
  ID2D1Bitmap *m_pBitmapSecondary;

  std::wstring m_currentPath;

  // View State
  float m_scale;
  float m_offsetX;
  float m_offsetY;
  bool m_isSpread;

  // Drag State
  bool m_isDragging;
  float m_lastMouseX;
  float m_lastMouseY;

  // Archive State
  bool m_isArchive;
  std::wstring m_archivePath;
  std::vector<std::string> m_archiveImages;
  int m_currentArchiveIndex;

  // Magnifying Glass
  bool m_magEnabled;
  float m_magSize;
  float m_magZoom;
  float m_mouseX;
  float m_mouseY;
  bool m_isRightButtonDown; // Added to track right button for magnifier

  void ResetView();
  bool LoadFromArchiveInternal(IWICImagingFactory *pFactory);
};

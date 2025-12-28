#include "ImageView.h"
#include "ArchiveHelper.h"
#include "PluginManager.h"
#include <algorithm>
#include <d2d1helper.h>
#include <shlwapi.h>

// Anonymous namespace for helpers
namespace {
IWICBitmap *CreateWicFromStreamHelper(IStream *pStream,
                                      IWICImagingFactory *pFactory) {
  IWICBitmapDecoder *pDecoder = NULL;
  IWICBitmap *pBitmap = NULL;
  if (SUCCEEDED(pFactory->CreateDecoderFromStream(
          pStream, NULL, WICDecodeMetadataCacheOnDemand, &pDecoder))) {
    IWICBitmapFrameDecode *pFrame = NULL;
    if (SUCCEEDED(pDecoder->GetFrame(0, &pFrame))) {
      pFactory->CreateBitmapFromSource(pFrame, WICBitmapCacheOnLoad, &pBitmap);
      pFrame->Release();
    }
    pDecoder->Release();
  }
  return pBitmap;
}

IWICBitmap *LoadWicBitmapHelper(const std::wstring &path,
                                IWICImagingFactory *pWICFactory,
                                void *pluginManagerOrNull) {
  PluginManager *pPM = static_cast<PluginManager *>(pluginManagerOrNull);

  // Check if simple file
  if (pPM) {
    return pPM->LoadImage(path, pWICFactory);
  }

  return nullptr;
}

ID2D1Bitmap *CreateD2DBitmapHelper(IWICBitmap *pWicBitmap,
                                   IWICImagingFactory *pWICFactory,
                                   ID2D1HwndRenderTarget *pRenderTarget) {
  ID2D1Bitmap *pD2DBitmap = nullptr;
  if (pWicBitmap) {
    IWICFormatConverter *pConverter = NULL;
    pWICFactory->CreateFormatConverter(&pConverter);
    if (pConverter) {
      pConverter->Initialize(pWicBitmap, GUID_WICPixelFormat32bppPBGRA,
                             WICBitmapDitherTypeNone, NULL, 0.f,
                             WICBitmapPaletteTypeMedianCut);

      pRenderTarget->CreateBitmapFromWicBitmap(pConverter, NULL, &pD2DBitmap);
      pConverter->Release();
    }
  }
  return pD2DBitmap;
}
} // namespace

ImageView::ImageView()
    : m_pD2DFactory(nullptr), m_pRenderTarget(nullptr),
      m_pWICFactoryCached(nullptr), m_pBitmap(nullptr),
      m_pBitmapSecondary(nullptr), m_scale(1.0f), m_offsetX(0.0f),
      m_offsetY(0.0f), m_isSpread(false), m_isDragging(false),
      m_lastMouseX(0.0f), m_lastMouseY(0.0f), m_isArchive(false),
      m_currentArchiveIndex(-1), m_magEnabled(false), m_magSize(200.0f),
      m_magZoom(2.0f), m_mouseX(0.0f), m_mouseY(0.0f),
      m_isRightButtonDown(false) {}

ImageView::~ImageView() {
  if (m_pBitmap)
    m_pBitmap->Release();
  if (m_pBitmapSecondary)
    m_pBitmapSecondary->Release();
}

void ImageView::Init(ID2D1Factory *pD2DFactory,
                     ID2D1HwndRenderTarget *pRenderTarget) {
  m_pD2DFactory = pD2DFactory;
  m_pRenderTarget = pRenderTarget;
}

void ImageView::SetRenderTarget(ID2D1HwndRenderTarget *pRenderTarget) {
  m_pRenderTarget = pRenderTarget;
}

void ImageView::ResetView() {
  m_scale = 1.0f;
  m_offsetX = 0.0f;
  m_offsetY = 0.0f;
  m_isDragging = false;
}

void ImageView::ResetOffsets() {
  m_offsetX = 0.0f;
  m_offsetY = 0.0f;
}

bool ImageView::LoadImageFile(const std::wstring &path,
                              IWICImagingFactory *pWICFactory,
                              void *pluginManager) {
  // Check if Archive
  if (ArchiveHelper::IsArchive(path)) {
    m_isArchive = true;
    m_archivePath = path;
    m_archiveImages = ArchiveHelper::GetImageFiles(path);
    m_currentArchiveIndex = 0;

    return LoadFromArchiveInternal(pWICFactory);
  }
  m_isArchive = false;

  // Normal File Load
  if (m_pBitmap) {
    m_pBitmap->Release();
    m_pBitmap = nullptr;
  }
  if (m_pBitmapSecondary) {
    m_pBitmapSecondary->Release();
    m_pBitmapSecondary = nullptr;
  }

  if (!m_pRenderTarget)
    return false;

  IWICBitmap *pWic = LoadWicBitmapHelper(path, pWICFactory, pluginManager);
  if (pWic) {
    m_pBitmap = CreateD2DBitmapHelper(pWic, pWICFactory, m_pRenderTarget);
    pWic->Release();
  }

  if (m_pBitmap) {
    m_currentPath = path;
    ResetView();
    if (OnRequestRepaint)
      OnRequestRepaint();
    return true;
  }
  return false;
}

bool ImageView::LoadFromArchiveInternal(IWICImagingFactory *pWICFactory) {
  if (m_archiveImages.empty())
    return false;
  if (m_currentArchiveIndex < 0)
    m_currentArchiveIndex = 0;
  if (m_currentArchiveIndex >= (int)m_archiveImages.size())
    m_currentArchiveIndex = (int)m_archiveImages.size() - 1;

  if (m_pBitmap) {
    m_pBitmap->Release();
    m_pBitmap = nullptr;
  }
  if (m_pBitmapSecondary) {
    m_pBitmapSecondary->Release();
    m_pBitmapSecondary = nullptr;
  }

  // Load Primary
  std::string currentImage = m_archiveImages[m_currentArchiveIndex];
  std::vector<uint8_t> data =
      ArchiveHelper::ExtractFileToMemory(m_archivePath, currentImage);
  if (data.empty())
    return false;

  IStream *pStream = SHCreateMemStream(data.data(), (UINT)data.size());
  if (pStream) {
    IWICBitmap *pWic = CreateWicFromStreamHelper(pStream, pWICFactory);
    if (pWic) {
      m_pBitmap = CreateD2DBitmapHelper(pWic, pWICFactory, m_pRenderTarget);
      pWic->Release();
    }
    pStream->Release();
  }

  // Load Secondary if Spread
  if (m_isSpread && m_currentArchiveIndex + 1 < (int)m_archiveImages.size()) {
    std::vector<uint8_t> data2 = ArchiveHelper::ExtractFileToMemory(
        m_archivePath, m_archiveImages[m_currentArchiveIndex + 1]);
    if (!data2.empty()) {
      IStream *pStream2 = SHCreateMemStream(data2.data(), (UINT)data2.size());
      if (pStream2) {
        IWICBitmap *pWic2 = CreateWicFromStreamHelper(pStream2, pWICFactory);
        if (pWic2) {
          m_pBitmapSecondary =
              CreateD2DBitmapHelper(pWic2, pWICFactory, m_pRenderTarget);
          pWic2->Release();
        }
        pStream2->Release();
      }
    }
  }

  if (m_pBitmap) {
    // Construct a logical path name for caption/vision api
    std::wstring wFilename;
    int size = MultiByteToWideChar(CP_UTF8, 0, currentImage.c_str(),
                                   (int)currentImage.size(), NULL, 0);
    if (size > 0) {
      std::vector<wchar_t> buf(size + 1);
      MultiByteToWideChar(CP_UTF8, 0, currentImage.c_str(),
                          (int)currentImage.size(), buf.data(), size);
      buf[size] = 0; // Null terminate
      wFilename = buf.data();
    }

    m_currentPath = m_archivePath + L"|" + wFilename; // Logical path
    ResetView();
    if (OnRequestRepaint)
      OnRequestRepaint();
    return true;
  }
  return false;
}

void ImageView::SetScale(float scale) {
  m_scale = scale;
  if (m_scale < 0.1f)
    m_scale = 0.1f;
  if (m_scale > 20.0f)
    m_scale = 20.0f;
  if (OnRequestRepaint)
    OnRequestRepaint();
}

void ImageView::ZoomFit(const D2D1_RECT_F &clientRect) {
  if (!m_pBitmap)
    return;

  D2D1_SIZE_F imageSize;
  if (m_isSpread && m_pBitmap && m_pBitmapSecondary) {
    D2D1_SIZE_F s1 = m_pBitmap->GetSize();
    D2D1_SIZE_F s2 = m_pBitmapSecondary->GetSize();
    imageSize =
        D2D1::SizeF(s1.width + s2.width, (std::max)(s1.height, s2.height));
  } else {
    imageSize = m_pBitmap->GetSize();
  }

  float w = clientRect.right - clientRect.left;
  float h = clientRect.bottom - clientRect.top;

  if (imageSize.width <= 0 || imageSize.height <= 0)
    return;

  float scaleX = w / imageSize.width;
  float scaleY = h / imageSize.height;

  m_scale = (std::min)(scaleX, scaleY);

  m_offsetX = 0;
  m_offsetY = 0;

  if (OnRequestRepaint)
    OnRequestRepaint();
}

void ImageView::ZoomFitWidth(const D2D1_RECT_F &clientRect) {
  if (!m_pBitmap)
    return;

  D2D1_SIZE_F imageSize;
  if (m_isSpread && m_pBitmap && m_pBitmapSecondary) {
    D2D1_SIZE_F s1 = m_pBitmap->GetSize();
    D2D1_SIZE_F s2 = m_pBitmapSecondary->GetSize();
    imageSize =
        D2D1::SizeF(s1.width + s2.width, (std::max)(s1.height, s2.height));
  } else {
    imageSize = m_pBitmap->GetSize();
  }

  float w = clientRect.right - clientRect.left;
  if (imageSize.width > 0) {
    m_scale = w / imageSize.width;
    m_offsetX = 0;
    m_offsetY = 0;
    if (OnRequestRepaint)
      OnRequestRepaint();
  }
}

void ImageView::ZoomFitHeight(const D2D1_RECT_F &clientRect) {
  if (!m_pBitmap)
    return;

  D2D1_SIZE_F imageSize;
  if (m_isSpread && m_pBitmap && m_pBitmapSecondary) {
    D2D1_SIZE_F s1 = m_pBitmap->GetSize();
    D2D1_SIZE_F s2 = m_pBitmapSecondary->GetSize();
    imageSize =
        D2D1::SizeF(s1.width + s2.width, (std::max)(s1.height, s2.height));
  } else {
    imageSize = m_pBitmap->GetSize();
  }

  float h = clientRect.bottom - clientRect.top;
  if (imageSize.height > 0) {
    m_scale = h / imageSize.height;
    m_offsetX = 0;
    m_offsetY = 0;
    if (OnRequestRepaint)
      OnRequestRepaint();
  }
}

void ImageView::Render(const D2D1_RECT_F &clientRect) {
  if (!m_pRenderTarget)
    return;

  float centerX = (clientRect.right - clientRect.left) / 2.0f;
  float centerY = (clientRect.bottom - clientRect.top) / 2.0f;

  D2D1::Matrix3x2F scale = D2D1::Matrix3x2F::Scale(
      m_scale, m_scale, D2D1::Point2F(centerX, centerY));
  D2D1::Matrix3x2F translation =
      D2D1::Matrix3x2F::Translation(m_offsetX, m_offsetY);

  m_pRenderTarget->SetTransform(scale * translation);

  if (m_isSpread && m_pBitmap && m_pBitmapSecondary) {
    D2D1_SIZE_F s1 = m_pBitmap->GetSize();
    D2D1_SIZE_F s2 = m_pBitmapSecondary->GetSize();
    float totalWidth = s1.width + s2.width;
    float maxHeight = (std::max)(s1.height, s2.height);
    float startX = centerX - (totalWidth / 2.0f);
    float startY = centerY - (maxHeight / 2.0f);

    D2D1_RECT_F dest1 =
        D2D1::RectF(startX + s2.width, startY + (maxHeight - s1.height) / 2.0f,
                    startX + s2.width + s1.width,
                    startY + (maxHeight - s1.height) / 2.0f + s1.height);
    m_pRenderTarget->DrawBitmap(m_pBitmap, dest1);

    D2D1_RECT_F dest2 = D2D1::RectF(
        startX, startY + (maxHeight - s2.height) / 2.0f, startX + s2.width,
        startY + (maxHeight - s2.height) / 2.0f + s2.height);
    m_pRenderTarget->DrawBitmap(m_pBitmapSecondary, dest2);
  } else if (m_pBitmap) {
    D2D1_SIZE_F size = m_pBitmap->GetSize();
    float x = (clientRect.right - clientRect.left - size.width) / 2.0f;
    float y = (clientRect.bottom - clientRect.top - size.height) / 2.0f;
    D2D1_RECT_F dest = D2D1::RectF(x, y, x + size.width, y + size.height);
    m_pRenderTarget->DrawBitmap(m_pBitmap, dest);
  }

  // Render Magnifying Glass if enabled and right button is down
  if (m_magEnabled && m_isRightButtonDown && m_pBitmap) {
    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    ID2D1EllipseGeometry *pEllipse = nullptr;
    D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(m_mouseX, m_mouseY),
                                         m_magSize / 2.0f, m_magSize / 2.0f);
    m_pD2DFactory->CreateEllipseGeometry(ellipse, &pEllipse);

    if (pEllipse) {
      m_pRenderTarget->PushLayer(
          D2D1::LayerParameters(D2D1::InfiniteRect(), pEllipse), nullptr);

      // We want the point under the mouse to stay under the mouse
      // Content scale is m_scale * m_magZoom
      float magScale = m_scale * m_magZoom;

      // The transform that puts IMAGE SPACE to SCREEN SPACE
      // We want: ScreenPos = MagTransform * ImagePos
      // And we want: MousePos = MagTransform * ImagePosUnderMouse
      // Where: MousePos = BaseTransform * ImagePosUnderMouse
      // So: MagTransform = Translation(MousePos) * Scale(magScale) *
      // Translation(-ImagePosUnderMouse)

      D2D1::Matrix3x2F baseTransform =
          D2D1::Matrix3x2F::Scale(
              m_scale, m_scale,
              D2D1::Point2F((clientRect.right - clientRect.left) / 2.0f,
                            (clientRect.bottom - clientRect.top) / 2.0f)) *
          D2D1::Matrix3x2F::Translation(m_offsetX, m_offsetY);

      D2D1::Matrix3x2F invBase;
      if (baseTransform.Invert()) {
        invBase = baseTransform; // Invert() modifies the object and returns
                                 // bool Wait, D2D1::Matrix3x2F::Invert is not
                                 // static and returns bool.
      }
      // Let's re-calculate baseTransform because Invert() might have changed it
      baseTransform =
          D2D1::Matrix3x2F::Scale(
              m_scale, m_scale,
              D2D1::Point2F((clientRect.right - clientRect.left) / 2.0f,
                            (clientRect.bottom - clientRect.top) / 2.0f)) *
          D2D1::Matrix3x2F::Translation(m_offsetX, m_offsetY);

      D2D1::Matrix3x2F inv;
      bool inverted = false;
      // D2D1::Matrix3x2F doesn't have a simple invert? It does.
      inv = baseTransform;
      inverted = inv.Invert();

      if (inverted) {
        D2D1_POINT_2F imgPt =
            inv.TransformPoint(D2D1::Point2F(m_mouseX, m_mouseY));

        D2D1::Matrix3x2F magTransform =
            D2D1::Matrix3x2F::Translation(-imgPt.x, -imgPt.y) *
            D2D1::Matrix3x2F::Scale(magScale, magScale) *
            D2D1::Matrix3x2F::Translation(m_mouseX, m_mouseY);

        m_pRenderTarget->SetTransform(magTransform);

        // Re-draw image(s) with mag transform
        if (m_isSpread && m_pBitmapSecondary) {
          D2D1_SIZE_F s1 = m_pBitmap->GetSize();
          D2D1_SIZE_F s2 = m_pBitmapSecondary->GetSize();
          float totalWidth = s1.width + s2.width;
          float maxHeight = (std::max)(s1.height, s2.height);
          float centerX = (clientRect.right - clientRect.left) / 2.0f;
          float centerY = (clientRect.bottom - clientRect.top) / 2.0f;
          float startX = centerX - (totalWidth / 2.0f);
          float startY = centerY - (maxHeight / 2.0f);

          D2D1_RECT_F d1 = D2D1::RectF(
              startX + s2.width, startY + (maxHeight - s1.height) / 2.0f,
              startX + s2.width + s1.width,
              startY + (maxHeight - s1.height) / 2.0f + s1.height);
          m_pRenderTarget->DrawBitmap(m_pBitmap, d1);

          D2D1_RECT_F d2 =
              D2D1::RectF(startX, startY + (maxHeight - s2.height) / 2.0f,
                          startX + s2.width,
                          startY + (maxHeight - s2.height) / 2.0f + s2.height);
          m_pRenderTarget->DrawBitmap(m_pBitmapSecondary, d2);
        } else {
          D2D1_SIZE_F sz = m_pBitmap->GetSize();
          float startX = (clientRect.right - clientRect.left - sz.width) / 2.0f;
          float startY =
              (clientRect.bottom - clientRect.top - sz.height) / 2.0f;
          D2D1_RECT_F d = D2D1::RectF(startX, startY, startX + sz.width,
                                      startY + sz.height);
          m_pRenderTarget->DrawBitmap(m_pBitmap, d);
        }
      }

      m_pRenderTarget->PopLayer();

      // Draw border
      m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
      ID2D1SolidColorBrush *pBrush = nullptr;
      m_pRenderTarget->CreateSolidColorBrush(
          D2D1::ColorF(D2D1::ColorF::White, 0.8f), &pBrush);
      if (pBrush) {
        m_pRenderTarget->DrawEllipse(ellipse, pBrush, 2.0f);
        pBrush->Release();
      }
      pEllipse->Release();
    }
  }

  m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
}

void ImageView::OnRightMouseDown(float x, float y) {
  m_mouseX = x;
  m_mouseY = y;
  m_isRightButtonDown = true;
  if (OnRequestRepaint)
    OnRequestRepaint();
}

void ImageView::OnRightMouseUp() {
  m_isRightButtonDown = false;
  if (OnRequestRepaint)
    OnRequestRepaint();
}

void ImageView::OnWheel(float delta, float x, float y) {
  if (m_isSpread) {
    // In spread mode, wheel navigates pairs
    bool next = (delta < 0); // Scroll down -> next
    if (m_isArchive) {
      if (next) {
        m_currentArchiveIndex += 2;
      } else {
        m_currentArchiveIndex -= 2;
      }
      ReloadCurrent();
    } else {
      if (OnNavigate)
        OnNavigate(next);
    }
    return;
  }

  // Original zoom logic for non-spread
  float factor = (delta > 0) ? 1.1f : 0.9f;
  m_scale *= factor;
  if (m_scale < 0.1f)
    m_scale = 0.1f;
  if (m_scale > 20.0f)
    m_scale = 20.0f;
  if (OnRequestRepaint)
    OnRequestRepaint();
}

void ImageView::OnMouseDown(float x, float y) {
  m_isDragging = true;
  m_lastMouseX = x;
  m_lastMouseY = y;
}

void ImageView::OnMouseMove(float x, float y) {
  m_mouseX = x;
  m_mouseY = y;

  if (m_isDragging) {
    float dx = x - m_lastMouseX;
    float dy = y - m_lastMouseY;

    m_offsetX += dx / m_scale;
    m_offsetY += dy / m_scale;

    m_lastMouseX = x;
    m_lastMouseY = y;
  }

  if (OnRequestRepaint)
    OnRequestRepaint();
}

void ImageView::OnMouseUp(float x, float width) {
  if (!m_isDragging) {
    // Click Navigation
    // Left 20% -> Prev, Right 20% -> Next
    // Or simple Left/Right split
    if (width > 0) {
      float ratio = x / width;
      if (ratio < 0.2f) {
        // Prev
        if (m_isArchive) {
          m_currentArchiveIndex -= (m_isSpread ? 2 : 1);
          ReloadCurrent();
        } else {
          if (OnNavigate)
            OnNavigate(false);
        }
      } else if (ratio > 0.8f) {
        // Next
        if (m_isArchive) {
          m_currentArchiveIndex += (m_isSpread ? 2 : 1);
          ReloadCurrent();
        } else {
          if (OnNavigate)
            OnNavigate(true);
        }
      }
    }
  }
  m_isDragging = false;
}

void ImageView::ReloadCurrent() {
  if (m_isArchive && m_pWICFactoryCached) {
    LoadFromArchiveInternal(m_pWICFactoryCached);
  }
}

void ImageView::OnKeyDown(WPARAM key) {
  float step = 50.0f;
  if (key == VK_RIGHT) {
    // Pan Right (move view left)
    m_offsetX -= step / m_scale;
    if (OnRequestRepaint)
      OnRequestRepaint();
  } else if (key == VK_LEFT) {
    // Pan Left (move view right)
    m_offsetX += step / m_scale;
    if (OnRequestRepaint)
      OnRequestRepaint();
  } else if (key == VK_DOWN) {
    // Pan Down
    m_offsetY -= step / m_scale;
    if (OnRequestRepaint)
      OnRequestRepaint();
  } else if (key == VK_UP) {
    // Pan Up
    m_offsetY += step / m_scale;
    if (OnRequestRepaint)
      OnRequestRepaint();
  }
  // Remove nav logic effectively, or maybe page up/down?
}

bool ImageView::LoadSecondaryImageFile(const std::wstring &path,
                                       IWICImagingFactory *pWICFactory,
                                       void *pluginManager) {
  if (m_pBitmapSecondary) {
    m_pBitmapSecondary->Release();
    m_pBitmapSecondary = nullptr;
  }
  if (!m_pRenderTarget)
    return false;

  IWICBitmap *pWic = LoadWicBitmapHelper(path, pWICFactory, pluginManager);
  if (pWic) {
    m_pBitmapSecondary =
        CreateD2DBitmapHelper(pWic, pWICFactory, m_pRenderTarget);
    pWic->Release();
  }
  return m_pBitmapSecondary != nullptr;
}

#pragma once
#include <windows.h>
#include <wincodec.h>
#include <string>
#include <propvarutil.h>

class MetadataHelper {
public:
    static std::wstring GetResolution(IWICBitmapFrameDecode* pFrame) {
        UINT width = 0, height = 0;
        if (SUCCEEDED(pFrame->GetSize(&width, &height))) {
            return std::to_wstring(width) + L" x " + std::to_wstring(height);
        }
        return L"";
    }

    static std::wstring GetDateTaken(IWICBitmapFrameDecode* pFrame) {
        IWICMetadataQueryReader* pMetadataReader = NULL;
        std::wstring dateTaken = L"";

        if (SUCCEEDED(pFrame->GetMetadataQueryReader(&pMetadataReader))) {
            PROPVARIANT value;
            PropVariantInit(&value);
            
            // Try common EXIF tags
            // DateTime is usually tag 0x0132, or "System.Photo.DateTaken"
            // Using WIC Query Language
            if (SUCCEEDED(pMetadataReader->GetMetadataByName(L"/app1/ifd/exif/{ushort=36867}", &value)) || // DateTimeOriginal
                SUCCEEDED(pMetadataReader->GetMetadataByName(L"/app1/ifd/exif/{ushort=306}", &value))) {   // DateTime
                
                if (value.vt == VT_LPSTR) {
                    // Convert ASCII to wstring
                    std::string str(value.pszVal);
                    dateTaken = std::wstring(str.begin(), str.end());
                }
            }
            PropVariantClear(&value);
            pMetadataReader->Release();
        }
        return dateTaken;
    }
};

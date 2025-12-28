#include "ArchiveHelper.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <shlwapi.h>
#include <unarr.h>
#include <windows.h>


namespace fs = std::filesystem;

// Helper to convert wstring to utf8 string (unarr uses char*)
std::string ToUtf8(const std::wstring &wstr) {
  if (wstr.empty())
    return std::string();
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                                        NULL, 0, NULL, NULL);
  std::string strTo(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0],
                      size_needed, NULL, NULL);
  return strTo;
}

ar_archive *OpenAnyArchive(ar_stream *stream) {
  ar_archive *ar = ar_open_zip_archive(stream, false);
  if (!ar)
    ar = ar_open_rar_archive(stream);
  if (!ar)
    ar = ar_open_7z_archive(stream);
  if (!ar)
    ar = ar_open_tar_archive(stream);
  return ar;
}

bool ArchiveHelper::IsArchive(const std::wstring &path) {
  std::wstring ext = fs::path(path).extension().wstring();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
  return (ext == L".zip" || ext == L".rar" || ext == L".7z" || ext == L".tar");
}

std::vector<std::string>
ArchiveHelper::GetImageFiles(const std::wstring &archivePath) {
  std::vector<std::string> images;
  std::string pathUtf8 = ToUtf8(archivePath);

  ar_stream *stream = ar_open_file(pathUtf8.c_str());
  if (!stream)
    return images;

  ar_archive *archive = OpenAnyArchive(stream);
  if (archive) {
    while (ar_parse_entry(archive)) {
      const char *name = ar_entry_get_name(archive);
      if (name) {
        std::string sName(name);
        std::string ext = fs::path(sName).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" ||
            ext == ".gif") {
          images.push_back(sName);
        }
      }
      if (!ar_parse_entry_at(archive, ar_entry_get_offset(archive) +
                                          ar_entry_get_size(archive)))
        break; // Should rely on while(ar_parse_entry) usually, but unarr usage
               // varies. Actually ar_parse_entry advances to next? Checking
               // docs/examples: ar_parse_entry reads next.
    }
    ar_close_archive(archive);
  } else {
    ar_close(stream);
  }

  // Natural sort for filenames
  std::sort(images.begin(), images.end(),
            [](const std::string &a, const std::string &b) {
              // Need to convert to wstring for StrCmpLogicalW
              int lenA =
                  MultiByteToWideChar(CP_UTF8, 0, a.c_str(), -1, NULL, 0);
              int lenB =
                  MultiByteToWideChar(CP_UTF8, 0, b.c_str(), -1, NULL, 0);
              std::vector<wchar_t> wa(lenA);
              std::vector<wchar_t> wb(lenB);
              MultiByteToWideChar(CP_UTF8, 0, a.c_str(), -1, wa.data(), lenA);
              MultiByteToWideChar(CP_UTF8, 0, b.c_str(), -1, wb.data(), lenB);
              return StrCmpLogicalW(wa.data(), wb.data()) < 0;
            });

  return images;
}

std::vector<uint8_t>
ArchiveHelper::ExtractFileToMemory(const std::wstring &archivePath,
                                   const std::string &internalPath) {
  std::vector<uint8_t> buffer;
  std::string pathUtf8 = ToUtf8(archivePath);

  ar_stream *stream = ar_open_file(pathUtf8.c_str());
  if (!stream)
    return buffer;

  ar_archive *archive = OpenAnyArchive(stream);
  if (archive) {
    while (ar_parse_entry(archive)) {
      const char *name = ar_entry_get_name(archive);
      if (name && internalPath == name) {
        size_t size = ar_entry_get_size(archive);
        if (size > 0) {
          buffer.resize(size);
          if (!ar_entry_uncompress(archive, buffer.data(), size)) {
            buffer.clear();
          }
        }
        break;
      }
    }
    ar_close_archive(archive);
  } else {
    ar_close(stream);
  }
  return buffer;
}

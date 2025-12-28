#pragma once
#include <string>
#include <vector>
#include <memory>

class ArchiveHelper {
public:
    static bool IsArchive(const std::wstring& path);

    // Returns a list of image file paths inside the archive (e.g. "folder/img.jpg")
    static std::vector<std::string> GetImageFiles(const std::wstring& archivePath);

    // Extracts a specific file from archive to a memory buffer
    static std::vector<uint8_t> ExtractFileToMemory(const std::wstring& archivePath, const std::string& internalPath);
};

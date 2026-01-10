#pragma once
#include <map>
#include <string>
#include <vector>
#include <wincodec.h>
#include <windows.h>

enum KeyAction { KA_MOVE, KA_COPY };

class MoveHelper {
public:
  static MoveHelper &Instance();

  void SetKeyAction(KeyAction action);
  KeyAction GetKeyAction() const;

  void SetFolder(int key, const std::wstring &path);
  std::wstring GetFolder(int key);

  // Shows Confirmation Dialog and Moves Files if confirmed.
  // Returns true if any moves happened.
  bool RequestMove(HWND hParent, const std::vector<std::wstring> &filePaths,
                   int key, IWICImagingFactory *pFactory);
  bool RequestCopy(HWND hParent, const std::vector<std::wstring> &filePaths,
                   int key, IWICImagingFactory *pFactory);

  // Context Menu Operations
  bool RenameFile(HWND hParent,
                  const std::wstring &filePath); // Rename remains single file
  bool DeleteFiles(HWND hParent, const std::vector<std::wstring> &filePaths);

  bool CutFile(HWND hParent, const std::wstring &filePath);

  // Dialog Proc for Folder Settings
  static INT_PTR CALLBACK FolderSettingsDlgProc(HWND hDlg, UINT message,
                                                WPARAM wParam, LPARAM lParam);

private:
  MoveHelper();
  std::map<int, std::wstring> m_folderMap;
  KeyAction m_keyAction = KA_MOVE;

  // Move Confirmation State
  struct ConfirmState {
    std::vector<std::wstring> sourcePaths;
    std::wstring destFolder;
    IWICImagingFactory *pFactory;
    HICON hIcon;
  };

  static ConfirmState s_confirmState;

  static INT_PTR CALLBACK MoveConfirmDlgProc(HWND hDlg, UINT message,
                                             WPARAM wParam, LPARAM lParam);
  static bool PerformMove(HWND hParent, const std::wstring &src,
                          const std::wstring &dst);
  static HBITMAP GenerateThumbnail(const std::wstring &path,
                                   IWICImagingFactory *pFactory, int width,
                                   int height);
};

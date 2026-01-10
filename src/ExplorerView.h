#include "ArchiveHelper.h"
#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

// Forward declarations for CommCtrl types to avoid including the massive header
// here and causing include order issues (prsht.h/windows.h conflicts).
typedef struct _IMAGELIST *HIMAGELIST;
typedef struct _TREEITEM *HTREEITEM;
typedef struct tagLVITEMW LVITEMW;

enum ExplorerViewMode { EV_THUMBNAILS, EV_DETAILS };

class ExplorerView {
public:
  ExplorerView();
  ~ExplorerView();

  void Create(HWND hParent, HINSTANCE hInst);
  void Resize(int x, int y, int width, int height);

  // Commands
  void Refresh();
  void SetViewMode(ExplorerViewMode mode);
  ExplorerViewMode GetViewMode() const { return m_currentMode; }
  void SetThumbnailSize(int size);

  // Navigation
  void NavigateTo(const std::wstring &path);
  const std::wstring &GetCurrentFolder() const { return m_currentFolder; }

  // Callbacks
  std::function<void(const std::wstring &)>
      OnFileSelected; // Single click or selection change
  std::function<void(const std::wstring &, const std::vector<std::wstring> &)>
      OnFileOpen; // Double click (starts viewer)

  // Window Procedures
  LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
  HWND m_hParent;
  HINSTANCE m_hInst;
  HWND m_hTree;
  HWND m_hList;
  HWND m_hSplitter;

  ExplorerViewMode m_currentMode;
  int m_thumbnailSize;
  int m_splitterPos;
  bool m_draggingSplitter;

  std::wstring m_currentFolder;
  std::vector<std::wstring>
      m_currentFileList; // Store current folder's file list for navigation

  // Image list for thumbnails
  HIMAGELIST m_hImageListThumb;
  HIMAGELIST m_hImageListSys;

  void PopulateTree(const std::wstring &rootPath);
  void PopulateList(const std::wstring &folderPath);
  void AddChildFolders(HTREEITEM hParent, const std::wstring &path);
  void AddDummyNode(HTREEITEM hItem);
  std::vector<std::wstring> GetSelectedPaths();

  std::thread m_thumbThread;
  std::atomic<bool> m_cancelThumb;

  // Helpers
  std::wstring GetPathFromTreeItem(HTREEITEM hItem);
  HTREEITEM FindTreeItem(HTREEITEM hParent, const std::wstring &name);
  void SetupColumns();
  void GenerateThumbnailsThread();
};

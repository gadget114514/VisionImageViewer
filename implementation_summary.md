# Implementation Summary: Multi-File Operations & Context Menu Enhancements

## Features Implemented

1.  **Multi-File Support**:
    -   `ExplorerView::GetSelectedPaths()` added to retrieve all selected items.
    -   `MoveHelper::RequestMove`, `DeleteFiles`, and the new `RequestCopy` now support `std::vector<std::wstring>` for batch operations.
    -   `SHFileOperationW` is used for Move, Copy, and Delete, providing native Windows progress dialogs and Undo capability.

2.  **Context Menu Enhancements**:
    -   **"Move to" Submenu**: Dynamically populated with configured folder names (1-9, 0).
    -   **"Copy to" Submenu**: Added a new submenu identical to "Move to" for copying files.
    -   **"Open in Explorer"**: Added context menu item to open the selected file/folder in Windows Explorer.
    -   **"Delete"**: Now supports deleting multiple selected files.

3.  **Keyboard Shortcuts & Configuration**:
    -   **0-9 Keys**: Pressing 0-9 in the file list now triggers either "Move to" or "Copy to" for the selected file(s).
    -   **Configuration Dialog**: Added "Key Bindings (0-9)" section with Radio Buttons to toggle between "Move to Folder" (Default) and "Copy to Folder".
    -   **Persistence**: The KeyAction setting is saved to `VisionImageViewer.ini`.

4.  **Technical Debt & Lint Fixes**:
    -   Cleaned up `MoveHelper.cpp` and `ExplorerView.cpp`.
    -   Ensured proper Unicode API usage (`MessageBoxW`, `SHFileOperationW`, etc.).
    -   Resolved missing `IDM_CTX_DELETE` handler.

## Build Status
-   Project builds successfully (Debug/Release).
-   `VisionImageViewer.exe` is located in `build/Debug/`.

## Dependencies
-   Uses `ShellExecuteW` and `SHFileOperationW` for shell interactions.
-   Uses standard `std::filesystem` (aliased as `stdfs`).

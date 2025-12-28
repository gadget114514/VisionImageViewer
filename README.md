# VisionImageViewer

A high-performance Windows image viewer with advanced features including Susie plugin support, archive handling, EXIF metadata viewing, and a powerful file explorer interface.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)

## ✨ Features

### 🖼️ Image Viewing
- **High-Performance Rendering**: Hardware-accelerated rendering using Direct2D
- **Wide Format Support**: Built-in support for common image formats via WIC (Windows Imaging Component)
- **Susie Plugin Support**: Compatible with Susie plugins for extended format support
- **Archive Support**: View images directly from ZIP, RAR, and other archive formats without extraction
- **Dual-Page Spread View**: View two images side-by-side for manga/comic reading
- **Magnifying Glass**: Real-time magnification tool for detailed image inspection (configurable size and zoom)

### 📁 File Management
- **Explorer Interface**: Integrated file explorer with tree and list views
- **Multiple View Modes**: Switch between thumbnail and detail views
- **Batch Operations**: Multi-select for move, copy, and delete operations
- **Quick File Organization**: 
  - Configure up to 10 destination folders (0-9)
  - Press number keys to quickly move or copy files
  - Native Windows shell operations with progress dialogs and undo support
- **Context Menu Integration**: 
  - Copy/Move to configured folders
  - Delete files
  - Open in Windows Explorer
  - Rename files

### 📊 Metadata & Information
- **Comprehensive EXIF Viewer**: Display detailed camera and photo information
  - Camera make, model, and lens information
  - Exposure settings (aperture, shutter speed, ISO, exposure bias)
  - GPS location data
  - Date/time information with subsecond precision
  - Orientation and flash information
  - Software and copyright details
- **Image Properties**: View resolution, DPI, bit depth, and format details via WIC

### ⚙️ Customization
- **Plugin Management**: Enable/disable and reorder Susie plugins
- **Configurable Hotkeys**: Choose between move or copy mode for number key shortcuts
- **Persistent Settings**: Automatically saves and restores:
  - Folder configurations
  - Last viewed directory
  - View preferences
  - Magnifier settings
  - Plugin states

## 🚀 Getting Started

### Prerequisites
- Windows 10 or later (64-bit)
- Visual Studio 2019 or later (for building from source)
- CMake 3.20 or later
- vcpkg (optional, for dependency management)

### Installation

#### Binary Release
1. Download the latest release from the [Releases](../../releases) page
2. Extract the archive to your preferred location
3. Run `VisionImageViewer.exe`
4. (Optional) Place Susie plugins in the `plugins` subfolder to extend format support

#### Building from Source

1. **Clone the repository**
   ```bash
   git clone https://github.com/gadget114514/VisionImageViewer.git
   cd VisionImageViewer
   ```

2. **Initialize submodules** (if any)
   ```bash
   git submodule update --init --recursive
   ```

3. **Build with CMake**
   ```bash
   mkdir build
   cd build
   cmake ..
   cmake --build . --config Release
   ```

4. **Run the application**
   ```bash
   cd Release
   VisionImageViewer.exe
   ```

## 📖 Usage

### Basic Navigation
- **Open Image**: `File > Open` or double-click an image in the explorer view
- **Navigate Images**: Use arrow keys or mouse wheel to browse through images in the current folder
- **Zoom**: 
  - Mouse wheel to zoom in/out
  - `F` - Fit to window
  - `W` - Fit width
  - `H` - Fit height
- **Pan**: Click and drag to pan around zoomed images

### File Organization
1. **Configure Folders**: `Settings > Folder Settings`
   - Assign up to 10 destination folders (keys 0-9)
   - Enter folder path or browse
2. **Quick Move/Copy**:
   - Select file(s) in the explorer view
   - Press 0-9 to move/copy to configured folder
   - Switch between move/copy mode in `Settings > Configuration`

### Magnifying Glass
- **Enable**: `Settings > Configuration` → Check "Enable Magnifier"
- **Use**: Hold right mouse button over image to zoom in
- **Configure**: Adjust magnifier size and zoom level in settings

### Context Menu
Right-click on files in the explorer view for quick access to:
- Move to folder (submenu)
- Copy to folder (submenu)
- Delete
- Rename
- Open in Explorer

### EXIF Metadata
- Select an image in the explorer view
- Choose `View > Properties` or right-click > `Properties`
- View comprehensive EXIF metadata and image information

## 🔌 Plugin Support

VisionImageViewer supports Susie plugins for extended image format support:

1. Create a `plugins` folder in the same directory as the executable
2. Place Susie plugin DLLs (`.spi` files) in the plugins folder
3. Restart the application
4. Configure plugins via `Help > Plugins` menu
   - Enable/disable individual plugins
   - Reorder plugin priority
   - View supported file extensions

**Note**: Both 32-bit and 64-bit plugins are supported, but must match the application architecture.

## ⚙️ Configuration

### Settings File
Settings are stored in `VisionImageViewer.ini` in the same directory as the executable:

```ini
[Folders]
Folder0=C:\Images\Archive
Folder1=C:\Images\Favorites
...

[Settings]
KeyAction=0          ; 0=Move, 1=Copy
LastFolder=C:\Images
MagEnabled=1         ; 0=Disabled, 1=Enabled
MagSize=200          ; Magnifier size in pixels
MagZoom=2            ; Magnification level

[Plugins]
; Plugin states and order
```

## 🏗️ Architecture

### Core Components
- **ImageView**: High-performance image rendering with Direct2D and WIC
- **ExplorerView**: File system browser with tree and list views
- **PluginManager**: Susie plugin loader and image format handler
- **MoveHelper**: File operation manager with Windows shell integration
- **ArchiveHelper**: Archive file reader (ZIP, RAR support via unarr)

### Dependencies
- **Direct2D**: Hardware-accelerated 2D graphics
- **WIC (Windows Imaging Component)**: Native image codec support
- **DirectWrite**: Text rendering
- **TinyEXIF** (MIT): EXIF metadata parsing
- **nlohmann/json** (MIT): JSON configuration (via CMake FetchContent)
- **unarr** (LGPL-3.0): Archive file reading

## 🤝 Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

### Development Setup
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Code Style
- C++17 standard
- Use Unicode APIs (`wchar_t`, `std::wstring`)
- Follow existing code formatting conventions

## 📝 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **TinyEXIF** (MIT): Lightweight EXIF parser library
- **unarr** (LGPL-3.0): Archive extraction library
- **Susie Plugin Specification**: Legacy but powerful plugin system for Japanese image viewers
- **nlohmann/json** (MIT): Modern C++ JSON library

## 📧 Contact

**Project Link**: [https://github.com/gadget114514/VisionImageViewer](https://github.com/gadget114514/VisionImageViewer)

---

<p align="center">Made with ❤️ for image enthusiasts</p>

made with gemini 3 pro

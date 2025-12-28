@echo off
echo Building NativeImageViewer and Tests...

if not exist build mkdir build
cd build

echo Configuring CMake...
cmake ..
if %errorlevel% neq 0 (
    echo CMake Configuration Failed. Ensure CMake is in PATH.
    pause
    exit /b %errorlevel%
)

echo Building Debug configuration...
cmake --build . --config Debug
if %errorlevel% neq 0 (
    echo Build Failed.
    pause
    exit /b %errorlevel%
)

echo Build Success!
echo Run apps located in build/Debug/
echo .
pause

@echo off
setlocal

echo ==========================================
echo  AudioDeviceSwitcher - Build
echo ==========================================
echo.

:: Find vswhere.exe
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] vswhere.exe not found. Install Visual Studio 2019/2022.
    pause & exit /b 1
)

:: Find VS installation with C++ tools
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)
if "%VS_PATH%"=="" (
    echo [ERROR] No Visual Studio with C++ tools found.
    echo         Open Visual Studio Installer and add "Desktop development with C++".
    pause & exit /b 1
)

set "VCVARS=%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat"
echo [1/5] MSVC: %VCVARS%
call "%VCVARS%" x64 > nul 2>&1

:: Find cmake
set "CMAKE=cmake"
where cmake > nul 2>&1
if errorlevel 1 (
    if exist "C:\Program Files\CMake\bin\cmake.exe" (
        set "CMAKE=C:\Program Files\CMake\bin\cmake.exe"
    ) else (
        echo [ERROR] cmake not found. Run: winget install --id Kitware.CMake
        pause & exit /b 1
    )
)
echo [2/5] cmake: %CMAKE%

:: Find ninja
where ninja > nul 2>&1
if errorlevel 1 (
    echo [ERROR] ninja not found. Run: winget install --id Ninja-build.Ninja
    pause & exit /b 1
)

:: Clean
echo [3/5] Cleaning old build...
if exist build rd /s /q build

:: Configure
echo [4/5] CMake configure (Ninja + MSVC Release)...
"%CMAKE%" -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    pause & exit /b 1
)

:: Build
echo [5/5] Building...
"%CMAKE%" --build build
if errorlevel 1 (
    echo [ERROR] Build failed.
    pause & exit /b 1
)

echo.
echo Build complete!
if exist "build\AudioDeviceSwitcher.exe" (
    powershell -NoProfile -Command "$f=Get-Item 'build\AudioDeviceSwitcher.exe'; Write-Host 'Output:' $f.FullName; Write-Host 'Size:' $f.Length 'bytes (' ([math]::Round($f.Length/1KB,1)) 'KB)'"
)
echo.
echo ==========================================
pause
endlocal

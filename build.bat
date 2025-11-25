@echo off
chcp 65001 > nul
echo ==========================================
echo AudioDeviceSwitcher - 构建脚本
echo ==========================================
echo.

echo [1/3] 清理旧文件...
if exist bin rd /s /q bin
if exist obj rd /s /q obj

echo [2/3] 编译项目...
dotnet publish -c Release -r win-x64 --self-contained false -p:PublishSingleFile=true

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo 构建失败！
    pause
    exit /b 1
)

echo [3/3] 完成！
echo.
echo 输出文件: bin\Release\net8.0-windows\win-x64\publish\AudioDeviceSwitcher.exe
echo.
powershell -NoProfile -Command "$file = Get-Item 'bin\Release\net8.0-windows\win-x64\publish\AudioDeviceSwitcher.exe'; Write-Host '文件名:' $file.Name; Write-Host '大小:' $file.Length '字节 (' ([math]::Round($file.Length/1KB, 1)) ' KB)'"
echo.
echo ==========================================
pause


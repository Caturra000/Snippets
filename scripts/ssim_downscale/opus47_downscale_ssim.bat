@echo off
:: 强制 CMD 控制台输出/输入使用 UTF-8
chcp 65001 >nul
setlocal

if "%~1"=="" (
    echo 请将一个或多个 4K 图片拖到本批处理上。
    pause
    exit /b
)

:: 记录真实路径和脚本路径
set "REAL_SCRIPT_DIR=%~dp0"
set "PS1_PATH=%~dp0opus47_downscale_ssim.ps1"

:: 方案 A：检查是否存在现代版 PowerShell 7 (pwsh.exe)
:: pwsh 原生支持无 BOM 的 UTF-8 脚本，如果有直接用它运行
where pwsh >nul 2>nul
if %ERRORLEVEL% equ 0 (
    pwsh -NoProfile -ExecutionPolicy Bypass -File "%PS1_PATH%" %*
    pause
    exit /b
)

:: 方案 B：兼容 Windows 自带的 PowerShell 5.1 (powershell.exe)
:: 因为 PS 5.1 读取无 BOM 的 UTF-8 会导致中文乱码，
:: 这里通过命令在 %TEMP% 动态生成一个【带 BOM】的副本来执行，保护源文件不带 BOM。
set "TEMP_PS1=%TEMP%\~run_ps1_%RANDOM%.ps1"
powershell -NoProfile -Command "[IO.File]::WriteAllText($env:TEMP_PS1, [IO.File]::ReadAllText($env:PS1_PATH, [Text.Encoding]::UTF8), [Text.Encoding]::UTF8)"

:: 把所有拖入参数透传给这个临时 PowerShell 脚本
powershell -NoProfile -ExecutionPolicy Bypass -File "%TEMP_PS1%" %*

:: 执行完毕后清理无痕迹
if exist "%TEMP_PS1%" del "%TEMP_PS1%"

pause
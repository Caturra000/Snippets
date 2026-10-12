@echo off
setlocal DisableDelayedExpansion

set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"

"%PS%" -NoProfile -ExecutionPolicy Bypass ^
  -File "%~dp0Convert.ps1" %*

exit /b %ERRORLEVEL%
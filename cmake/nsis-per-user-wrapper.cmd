@echo off
setlocal

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0PatchNsisPerUser.ps1" %*
exit /b %ERRORLEVEL%

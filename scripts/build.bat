@echo off
setlocal
pushd "%~dp0.."
set "R2N64_MODE=%~1"
if not defined R2N64_MODE set "R2N64_MODE=ps4"
wsl.exe -d Ubuntu-24.04 -- bash scripts/build.sh %R2N64_MODE%
set "R2N64_RESULT=%ERRORLEVEL%"
popd
exit /b %R2N64_RESULT%


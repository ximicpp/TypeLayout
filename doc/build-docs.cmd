@echo off
REM Build Boost.TypeLayout Documentation (Windows)
REM
REM This script builds the Antora documentation site.
REM Prerequisites: Node.js 18+ and npm installed
REM
REM Usage:
REM   build-docs.cmd          - Build documentation
REM   build-docs.cmd serve    - Build and serve locally
REM   build-docs.cmd clean    - Clean build artifacts

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"
set "SITE_DIR=%BUILD_DIR%\site"

REM Parse argument
set "ACTION=%~1"
if "%ACTION%"=="" set "ACTION=build"

if "%ACTION%"=="build" goto :build
if "%ACTION%"=="serve" goto :serve
if "%ACTION%"=="clean" goto :clean
goto :usage

:check_prerequisites
echo [INFO] Checking prerequisites...

where node >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Node.js is not installed. Please install Node.js 18 or later.
    exit /b 1
)

for /f "tokens=1 delims=v." %%i in ('node -v') do set "NODE_VER=%%i"
for /f "tokens=2 delims=v." %%i in ('node -v') do set "NODE_VER=%%i"
if !NODE_VER! lss 18 (
    echo [WARN] Node.js version !NODE_VER! detected. Version 18+ recommended.
)

where npm >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] npm is not installed.
    exit /b 1
)

echo [INFO] Prerequisites OK
exit /b 0

:install_antora
echo [INFO] Installing Antora...

if not exist "%SCRIPT_DIR%node_modules\@antora" (
    cd /d "%SCRIPT_DIR%"
    call npm install @antora/cli @antora/site-generator
) else (
    echo [INFO] Antora already installed
)
exit /b 0

:build_docs
echo [INFO] Building documentation...

cd /d "%SCRIPT_DIR%"
call npx antora --fetch antora-playbook.yml

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Documentation build failed
    exit /b 1
)

echo [INFO] Documentation built successfully!
echo [INFO] Output: %SITE_DIR%
exit /b 0

:serve_docs
echo [INFO] Starting local server...

where python >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Python required for local server
    exit /b 1
)

cd /d "%SITE_DIR%"
echo [INFO] Serving at http://localhost:8000
echo [INFO] Press Ctrl+C to stop

python -m http.server 8000
exit /b 0

:clean_build
echo [INFO] Cleaning build artifacts...

if exist "%BUILD_DIR%" (
    rmdir /s /q "%BUILD_DIR%"
    echo [INFO] Removed %BUILD_DIR%
)

if exist "%SCRIPT_DIR%node_modules" (
    rmdir /s /q "%SCRIPT_DIR%node_modules"
    echo [INFO] Removed node_modules
)

if exist "%SCRIPT_DIR%package-lock.json" (
    del "%SCRIPT_DIR%package-lock.json"
)

echo [INFO] Clean complete
exit /b 0

:build
call :check_prerequisites
if %ERRORLEVEL% neq 0 exit /b 1
call :install_antora
if %ERRORLEVEL% neq 0 exit /b 1
call :build_docs
exit /b %ERRORLEVEL%

:serve
call :check_prerequisites
if %ERRORLEVEL% neq 0 exit /b 1
call :install_antora
if %ERRORLEVEL% neq 0 exit /b 1
call :build_docs
if %ERRORLEVEL% neq 0 exit /b 1
call :serve_docs
exit /b %ERRORLEVEL%

:clean
call :clean_build
exit /b 0

:usage
echo Usage: %~nx0 {build^|serve^|clean}
exit /b 1

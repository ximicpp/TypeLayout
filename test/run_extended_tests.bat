@echo off
REM Run extended signature tests in Docker container
REM Usage: test\run_extended_tests.bat

setlocal EnableDelayedExpansion

set DOCKER_IMAGE=ghcr.io/ximicpp/typelayout-p2996:latest
set WORKSPACE=/workspace

echo ==================================================
echo TypeLayout Extended Signature Tests
echo ==================================================
echo.

REM Get current directory and convert to Docker path
set "CURRENT_DIR=%cd%"
REM Convert Windows path to Docker format (e.g., G:\workspace -> /g/workspace for Git Bash mount)
set "DOCKER_PATH=%CURRENT_DIR:\=/%"
set "DOCKER_PATH=%DOCKER_PATH:~0,1%%DOCKER_PATH:~2%"
set "DOCKER_PATH=/%DOCKER_PATH%"

echo Current directory: %CURRENT_DIR%
echo Docker mount path: %DOCKER_PATH%
echo.

REM Pull latest image
echo Pulling Docker image...
docker pull %DOCKER_IMAGE%
if errorlevel 1 (
    echo ERROR: Failed to pull Docker image
    exit /b 1
)

echo.
echo Running tests in Docker container...
docker run --rm -v "%DOCKER_PATH%:%WORKSPACE%" -w %WORKSPACE% %DOCKER_IMAGE% bash -c ^
"echo '=== Compiling test_signature_extended ===' && ^
/opt/llvm-p2996/bin/clang++ -std=c++26 -stdlib=libc++ -freflection -I%WORKSPACE%/include -o /tmp/test_signature_extended %WORKSPACE%/test/test_signature_extended.cpp 2>&1 && ^
echo '' && ^
echo '=== Running Extended Tests ===' && ^
/tmp/test_signature_extended && ^
echo '' && ^
echo '=== Test Summary ===' && ^
echo 'All types compiled and ran successfully!'"

if errorlevel 1 (
    echo.
    echo ERROR: Tests failed!
    exit /b 1
)

echo.
echo ==================================================
echo Extended tests completed successfully!
echo ==================================================

endlocal

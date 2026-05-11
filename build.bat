@echo off
setlocal

set SRC=AIProcess-Terminate.cpp
set OUT=AIProcess-Terminate.exe
set OBJ_DIR=build_obj

if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

cl /W4 /WX /std:c++17 /EHsc /O2 /D_UNICODE /DUNICODE ^
   /Fo"%OBJ_DIR%\\" /Fe"%OUT%" ^
   %SRC% ^
   /link psapi.lib comctl32.lib user32.lib /SUBSYSTEM:WINDOWS

if %ERRORLEVEL% neq 0 (
    echo.
    echo [FAILED] Build error — see output above.
    exit /b %ERRORLEVEL%
)

echo.
echo [OK] Built: %OUT%
endlocal

@echo off
setlocal

set SRC=AIProcess-Terminate.cpp
set OUT=AIProcess-Terminate.exe
set OBJ_DIR=build_obj
set RC_SRC=resource.rc
set RC_OBJ=%OBJ_DIR%\resource.res

if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

rc /nologo /fo "%RC_OBJ%" "%RC_SRC%"
if %ERRORLEVEL% neq 0 (
    echo.
    echo [FAILED] Resource compiler error.
    exit /b %ERRORLEVEL%
)

cl /W4 /WX /std:c++17 /EHsc /O2 /D_UNICODE /DUNICODE ^
   /Fo"%OBJ_DIR%\\" /Fe"%OUT%" ^
   %SRC% ^
   /link psapi.lib comctl32.lib user32.lib /SUBSYSTEM:WINDOWS "%RC_OBJ%"

if %ERRORLEVEL% neq 0 (
    echo.
    echo [FAILED] Build error — see output above.
    exit /b %ERRORLEVEL%
)

echo.
echo [OK] Built: %OUT%
endlocal

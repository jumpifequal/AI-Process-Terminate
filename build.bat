@echo off
setlocal

rem ============================================================
rem  If no VS environment is active, auto-bootstrap via vswhere.
rem  The target architecture is taken from the active environment;
rem  call vcvars32.bat / vcvars64.bat beforehand to override.
rem ============================================================

if defined VCINSTALLDIR goto :BUILD

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] vswhere.exe not found. Install Visual Studio 2022 with the
    echo         "Desktop development with C++" workload, or source vcvarsall.bat
    echo         manually before running this script.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (
    `"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`
) do set "VS_PATH=%%i"

if not defined VS_PATH (
    echo [ERROR] No Visual Studio installation with C++ tools found.
    exit /b 1
)

call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] vcvars64.bat failed.
    exit /b %ERRORLEVEL%
)

:BUILD
rem ============================================================
rem  Build
rem ============================================================

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
   /link psapi.lib comctl32.lib user32.lib /SUBSYSTEM:WINDOWS /MANIFEST:NO "%RC_OBJ%"

if %ERRORLEVEL% neq 0 (
    echo.
    echo [FAILED] Build error — see output above.
    exit /b %ERRORLEVEL%
)

echo.
echo [OK] Built: %OUT%  (%VSCMD_ARG_TGT_ARCH%)
endlocal

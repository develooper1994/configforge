@echo off
rem MSVC build and test of a worktree: ACCEPT-WINDOWS's build, with its
rem build directory under %CF_WORK% (default D:\cf-work when D: exists,
rem else %TEMP%\cf-work). Visual Studio is found with vswhere.
rem usage: scripts\work\win.cmd <worktree dir> <id> [ctest regex, no "|"]
setlocal
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do set VS=%%i
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if "%CF_WORK%"=="" (if exist D:\ (set CF_WORK=D:\cf-work) else (set CF_WORK=%TEMP%\cf-work))
set SRC=%~1
set B=%CF_WORK%\win-%~2
if not exist %B% (cmake -S %SRC% -B %B% -G Ninja -DCMAKE_BUILD_TYPE=Release > %B%-cfg.log 2>&1 || (type %B%-cfg.log & exit /b 1))
cmake -S %SRC% -B %B% > %B%-recfg.log 2>&1 || (type %B%-recfg.log & exit /b 1)
cmake --build %B% 2>&1
if errorlevel 1 exit /b 1
ctest --test-dir %B% -j8 -R "%~3" --output-on-failure

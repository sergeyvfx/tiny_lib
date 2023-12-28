@echo OFF

REM Copyright (c) 2022 tiny lib authors
REM
REM SPDX-License-Identifier: MIT

REM A shortcut for building/testing/installing pipeline.
REM
REM Allows to type `make` with the same arguments on both Windows and Unix-like
REM systems.
REM This script takes care of Unix-like systems.
REM
REM Targets:
REM
REM   release, test, install - Build, test, and install the project in the
REM                            relase build configuration.
REM
REM   debug, debug-test, debug-install - Build, test, and install the project in
REM                                      the relase build configuration.
REM
REM Configuration variables:
REM
REM   DEVELOPER=1 - Enable all configurations used during development.
REM                 This includes address sanitizer, extra strict compiler
REM                 flags.
REM
REM                 NOTE: Only value of 1 is recognized as the flag enabled.
REM
REM   INSTALL_PREFIX=<prefix> - Use given path as a prefix for installation.
REM
REM   BUILD_DIR=<build directory> - Override automatically chosen build directory.


REM We don't want our intermediate variables to pollute context.
SETLOCAL

REM ============================================================================
REM Initial variables and configuration setup.
REM ============================================================================

set PROJECT_DIR=%~dp0

if NOT DEFINED INSTALL_PREFIX set INSTALL_PREFIX=
if NOT DEFINED DEVELOPER set DEVELOPER=0
if NOT DEFINED BUILD_DIR set BUILD_DIR=%PROJECT_DIR%\build

set CMAKE_ARGS=

set TARGET_RELEASE=0
set TARGET_TEST=0
set TARGET_INSTALL=0

set TARGET_DEBUG=0
set TARGET_DEBUG_TEST=0
set TARGET_DEBUG_INSTALL=0

set MSBUILD_VERBOSITY="minimal"
set SOLUTION_NAME="tiny_lib.sln"

REM ============================================================================
REM Parse arguments.
REM ============================================================================
:ARGV_LOOP
if NOT "%1" == "" (
  if "%1" == "debug" (
    set TARGET_DEBUG=1
  )
  if "%1" == "debug-test" (
    set TARGET_DEBUG=1
    set TARGET_DEBUG_TEST=1
  )
  if "%1" == "debug-install" (
    set TARGET_DEBUG=1
    set TARGET_DEBUG_INSTALL=1
  )

  if "%1" == "release" (
    set TARGET_RELEASE=1
  )
  if "%1" == "test" (
    set TARGET_RELEASE=1
    set TARGET_TEST=1
  )
  if "%1" == "install" (
    set TARGET_RELEASE=1
    set TARGET_INSTALL=1
  )

  if "%1" == "DEVELOPER" (
    if "%2" == "1" (
      set DEVELOPER=1
    )
    shift /1
  )

  if "%1" == "INSTALL_PREFIX" (
    set INSTALL_PREFIX=%2
    shift /1
  )

  if "%1" == "BUILD_DIR" (
    set BUILD_DIR=%2
    shift /1
  )

  shift /1
  goto ARGV_LOOP
)

REM Default to release target.
REM Similar to `all: release` in the makefile.
if %TARGET_DEBUG% EQU 0 if %TARGET_RELEASE% EQU 0 (
  set TARGET_RELEASE=1
)

if %DEVELOPER% EQU 1 (
  REM NOTE: Address sanitizer is not yet supported on Windows.
  REM set CMAKE_ARGS=%CMAKE_ARGS% -D WITH_DEVELOPER_SANITIZER=ON
  set CMAKE_ARGS=%CMAKE_ARGS% -D WITH_DEVELOPER_STRICT=ON
)

IF NOT "%INSTALL_PREFIX%" == "" (
  set CMAKE_ARGS=%CMAKE_ARGS% -D CMAKE_INSTALL_PREFIX=%INSTALL_PREFIX%
)

REM ============================================================================
REM Directories with spoaces support.
REM ============================================================================

REM There are some quircks in different aspects of this script, which makes it
REM more tricky to deal with spaces than simply using double quotes.
REM
REM - Using quotes on %~dp0 does not always work. This is because the direcotry
REM   contains backward slash at the end, which in some usages escapes the
REM   closing quote.
REM
REM - `IF EXISTS %FOO%\nul` to check whether directory exists does not work when
REM   put in quotes. `IF EXISTS "%FOO%"` does work, but is not guaranteed to be
REM   a directory.
REM
REM - CMake itself is doing verid decisions when paths with spaces and Windows
REM   like directory separators are passed. Replacing Windows separator with
REM   Unix one works around the issue.

REM If the directory has forward slashes already, the logic bewlow will break.
set PROJECT_DIR_NOSLASHES=%PROJECT_DIR:/=%
if not "%PROJECT_DIR%"=="%PROJECT_DIR_NOSLASHES%" (
  echo Forward slashes are detected in the project path.
  echo This is not currently supported, exiting.
  exit /b 1
)

REM ============================================================================
REM Current architecture detection.
REM ============================================================================
REM
REM We will use current machine architecture as the one to make the build.
REM
REM TODO(sergey): Consider adding foreign configuration support, so we can, for
REM example, cross-compile for different target CPUs.
REM
REM NOTE: Annoyingly, we need to pass diferent values to CMake and to
REM vcvarsall.bat. There seems to be no easy way around it, so we just maintain
REM two sets of stringss.

set CMAKE_GENERATOR_ARCH=
set VCVARSALL_ARCH=
if "%PROCESSOR_ARCHITECTURE%" == "AMD64" (
  set CMAKE_GENERATOR_ARCH=Win64
  set VCVARSALL_ARCH=x64
) else if "%PROCESSOR_ARCHITEW6432%" == "AMD64" (
  set CMAKE_GENERATOR_ARCH=Win64
  set VCVARSALL_ARCH=x64
) else (
  set CMAKE_GENERATOR_ARCH=
  set VCVARSALL_ARCH=x86
)

REM ============================================================================
REM System configuration detection.
REM ============================================================================
REM
REM Detect where the 32 bit Program Files are.
REM
REM It will be %ProgramFiles% on 64 bit Windows, but %ProgramFiles%
REM on 32 bit Windows.
REM
REM This is needed because Visual Studio is installed in the 32 bit Program
REM files.
set PROGRAM_FILES_X86=%ProgramFiles(x86)%
if not exist "%PROGRAM_FILES_X86%" set PROGRAM_FILES_X86=%ProgramFiles%

REM ============================================================================
REM Visual studio detection.
REM ============================================================================
REM
REM We use latest available Visual Studio, since the project is supposed to be
REM working on the wide variety of compilers equally good. Might look into
REM detecting some soert of "default" Visual Studio, similar to the default GCC
REM and Clang on Linux.

REM Detect executable which allows to query various Visual Studio installations.
set VSWHERE=%PROGRAM_FILES_X86%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%VSWHERE%" (
  echo Visual Studio installation is not detected
  goto FINITO
)

REM Parse output, so we can know where the installation directory is, and where
REM to look for vcvarsall.bat.
REM We also detecting all the parts of the Visual studio version which are
REM needed to construct CMake generator name.
REM
REM Examples:
REM   VS_INSTALL_DIR="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community"
REM   VS_DISPLAY_NAME="Visual Studio Community 2017"
REM   VS_VERSION_YEAR="2017"
REM   VS_VERSION_SEMANTIC="15.9.2+28307.108"
set VS_INSTALL_DIR=
set VS_DISPLAY_NAME=
set VS_VERSION_YEAR=
set VS_VERSION_SEMANTIC=
for /f "usebackq tokens=1* delims=: " %%i in (`"%VSWHERE%" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64`) do (
  if /i "%%i"=="installationPath" set VS_INSTALL_DIR=%%j
  if /i "%%i"=="displayName" set VS_DISPLAY_NAME=%%j
  if /i "%%i"=="catalog_productLineVersion" set VS_VERSION_YEAR=%%j
  if /i "%%i"=="catalog_productSemanticVersion" set VS_VERSION_SEMANTIC=%%j
)
REM CMake expects both versaion and the year, for example the proper geenrator
REM name is "Visual Studio 15 2017 Win64". Here we extract the major version
REM from semantic version.
REM
REM TODO(sergey): Split y the dot, instead of doing substring selection.
set VS_VERSION_SHORT=%VS_VERSION_SEMANTIC:~0,2%

REM Set up the build environment to the detected toolchain and processor.
echo Detected %VS_DISPLAY_NAME%
set VCVARSALL=%VS_INSTALL_DIR%\VC\Auxiliary\Build\vcvarsall.bat
call "%VCVARSALL%" %VCVARSALL_ARCH%

REM ============================================================================
REM Perform some sanity checks, to see all commands and such are found.
REM ============================================================================

where /Q msbuild
if %ERRORLEVEL% NEQ 0 (
  echo Error: "MSBuild" command not found.
  goto FINITO
)
where /Q cmake
if %ERRORLEVEL% NEQ 0 (
  echo Error: "CMake" command not found.
  goto FINITO
)

REM ============================================================================
REM Entry point.
REM ============================================================================
REM
REM Skip all functions and jump into targets handling.

goto targets

REM ============================================================================
REM Configure the project.
REM ============================================================================

:configure

set PROJECT_DIR_UNIX=%PROJECT_DIR:\=/%
set BUILD_DIR_UNIX=%BUILD_DIR:\=/%
set CONFIGURE_CMAKE_ARGS=%CMAKE_ARGS%

set CMAKE_GENERATOR_NAME=Visual Studio %VS_VERSION_SHORT% %VS_VERSION_YEAR%
if %VS_VERSION_YEAR% LSS 2019 (
  set CMAKE_GENERATOR_NAME=%CMAKE_GENERATOR_NAME% %CMAKE_GENERATOR_ARCH%
) else (
  REM TODO(sergey): Check whether CMake is new enough to support this version of MSVC.
  set CONFIGURE_CMAKE_ARGS=%CONFIGURE_CMAKE_ARGS% -A %VCVARSALL_ARCH%
)

set CONFIGURE_CMAKE_ARGS=%CONFIGURE_CMAKE_ARGS% -G "%CMAKE_GENERATOR_NAME%"

REM Configure on every run of `make`. While this seems to be an unnecessary step
REM which only slows overall process down there are some good reasons for it:
REM
REM - There is some issue which leads to MSBuild failures when new files are
REM   addedl even though CMake is re-run linking fails due to missing symbols
REM   from the new files.
REM
REM   This could be a bug in CMake or MSBuild, but need to deal with it so that
REM   there are no random build failures.
REM
REM - This allows to tweak CMake options even after the project was generated.
REM
REM - This is also how Makefile behaves.
REM
REM If faster iteration is needed it is always possible to cd into the build
REM directory and type commands in there.

echo.
echo **********************************************************************
echo ** Configuring the project...
echo **********************************************************************
echo.

cmake ^
  -H"%PROJECT_DIR_UNIX%" ^
  -B"%BUILD_DIR_UNIX%" ^
  %CONFIGURE_CMAKE_ARGS%

if %ERRORLEVEL% NEQ 0 (
  echo Error: Configuration Failed.
  goto FINITO
)

goto:eof

REM ============================================================================
REM Perform build of a given configuration,
REM ============================================================================

:build

echo.
echo **********************************************************************
echo ** Building the project...
echo **********************************************************************
echo.
msbuild ^
  "%BUILD_DIR%\\%SOLUTION_NAME%" ^
  /target:build ^
  /property:Configuration=%BUILD_TYPE% ^
  /maxcpucount ^
  /verbosity:%MSBUILD_VERBOSITY%
if %ERRORLEVEL% NEQ 0 (
  echo Error: Build Failed.
  goto FINITO
)

echo.
echo Build succeeded.

goto:eof

REM ============================================================================
REM Regression tests.
REM ============================================================================

:test

echo.
echo **********************************************************************
echo ** Running regression tests...
echo **********************************************************************
echo
msbuild ^
  "%BUILD_DIR%\RUN_TESTS.vcxproj" ^
  /property:Configuration=%BUILD_TYPE% ^
  /maxcpucount ^
  /verbosity:%MSBUILD_VERBOSITY%
if %ERRORLEVEL% NEQ 0 (
  echo Error: One or multiple tests mailed.
  goto FINITO
)

echo.
echo Tests passed.

goto:eof

REM ============================================================================
REM Installation
REM ============================================================================

:install

echo.
echo **********************************************************************
echo ** Installing the project...
echo **********************************************************************
echo.
msbuild ^
  "%BUILD_DIR%\INSTALL.vcxproj" ^
  /property:Configuration=%BUILD_TYPE% ^
  /maxcpucount ^
  /verbosity:%MSBUILD_VERBOSITY%
if %ERRORLEVEL% NEQ 0 (
  echo Error: Installation failed.
  goto FINITO
)

echo.
echo Installation completed.

goto:eof

REM ============================================================================
REM Targets.
REM ============================================================================

:targets

if %TARGET_RELEASE% NEQ 0 (
  set BUILD_TYPE="Release"

  call:configure
  call:build

  if %TARGET_TEST% NEQ 0 call:test
  if %TARGET_INSTALL% NEQ 0 call:install
)

if %TARGET_DEBUG% NEQ 0 (
  set BUILD_TYPE="Debug"

  call:configure
  call:build

  if %TARGET_DEBUG_TEST% NEQ 0 call:test
  if %TARGET_DEBUG_INSTALL% NEQ 0 call:install
)

:FINITO

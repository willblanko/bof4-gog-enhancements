@echo off
cd /d "%~dp0"
rem Build d3d9.dll (the Direct3D 9 proxy). Run this from the d3d9 folder.
rem Needs Visual Studio 2022 Build Tools with the x86 C++ compiler.
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
if not exist "%VCVARS%" (
  echo vcvars32.bat not found. Edit VCVARS at the top of this file.
  exit /b 1
)
call "%VCVARS%" >nul
cl /nologo /O1 /GS- /W3 /c d3d9_proxy.c
if errorlevel 1 exit /b 1
link /nologo /DLL /OUT:d3d9.dll /DEF:d3d9.def /ENTRY:DllMain@12 /NODEFAULTLIB /MAP d3d9_proxy.obj kernel32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b 1
echo Built d3d9.dll
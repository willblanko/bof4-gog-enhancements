@echo off
cd /d "%~dp0"
rem Build ddraw.dll (the pass-through DirectDraw wrapper). Run this from the ddraw folder.
rem Needs Visual Studio 2022 Build Tools with the x86 C++ compiler.
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
if not exist "%VCVARS%" (
  echo vcvars32.bat not found. Edit VCVARS at the top of this file.
  exit /b 1
)
call "%VCVARS%" >nul
cl /nologo /O1 /GS- /W3 /c ddraw_pass.c
if errorlevel 1 exit /b 1
link /nologo /DLL /OUT:ddraw.dll /DEF:ddraw_pass.def /ENTRY:proxy_entry@12 /NODEFAULTLIB ddraw_pass.obj kernel32.lib
if errorlevel 1 exit /b 1
echo Built ddraw.dll
@echo off
setlocal
set "CXX="
set "RC="
if defined MINGW_BIN if exist "%MINGW_BIN%\g++.exe" (
  set "CXX=%MINGW_BIN%\g++.exe"
  set "RC=%MINGW_BIN%\windres.exe"
)
if not defined CXX if exist "D:\tools\mingw64\bin\g++.exe" (
  set "CXX=D:\tools\mingw64\bin\g++.exe"
  set "RC=D:\tools\mingw64\bin\windres.exe"
)
if not defined CXX (
  set "CXX=g++"
  set "RC=windres"
)
"%CXX%" --version >nul 2>nul
if errorlevel 1 (
  echo ERROR: no se encontro g++. Define MINGW_BIN o agrega MinGW al PATH.
  exit /b 1
)
pushd "%~dp0"
"%RC%" organizador.rc organizador_res.o
if %errorlevel% neq 0 ( echo ERROR recurso & popd & exit /b 1 )
"%CXX%" -O2 -std=c++17 -municode -mwindows -static -s -flto ^
  -Wall -Wextra -DUNICODE -D_UNICODE ^
  organizador.cpp organizador_res.o -o organizador.exe ^
  -lshell32 -lgdi32 -luser32
if %errorlevel% neq 0 (
  echo ERROR compilacion
  popd
  exit /b 1
)
del organizador_res.o
popd
echo OK: organizador.exe
for %%f in ("%~dp0organizador.exe") do echo Tamano: %%~zf bytes
echo Ejecuta: organizador.exe --selftest

@echo off
setlocal
cd /d "%~dp0"
git submodule update --init --recursive
if errorlevel 1 exit /b %errorlevel%
tools\premake5 %* vs2022
exit /b %errorlevel%

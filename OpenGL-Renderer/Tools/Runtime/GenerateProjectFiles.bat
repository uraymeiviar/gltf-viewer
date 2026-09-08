@echo off

set source_directory=%~dp0
set build_directory=%~dp0\build
set platform=x64

mkdir "%build_directory%"
cd "%build_directory%"
cmake -A "%platform%" "%source_directory%"
pause
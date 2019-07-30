@echo off
SET script_path=%~dp0
pushd %script_path%
..\tools\bin\windows\genie.exe vs2019
popd

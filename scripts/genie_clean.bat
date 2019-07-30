@echo off
SET script_path=%~dp0
pushd %script_path%
..\tools\bin\windows\genie.exe clean
popd

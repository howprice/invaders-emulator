@echo off

if "%VisualStudioVersion%"=="" (
	echo Please run in Visual Studio Native Tools Command Prompt
	EXIT /B 1
)

if %VisualStudioVersion% == 15.0 (
	set CMAKE_GENERATOR="Visual Studio 15 2017"
) else if %VisualStudioVersion% == 16.0 (
	set CMAKE_GENERATOR="Visual Studio 16 2019"
) else if %VisualStudioVersion% == 17.0 (
	set CMAKE_GENERATOR="Visual Studio 17 2022"
) else (
	echo Unsupported Visual Studio version
	EXIT /B 1
)

mkdir build
pushd build
cmake -G %CMAKE_GENERATOR% -A x64 ..
popd

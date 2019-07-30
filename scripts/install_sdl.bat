@echo off

SET script_path=%~dp0
pushd %script_path%

REM cd up into repo root
cd ..

SET SDL2_VERSION=2.0.9
SET SDL2_MIXER_VERSION=2.0.4

IF NOT EXIST tmp mkdir tmp
pushd tmp


REM SDL2
SET SDL2_ZIP=SDL2-devel-%SDL2_VERSION%-VC.zip
IF NOT EXIST %SDL2_ZIP% (
	ECHO Downloading %SDL2_ZIP%
	curl -O https://www.libsdl.org/release/%SDL2_ZIP%
)
tar xfz %SDL2_ZIP%
IF %ERRORLEVEL% NEQ 0 unzip %SDL2_ZIP%
IF %ERRORLEVEL% NEQ 0 EXIT /B 0
robocopy /MOVE /S /E SDL2-%SDL2_VERSION% ..\libs\SDL2


REM SDL2_mixer
SET SDL2_MIXER_ZIP=SDL2_mixer-devel-%SDL2_MIXER_VERSION%-VC.zip
IF NOT EXIST %SDL2_MIXER_ZIP% (
	ECHO Downloading %SDL2_MIXER_ZIP%
	curl -O https://www.libsdl.org/projects/SDL_mixer/release/%SDL2_MIXER_ZIP%
)
tar xfz %SDL2_MIXER_ZIP%
IF %ERRORLEVEL% NEQ 0 unzip %SDL2_MIXER_ZIP%
IF %ERRORLEVEL% NEQ 0 EXIT /B 0
robocopy /MOVE /S /E SDL2_mixer-%SDL2_MIXER_VERSION% ..\libs\SDL2_mixer


REM back to repo root
popd

REM back to original directory
popd

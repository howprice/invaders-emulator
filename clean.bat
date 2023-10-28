@echo off
if exist build (
    rmdir build /s /q
    echo Deleted build folder
) else (
    echo build folder does not exist
)

if exist tmp (
    rmdir tmp /s /q
    echo Deleted tmp folder
) else (
    echo tmp folder does not exist
)

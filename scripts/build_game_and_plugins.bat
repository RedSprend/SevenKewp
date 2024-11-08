@echo off
cls

:: run from the location of this script
cd %~dp0
cd ..

mkdir build
cd build
cmake -A Win32 -DBUILD_SERVER=ON -DBUILD_PLUGINS=ON -DUPDATE_PLUGINS=ON ..
cmake --build . --config Release

echo.
pause
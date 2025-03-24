@echo off
gcc ./src/main.c -I./include -L./build -lSDL3 -lSDL3_image -std=c99 -Wall -Wextra -Wno-unused-parameter -O3 -o build/main.exe
if %errorlevel% equ 0 (
   echo *** Build successful, running main.exe ...
   .\build\main.exe
) else (
   echo Build script failed.
)

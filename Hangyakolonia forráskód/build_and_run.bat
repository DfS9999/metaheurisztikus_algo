@echo off
gcc ./src/main.c ./src/antcolony.c -I./include -L./build -lSDL3 -lSDL3_image -lSDL3_ttf -std=c99 -Wall -Wextra -Werror -pedantic -Wno-unused-parameter -O3 -o build/Hangyakolonia.exe
if %errorlevel% equ 0 (
   echo *** Build successful, running Hangyakolonia.exe ...
   .\build\Hangyakolonia.exe
) else (
   echo Build script failed.
)

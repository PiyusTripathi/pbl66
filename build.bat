@echo off
echo [*] Compiling Huffman CLI (huffman.exe)...
gcc -O3 -Wall -Wextra -D__USE_MINGW_ANSI_STDIO=1 -Iinclude src/main.c src/huffman.c src/bitio.c src/crc32.c -o huffman.exe
if %ERRORLEVEL% neq 0 (
    echo [X] CLI Build Failed!
    exit /b %ERRORLEVEL%
)

echo [*] Compiling Huffman GUI Desktop App (huffman_gui.exe)...
gcc -O3 -Wall -Wextra -D__USE_MINGW_ANSI_STDIO=1 -Iinclude src/gui.c src/huffman.c src/bitio.c src/crc32.c -o huffman_gui.exe -mwindows -lcomctl32 -lcomdlg32 -lshell32
if %ERRORLEVEL% neq 0 (
    echo [X] GUI Build Failed!
    exit /b %ERRORLEVEL%
)

echo.
echo [+] BUILD SUCCESSFUL!
echo     - CLI Binary: huffman.exe
echo     - GUI Application: huffman_gui.exe (Double-click to run!)


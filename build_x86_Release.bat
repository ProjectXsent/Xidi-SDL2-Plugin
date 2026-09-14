cmake -B build_x86 -A Win32 -DSDL2_DIR=".\SDL2\cmake"
cmake --build build_x86 --config Release
pause
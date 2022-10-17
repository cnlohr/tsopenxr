rem Just use gcc or download the 4MB tinycc + Windows headers to somewhere in your system path.
tcc demo.c -o demo.exe -I. -luser32 -lopengl32 -lgdi32 openxr_loader.dll -Wall -g
tcc minimal.c -o minimal.exe -I. -luser32 -lopengl32 -lgdi32 openxr_loader.dll -Wall -Os

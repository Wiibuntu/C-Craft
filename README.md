C-Craft (voxel)

A lightweight Minecraft-style voxel sandbox written in C++ (SDL2 + OpenGL + GLEW).

---------------------------------------------------------------------------------------

Runtime assets (required)

Place these files in the same directory as the executable:

texture.png (block atlas)

hand.png (hand/held item)

BG.png (menu background)

If any are missing, game will fail to load.

---------------------------------------------------------------------------------------

Build requirements

A C++ compiler with C++11 support
OpenGL headers/libraries
SDL2 development files
GLEW development files


Debian/Ubuntu:

sudo apt update
sudo apt install -y build-essential make \
  libsdl2-dev libglew-dev libgl1-mesa-dev


Arch/Manjaro (native Linux build):

sudo pacman -S --needed base-devel make sdl2 glew mesa

make linux

---------------------------------------------------------------------------------------

Build for Windows

Not Finished, slightly complex. 

Install mingw-w64-gcc and also download the Devel files for SDL2.
Make sure you MERGE the SDL2 devel folders with MingW folders.
Download the source code for GLEW and GLUT and copy the contents from the Include/GL folder to the GL folder included with MingW.

run make windows

Now you have a EXE that doesnt work.. you need to locate glew32.dll, libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll, SDL2.dll.
Once you have those files located in there respective locations you need to copy them to a folder and copy the voxel.exe file to that folder along with the needed .png files for the engine to run.

NOW FINALLY it should run on Windows :)

---------------------------------------------------------------------------------------

Black screen / GL errors on old GPUs

This project uses OpenGL 3.3 core shaders (#version 330 core). Make sure your drivers/GPU support it.




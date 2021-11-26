# Ludum Dare 48 Comp Game
This is a game I made as an exploration of a scifi theme, as well as trying out a way to construct entities in code that allowed me to create a variety of things and interactions. It’s a pretty short and simple game that’s more about establishing a the setting and theme rather than creative gameplay.

[Download here](https://github.com/rytc/ld48/releases/tag/1.0)

## Dependencies
- Raylib
- GLFW
 
## How to build
- Linux: `./build.sh` (Uses clang to compile)
- Windows: `./build.bat` (Uses msvc to compile) 
Assets can be downloaded from the release .zip

## Windows Build
- Install GLFW into `thirdparty/` if not in the path, ie `thirdparty/glfwdll.lib` and `thirdparty/GLFW/glfw3.h`
- Need to manually run `vcvarsall.bat` to setup the cmd prompt for VC.
- Build/tested with MSVC 2019

![Image of Depar](https://github.com/rytc/ld48/blob/main/screenshots/Screenshot%20from%202021-04-25%2019-29-58.png)

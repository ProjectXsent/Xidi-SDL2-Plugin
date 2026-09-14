# Xidi SDL2 Plugin
This is a plugin for [Xidi](https://github.com/samuelgr/Xidi) (versions 5.0.0 and above) that allows the usage of any controller that is supported by the [SDL2 Gamepad API](https://wiki.libsdl.org/SDL2/CategoryGameController). This includes most popular controllers, including:
* Sony DualShock 3
* Sony DualShock 4
* Sony DualSense
* Microsoft Xbox 360 Controller
* Microsoft Xbox One Controller
* Nintendo Switch Joy-Con
* Nintendo Switch Pro Controller
* Steam Controller
* Google Stadia Controller
* Amazon Luna Controller
* Various 8BitDo Gamepads
* Various Logitech Gamepads
* Various Razer Gamepads

A full list of supported controllers can be found in the [SDL2 source code](https://github.com/libsdl-org/SDL/blob/SDL2/src/joystick/SDL_gamecontrollerdb.h).

## Building from source
**Requirements:**
- [CMake](https://cmake.org)
- [Microsoft Visual Studio](https://visualstudio.microsoft.com) (2022 and above or use Visual Studio Build Tools)
- SDL2-devel-2.x.xx-VC (current repo is using v2.32.8)
    - In case you want to update, download the latest `SDL2-devel-2.x.xx-VC.zip`, extract the folder to Xidi SDL2 Plugin repository and rename it to `SDL2`. This is for the `-DSDL2_DIR=".\SDL2\cmake"` in the .bat file.

**On Windows (primary path):**
- Run `build_x86_Release.bat` or `build_x64_Release.bat`.

## Usage
To use the plugin, [set up Xidi as per normal](https://github.com/samuelgr/Xidi/wiki/Getting-Started), download the [latest release of the SDL2 Plugin](https://github.com/ProjectXsent/Xidi-SDL3-Plugin/releases/latest), and place either the `SDL.XidiPlugin.32.dll` file (for a 32-bit application), or the `SDL.XidiPlugin.64.dll` (for a 64-bit application) and the appropriate `SDL2.dll` into the same directory as the application executable.

Xidi must then be configured to use the plugin instead of its own implementation, to do this, create a [configuration file](https://github.com/samuelgr/Xidi/wiki/Configuration) in the same directory as the application named `Xidi.ini` and add the following text to the top of the file:
````
Plugin = SDL
ControllerBackend = SDL2
````

Note that there is no INI section for this, it must be added before any INI section headers (text in \[angled brackets\] are section headers). You can leave the rest of the configuration file blank, or configure it as described in the [Xidi wiki](https://github.com/samuelgr/Xidi/wiki/Configuration).

## SDL3
To use SDL3, see [SDL2-Compat](https://github.com/libsdl-org/sdl2-compat) or use RibShark's [Xidi SDL3 Plugin](https://github.com/RibShark/Xidi-SDL3-Plugin).

## Credits
- libsdl-org (SDL2/SDL2-Compat)
- samuelgr (Xidi)
- RibShark (Xidi-SDL3-Plugin)

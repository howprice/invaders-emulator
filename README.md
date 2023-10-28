# invaders-emulator [![Build Status](https://travis-ci.org/howprice/invaders-emulator.svg?branch=master)](https://travis-ci.org/howprice/invaders-emulator)

Cross-platform Space Invaders emulator. My first emulator! Inspired by the excellent tutorial at [Emulator 101](http://emulator101.com).

Repo also includes simple command line disassembler project.

<br><img src="docs/screenshot.png">

## Dependencies

This project depends on [SDL2](https://www.libsdl.org) (at least version 2.0.6) and [SDL_mixer 2.0](https://www.libsdl.org/projects/SDL_mixer/)

Unzipped invaders ROMs should be placed the 'data' folder. The emulator will not run if the ROMs are not present.

Unzip samples 0.wav .. 8.wav from http://samples.mameworld.info/ into the 'data' folder. The emulator can run without samples present.

## Building and Running

This project uses CMake.

### Windows

Either:
1. Run [update_dependencies.bat](update_dependencies.bat) (assumes that [tar and curl are available](https://devblogs.microsoft.com/commandline/tar-and-curl-come-to-windows/))
2. Download the [SDL2 Development libraries for Visual C++](https://www.libsdl.org/download-2.0.php) and the [SDL_mixer 2.0](https://www.libsdl.org/projects/SDL_mixer/) development library. Unzip to the [libs](libs) folder and remove the version numbers from the folder names.

To generate the Visual Studio Solution and project files, run [gensln.bat](gensln.bat) from *Visual Studio Native Tools Command Prompt for VS20xx* (2017, 2019, 2022 supported). The generated files are located in the 'build' folder.

Build and run from within Visual Studio, or [build from the command line](https://docs.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=vs-2017) with [MSBuild](https://docs.microsoft.com/en-us/visualstudio/msbuild/msbuild?view=vs-2017).

### Linux

[Download dependencies](https://www.libsdl.org/download-2.0.php) and install from source, or install using a package manager. For example, for Ubuntu and Raspberry Pi:

	$ sudo apt-get install libsdl2-dev
	$ sudo apt-get install libsdl2-mixer-dev

Clone this repo, build the makefiles and build:

	$ git clone https://github.com/howprice/invaders-emulator
	$ cd invaders-emulator
	$ ./genie_gmake.sh  (or ./genie_raspberrypi.sh)
	$ cd build
	$ make

n.b. You may need to 'chmod +x' genie executable and or .sh shell script files.

`make` with no config specified defaults to the debug config for native architecture (usually 64-bit). Can call `make config=<xxx>` where `xxx` can be `debug`, `dev`, `release`, `debug32`, `dev32`, `release32`, `debug64`, `dev64` or `release64`. Or type `make help` for more info. Make sure you're in the `build` folder.

To run the emulator

	$ cd data
	$ ../bin/debug/emulator   (or dev or release)

## Mac OS X

[Download dependencies](https://www.libsdl.org/download-2.0.php) and install from source, or install using [Brew](http://brew.sh):

	$ brew install sdl2
	$ brew install sdl2_mixer
	
Clone this repo, build the makefiles for either gcc (aliased to clang on Mac) or Xcode:

	$ ./genie_gmake.sh   // then build and run as Linux above
	
or

	$ ./genie_xcode10.sh   // then build and run in XCode

## Cleaning the build files

	$ ./genie_clean.sh

## Controls

### Game Controls
- Press 5 to insert coin
- Press 1 to start one player game
- Press 2 to start two player game
- Player 1 controls: Controller or keyboard cursors left/right and space
- Player 2 controls: Share Player 1 controls, or second controller and O, P, and Q keys
- Press Esc to quit
- Press T to tilt

### Emulator Controls
- Tab - Toggle user interface.
- F3 - Reset machine
- F5 - Break / resume execution
- Shift+F7 - Save state (to memory)
- F7 - Restore state (from memory)
- F8 - Step Frame
- F10 - Step Over
- F11 - Step Into
- Shift+F11 - Step Out

- Right click on Disassembly Window for context menu

## Notes

Cycle-exact emulation is not required.

## Links and thanks

- [Emulator 101](http://emulator101.com)
- The excellent [ImGui](https://github.com/ocornut/imgui)
- http://www.emutalk.net/threads/38177-Space-Invaders
- https://en.wikipedia.org/wiki/Intel_8080
- Intel 8080 Assembly Language Programming Manual a.k.a. "The Data Book"
- [Intel 8080/8085 Assembly Language Programming](https://www.tramm.li/i8080/Intel%208080-8085%20Assembly%20Language%20Programming%201977%20Intel.pdf)
- [Intel 8080 Microcomputer System User's Manual](http://www.nj7p.info/Manuals/PDFs/Intel/9800153B.pdf)
- [Computer Archeology - Space Invaders](http://computerarcheology.com/Arcade/SpaceInvaders/)
  - [Disassembled Code as Z80 opcodes](http://computerarcheology.com/Arcade/SpaceInvaders/Code.html)
  - [RAM Usage](http://computerarcheology.com/Arcade/SpaceInvaders/RAMUse.html)
  - [Hardware](http://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html)

## License

See the [LICENSE](LICENSE) file for details.

# invaders-emulator

---

## About

Space Invaders emulator. My first emulator. Inspired by the excellent tutorial at [Emulator 101](http://emulator101.com)

## Dependencies

This project depends on [SDL2](https://www.libsdl.org). The [SDL2 Development libraries for Visual C++](https://www.libsdl.org/download-2.0.php) are checked in to 3rdparty for convenience. 

Unzipped invaders ROMs should be placed the 'data' folder.

## Building

This project uses [GENie](https://github.com/bkaradzic/genie) to generate the build files for the target platform.

### Windows

Run genie_vs2017.bat or genie_vs2019.bat to build the Visual Studio Solution and projects into the 'build' folder

### Other Platforms

Not yet tested. May require minor fix-up.

## Controls

- Press Esc to quit
- Press 5 to insert coin
- Press 1 to start one player game
- Press 2 to start two player game
- Player 1 controls: cursors left/right and space
- Player 2 controls: O/P to move left and right and Q to fire

- Press Tab to show/hide dev UI.
- F5 - Break / resume execution
- F8 - Step Frame
- F10 - Step Over
- F11 - Step Into

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

## TODO

- Move debugHook into debugger class/file

- Get rid of Machine.memorySizeBytes
- Update memory window to select Chunk to view, which is guaranteed to be a single contiguous block.
- Debugger
  - Stack Window
    - Store "stack base" when SP set with LXI SP,<address> instruction
  - "Backtrace"? (Previously executed line(s))
  - Editable registers in CPU Window
  - Trap?
  - Disassembly Window:
    - Use ImGuiListClipper (see ImGui hex editor code)
    - Add Autoscroll option
  - Load Computer Archaeology annotated disassembly and memory files to auto annotate! 
- Sounds
  - http://www.brentradio.com/SpaceInvaders.htm
- Save/restore state
- Load different ROMs
  - List in mame\src\mame\drivers\mw8080bw.cpp
- Cheats
  - How does MAME handle cheats?
- Accurate display buffer generation by copying pixel by pixel as the CPU / raster progresses. 
- What is the correct point in the frame to generate the interrupts? See machine update function
- Use point sampler in shader
- Headless mode (command line arg)
- Test on Linux/Mac
- What is the purpose of the RAM mirror? (It *is* used)
- Apply coloured overlay (make machine display buffer RGB8 and apply in video RAM copy)

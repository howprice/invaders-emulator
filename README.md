# invaders-emulator

---

## About

Space Invaders emulator. My first emulator. Inspired by the excellent tutorial at [Emulator 101](http://emulator101.com)

## Links

- [Emulator 101](http://emulator101.com)
- http://www.emutalk.net/threads/38177-Space-Invaders
- https://en.wikipedia.org/wiki/Intel_8080
- Intel 8080 Assembly Language Programming Manual a.k.a. "The Data Book"
- [Intel 8080 Microcomputer System User's Manual](http://www.nj7p.info/Manuals/PDFs/Intel/9800153B.pdf)

## TODO

- Rotate screen 90 degrees anti-clockwise
- Map host input (SDL) to port data
- Add timings to 8080 instructions 
- Return instruction cycle count from instruction execution functions
  - n.b. Can be different depending on what the instruction did e.g. jump or not jump
  - Best reference for this? Data Book?
- Accurate display buffer generation by copying pixel by pixel as the CPU / raster progresses. 
- What is the correct point in the frame to generate the interrupts? See machine update function
- Use point sampler in shader
- Headless mode (command line arg)
- Remove pMemory from State8080; it belongs in the Machine
- Debugger
  - ImGui and/or command line
  - Show stack
- Sounds
  - http://www.brentradio.com/SpaceInvaders.htm
- Test on Linux/Mac
- Test with other 8080bw compatible ROMs See list in C:\GitHub\mamedev\mame\src\mame\drivers\mw8080bw.cpp
  - May need to implement more instructions and Auxiliary Carry Flag
- What is the purpose of the RAM mirror? (It *is* used)
- Apply coloured overlay (make machine display buffer RGB8 and apply in video RAM copy)

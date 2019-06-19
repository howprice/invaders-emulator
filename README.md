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

- Render video memory to SDL window
  - Use point sampler
- Render a textured quad, and update it per frame with glTexSubImage2D
  https://stackoverflow.com/questions/38824212/im-trying-to-update-a-texture-with-gltexsubimage2d
- Approximate display buffer by copying instantaneously from RAM at and of frame. 
- Add timings to 8080 instructions 
- Accurate display buffer generation by copying pixel by pixel as the CPU / raster progresses. 
- What is the correct point in the frame to generate the interrupts? See machine update function
- Headless mode (command line arg)
- Remove pMemory from State8080; it belongs in the Machine
- Map host input (SDL) to port data
- Return instruction cycle count from instruction execution functions
  - n.b. Can be different depending on what the instruction did e.g. jump or not jump
  - Best reference for this? Data Book?
- Debugger
  - ImGui and/or command line
  - Show stack
- Sounds
  - http://www.brentradio.com/SpaceInvaders.htm
- Test on Linux/Mac
- Test with other 8080bw compatible ROMs See list in C:\GitHub\mamedev\mame\src\mame\drivers\mw8080bw.cpp
  - May need to implement more instructions and Auxiliary Carry Flag
- What is the purpose of the RAM mirror? (It *is* used)

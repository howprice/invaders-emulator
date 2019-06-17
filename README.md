# invaders-emulator

---

## About

Space Invaders emulator. My first emulator. Inspired by the excellent tutorial at [Emulator 101](http://emulator101.com)

## Links

- [Emulator 101](http://emulator101.com)
- http://www.emutalk.net/threads/38177-Space-Invaders
- https://en.wikipedia.org/wiki/Intel_8080
- Intel 8080 Assembly Language Programming Manual a.k.a. "The Data Book"

## TODO

- Implement Shift Register
- Render video memory to SDL window
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

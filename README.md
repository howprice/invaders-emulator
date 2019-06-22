# invaders-emulator

---

## About

Space Invaders emulator. My first emulator. Inspired by the excellent tutorial at [Emulator 101](http://emulator101.com)

## Controls

Press 5 to insert coin

## Links

- [Emulator 101](http://emulator101.com)
- http://www.emutalk.net/threads/38177-Space-Invaders
- https://en.wikipedia.org/wiki/Intel_8080
- Intel 8080 Assembly Language Programming Manual a.k.a. "The Data Book"
- [Intel 8080/8085 Assembly Language Programming](https://www.tramm.li/i8080/Intel%208080-8085%20Assembly%20Language%20Programming%201977%20Intel.pdf)
- [Intel 8080 Microcomputer System User's Manual](http://www.nj7p.info/Manuals/PDFs/Intel/9800153B.pdf)
- [Half-carry flag](https://en.wikipedia.org/wiki/Half-carry_flag)
- [Computer Archeology - Space Invaders](http://computerarcheology.com/Arcade/SpaceInvaders/)
  - [Disassembled Code as Z80 opcodes](http://computerarcheology.com/Arcade/SpaceInvaders/Code.html)
  - [RAM Usage](http://computerarcheology.com/Arcade/SpaceInvaders/RAMUse.html)
  - [Hardware](http://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html)

## TODO

- Debugger
  - Step Into (F11)
  - Step Over (F10)
  - Set breakpoint
  - ImGui and/or command line
  - Show stack
  - Load Computer Archaeology annotated disassembly and memory files to auto annotate! 
  - Trap?

- Accurate display buffer generation by copying pixel by pixel as the CPU / raster progresses. 
- What is the correct point in the frame to generate the interrupts? See machine update function
- Use point sampler in shader
- Headless mode (command line arg)
- Remove pMemory from State8080; it belongs in the Machine

Support reset. i.e. Set PC to 0
- Sounds
  - http://www.brentradio.com/SpaceInvaders.htm
- Test on Linux/Mac
- Test with other 8080bw compatible ROMs See list in C:\GitHub\mamedev\mame\src\mame\drivers\mw8080bw.cpp
  - May need to implement more instructions and Auxiliary Carry Flag
- What is the purpose of the RAM mirror? (It *is* used)
- Apply coloured overlay (make machine display buffer RGB8 and apply in video RAM copy)

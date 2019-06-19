/*
http://www.emutalk.net/threads/38177-Space-Invaders?s=e58df01e41111c4efc6f3207b2890054&p=359411&viewfull=1#post359411
http://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html#DedicatedShiftHardware

Space Invaders, (C) Taito 1978, Midway 1979

CPU: Intel 8080 @ 2MHz (CPU similar to the (newer) Zilog Z80)

Interrupts: $cf (RST 8) at the start of vblank, $d7 (RST $10) at the end of vblank.

Video: 256(x)*224(y) @ 60Hz, vertical monitor. Colours are simulated with a
plastic transparent overlay and a background picture.
Video hardware is very simple: 7168 bytes 1bpp bitmap (32 bytes per scanline).

Sound: SN76477 and samples.

Memory map:
	ROM
	$0000-$07ff:	invaders.h
	$0800-$0fff:	invaders.g
	$1000-$17ff:	invaders.f
	$1800-$1fff:	invaders.e
	
	RAM
	$2000-$23ff:	work RAM
	$2400-$3fff:	video RAM
	
	$4000-:		RAM mirror

Ports:
	Read 1
	BIT	0	coin (0 when active)
		1	P2 start button
		2	P1 start button
		3	?
		4	P1 shoot button
		5	P1 joystick left
		6	P1 joystick right
		7	?
	
	Read 2
	BIT	0,1	dipswitch number of lives (0:3,1:4,2:5,3:6)
		2	tilt 'button'
		3	dipswitch bonus life at 1:1000,0:1500
		4	P2 shoot button
		5	P2 joystick left
		6	P2 joystick right
		7	dipswitch coin info 1:off,0:on
	
	Read 3		shift register result
	
	Write 2		shift register result offset (bits 0,1,2)
	Write 3		sound related
	Write 4		fill shift register
	Write 5		sound related
	Write 6		strange 'debug' port? eg. it writes to this port when
			it writes text to the screen (0=a,1=b,2=c, etc)
	
	(write ports 3,5,6 can be left unemulated, read port 1=$01 and 2=$00
	will make the game run, but but only in attract mode)

I haven't looked into sound details.

16 bit shift register:

	The 8080 instruction set does not include opcodes for shifting. An 8 - bit pixel image must
	be shifted into a 16 - bit word for the desired bit - position on the screen. Space Invaders
	adds a hardware shift register to help with the math.

	f              0	bit
	xxxxxxxxyyyyyyyy
	
	Writing to port 4 shifts x into y, and the new value into x, eg.
	$0000,
	write $aa -> $aa00,
	write $ff -> $ffaa,
	write $12 -> $12ff, ..
	
	Writing to port 2 (bits 0,1,2) sets the offset for the 8 bit result, eg.
	offset 0:
	rrrrrrrr		result=xxxxxxxx
	xxxxxxxxyyyyyyyy
	
	offset 2:
	  rrrrrrrr	result=xxxxxxyy
	xxxxxxxxyyyyyyyy
	
	offset 7:
	       rrrrrrrr	result=xyyyyyyy
	xxxxxxxxyyyyyyyy
	
	Reading from port 3 returns said result.

Overlay dimensions (screen rotated 90 degrees anti-clockwise):
	,_______________________________.
	|WHITE            ^             |
	|                32             |
	|                 v             |
	|-------------------------------|
	|RED              ^             |
	|                32             |
	|                 v             |
	|-------------------------------|
	|WHITE                          |
	|         < 224 >               |
	|                               |
	|                 ^             |
	|                120            |
	|                 v             |
	|                               |
	|                               |
	|                               |
	|-------------------------------|
	|GREEN                          |
	| ^                  ^          |
	|56        ^        56          |
	| v       72         v          |
	|____      v      ______________|
	|  ^  |          | ^            |
	|<16> |  < 118 > |16   < 122 >  |
	|  v  |          | v            |
	|WHITE|          |         WHITE|
	`-------------------------------'
	
	Way of out of proportion :P
	
*/

#include "machine.h"

#include "debugger.h"
#include "Assert.h"
#include "Helpers.h"

static const unsigned int kClockSpeed = 2000000; // 2MHz
static const unsigned int kRefreshRate = 60; // Hz

enum class MemoryType
{
	ROM,
	RAM
};

struct Chunk
{
	size_t addressStart; // in machine address space
	size_t physicalMemoryStart;
	size_t sizeInBytes;
	MemoryType memoryType;
};

// Memory map :
//   ROM
//   $0000 - $07ff : invaders.h
//   $0800 - $0fff : invaders.g
//   $1000 - $17ff : invaders.f
//   $1800 - $1fff : invaders.e
//
//   RAM
//   $2000 - $23ff : work RAM
//   $2400 - $3fff : video RAM
//   
//   $4000 - : RAM mirror
//
// http://www.emutalk.net/threads/38177-Space-Invaders?s=e58df01e41111c4efc6f3207b2890054&p=359411&viewfull=1#post359411

static const Chunk kChunks[] =
{
	{0x0000, 0x0000, 0x2000, MemoryType::ROM},
	{0x2000, 0x2000, 0x2000, MemoryType::RAM},
	{0x4000, 0x2000, 0x2000, MemoryType::RAM}, // RAM mirror
};

// In a more complicated system would calculate these from Chunk array
static const size_t kRomPhysicalSizeBytes = 0x2000;
static const size_t kRamPhysicalSizeBytes = 0x2000;
static const size_t kPhysicalMemorySizeBytes = kRomPhysicalSizeBytes + kRamPhysicalSizeBytes;

static bool WriteByteToMemory(uint8_t* pMemory, size_t address, uint8_t val, bool fatalOnFail = false)
{
	HP_ASSERT(pMemory);

//	HP_ASSERT(address < 0x4000, "Looks like the RAM mirror *is* used. What is its purpose?");

	for(unsigned int chunkIndex = 0; chunkIndex < COUNTOF_ARRAY(kChunks); chunkIndex++)
	{
		const Chunk& chunk = kChunks[chunkIndex];
		if(chunk.memoryType == MemoryType::ROM)
			continue;

		if(address >= chunk.addressStart && address < chunk.addressStart + chunk.sizeInBytes)
		{
			size_t offset = address - chunk.addressStart;
			size_t physicalAddress = chunk.physicalMemoryStart + offset;
			HP_ASSERT(physicalAddress < kPhysicalMemorySizeBytes);
			pMemory[physicalAddress] = val;
			return true;
		}
	}

	if(fatalOnFail)
	{
		// #TODO: Maybe fail silently - think this is how MSX cartridge copy protection may work.
		HP_FATAL_ERROR("Failed to find RAM memory chunk containing address %u", address);
	}

	return false;
}

static uint8_t ReadByteFromMemory(uint8_t* pMemory, size_t address, bool fatalOnFail = false)
{
	HP_ASSERT(pMemory);

//	HP_ASSERT(address < 0x4000, "Looks like the RAM mirror *is* used. What is its purpose?");

	for(unsigned int chunkIndex = 0; chunkIndex < COUNTOF_ARRAY(kChunks); chunkIndex++)
	{
		const Chunk& chunk = kChunks[chunkIndex];
		if(address >= chunk.addressStart && address < chunk.addressStart + chunk.sizeInBytes)
		{
			size_t offset = address - chunk.addressStart;
			size_t physicalAddress = chunk.physicalMemoryStart + offset;
			HP_ASSERT(physicalAddress < kPhysicalMemorySizeBytes);
			uint8_t val = pMemory[physicalAddress];
			return val;
		}
	}

	if(fatalOnFail)
	{
		// #TODO: Maybe fail silently - think this is how MSX cartridge copy protection may work.
		HP_FATAL_ERROR("Failed to find RAM memory chunk containing address %u", address);
	}

	return 0;
}

static void TestMemory(uint8_t* pMemory)
{
	HP_ASSERT(pMemory);

	// ROM 0 - $1fff
	for(size_t address = 0; address < 0x2000; address++)
	{
		HP_ASSERT(WriteByteToMemory(pMemory, address, 0xcc) == false);
	}

	// RAM $2000 - $3fff
	for(size_t address = 0x2000; address < 0x4000; address++)
	{
		uint8_t val = (uint8_t)address;
		HP_ASSERT(WriteByteToMemory(pMemory, address, val) == true);
		HP_ASSERT(ReadByteFromMemory(pMemory, address) == val);
	}

	// RAM mirror $4000 - $5fff
	for(size_t address = 0x4000; address < 0x6000; address++)
	{
		uint8_t val = (uint8_t)address + 1;
		HP_ASSERT(WriteByteToMemory(pMemory, address, val) == true);
		HP_ASSERT(ReadByteFromMemory(pMemory, address) == val);
	}
}

struct Rom
{
	const char* fileName;
	size_t fileSizeBytes;
};

static const Rom kRoms[] =
{
	{"invaders.h", 0x800},
	{"invaders.g", 0x800},
	{"invaders.f", 0x800},
	{"invaders.e", 0x800},
};

static uint8_t In(uint8_t port, void* userdata)
{
	HP_ASSERT(userdata);
	Machine* pMachine = (Machine*)userdata;

	// http://www.emutalk.net/threads/38177-Space-Invaders?s=e58df01e41111c4efc6f3207b2890054&p=359411&viewfull=1#post359411

	// Space Invaders input ports

	// Read 1
	// BIT 0    coin(0 when active)
	//     1    P2 start button
	//     2    P1 start button
	//     3 ?
	//     4    P1 shoot button
	//     5    P1 joystick left
	//     6    P1 joystick right
	//     7 ?
	//     
	// Read 2
	// BIT 0, 1 dipswitch number of lives(0:3, 1 : 4, 2 : 5, 3 : 6)
	//     2    tilt 'button'
	//     3    dipswitch bonus life at 1 : 1000, 0 : 1500
	//     4    P2 shoot button
	//     5    P2 joystick left
	//     6    P2 joystick right
	//     7    dipswitch coin info 1 : off, 0 : on
	//     
	// Read 3   shift register result

	// Read port 1=$01 and 2=$00 will make the game run, but but only in attract mode.

	if(port == 1)
		return 1;
	else if(port == 2)
		return 0;
	else if(port == 3)
	{
		// Reading from port 3 returns the shifted register value.

		// Writing to port 2 (bits 0, 1, 2) sets the offset for the 8 bit result, eg.
		// offset 0:
		// rrrrrrrr		     result = xxxxxxxx
		// xxxxxxxxyyyyyyyy
		// 
		// offset 2 :
		//   rrrrrrrr	         result = xxxxxxyy
		// xxxxxxxxyyyyyyyy
		// 
		// offset 7 :
		//        rrrrrrrr      result = xyyyyyyy
		// xxxxxxxxyyyyyyyy
		// 

		// offset | shift
		//   0    |   8
		//   1    |   7
		//   2    |   6
		//   ...
		//   7    |   1
		//
		// So shift = 8 - offset

		HP_ASSERT(pMachine->shiftRegisterOffset <= 7);
		unsigned int shift = 8 - pMachine->shiftRegisterOffset;
		uint8_t result = (uint8_t)(pMachine->shiftRegisterValue >> shift);
		return result;
	}
	else
	{
		HP_FATAL_ERROR("Unexpected IN port: %u", port);
	}

	return 0;
}

static void Out(uint8_t port, uint8_t val, void* userdata)
{
	HP_UNUSED(val);
	HP_ASSERT(userdata);
	Machine* pMachine = (Machine*)userdata;

	// Write 2    shift register result offset (bits 0, 1, 2)
	// Write 3    sound related
	// Write 4    fill shift register
	// Write 5    sound related
	// Write 6    strange 'debug' port ? eg.it writes to this port when
	//            it writes text to the screen(0 = a, 1 = b, 2 = c, etc)
	// 
	// (write ports 3, 5, 6 can be left unemulated)
	// http://www.emutalk.net/threads/38177-Space-Invaders?s=e58df01e41111c4efc6f3207b2890054&p=359411&viewfull=1#post359411

	switch(port)
	{
	case 2:
		// Writing to port 2 (bits 0, 1, 2) sets the offset for the 8 bit result,
		HP_ASSERT(val <= 7);
		pMachine->shiftRegisterOffset = val;
		break;
	case 3:
		// #TODO: Implement sound
		break;
	case 4:
	{
		// Write into Shift Register
		// 
		// f              0	bit
		// xxxxxxxxyyyyyyyy
		//
		// Writing to port 4 shifts x into y, and the new value into x, eg.
		//	$0000,
		//	write $aa->$aa00,
		//	write $ff->$ffaa,
		//	write $12->$12ff, ..
		//
		// http://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html#DedicatedShiftHardware

		pMachine->shiftRegisterValue = pMachine->shiftRegisterValue >> 8;
		//		s_shiftRegisterValue &= 0x00ff; // redundant; the above shift will shift in zeros from the left
		pMachine->shiftRegisterValue |= (uint16_t)val << 8;
		break;
	}
	case 5:
		// #TODO: Implement sound
		break;
	case 6:
		// #TODO: Is this the "Watchdog"? http://computerarcheology.com/Arcade/SpaceInvaders/Code.html
		break;
	default:
		HP_FATAL_ERROR("Unexpected OUT port: %u", port);
	}
}

//------------------------------------------------------------------------------
// Load ROMs into ROM memory

static bool loadRoms(uint8_t* pMemory, size_t memorySizeBytes)
{
	size_t address = 0;
	for(unsigned int romIndex = 0; romIndex < COUNTOF_ARRAY(kRoms); romIndex++)
	{
		const Rom& rom = kRoms[romIndex];

		const char* fileName = rom.fileName;
		FILE* pFile = fopen(fileName, "rb");
		if(!pFile)
		{
			fprintf(stderr, "Failed to open file: %s\n", fileName);
			return false;
		}

		fseek(pFile, 0, SEEK_END);
		size_t fileSizeBytes = ftell(pFile);
		printf("Opened file \"%s\" size %u (0x%X)\n", fileName, (unsigned int)fileSizeBytes, (unsigned int)fileSizeBytes);

		if(fileSizeBytes != rom.fileSizeBytes)
		{
			fprintf(stderr, "ROM file \"%s\" is incorrect size. Expected %u, got %u\n", fileName, rom.fileSizeBytes, fileSizeBytes);
			return false;
		}
		fseek(pFile, 0, SEEK_SET);

		if(memorySizeBytes - address < fileSizeBytes)
		{
			fprintf(stderr, "Insufficient memory to load ROM file: %s\n", fileName);
			return false;
		}

		fread(pMemory + address, 1, fileSizeBytes, pFile);
		fclose(pFile);
		pFile = nullptr;

		address += fileSizeBytes;
	}

	return true;
}

bool CreateMachine(Machine** ppMachine)
{
	Machine* pMachine = new Machine;
	*pMachine = {};
	pMachine->cpu = {};

	pMachine->cpu.userdata = pMachine; // for callbacks

	// Init memory
	// #TODO: Should really assert that address space regions don't overlap, or if they do that
	//        they are of the same type? 
	pMachine->cpu.pMemory = new uint8_t[kPhysicalMemorySizeBytes];
	pMachine->cpu.memorySizeBytes = kPhysicalMemorySizeBytes;
	pMachine->cpu.readByteFromMemory = ReadByteFromMemory;
	pMachine->cpu.writeByteToMemory = WriteByteToMemory;

	//	TestMemory(state.pMemory);

	if(!loadRoms(pMachine->cpu.pMemory, pMachine->cpu.memorySizeBytes))
	{
		fprintf(stderr, "Failed to load ROMs\n");
		DestroyMachine(pMachine);
		return false;
	}

#if 1
	// Zero RAM
	// #TODO: Is this expected or required? Maybe just helpful for debugging. Write 0xCC maybe? 
	for(size_t address = kRomPhysicalSizeBytes; address < kPhysicalMemorySizeBytes; address++)
	{
		pMachine->cpu.pMemory[address] = 0x00;
	}
#endif

	// set up machine inputs and outputs, for assembly IN and OUT instructions
	pMachine->cpu.in = In;
	pMachine->cpu.out = Out;

	HP_ASSERT((Machine::kDisplayWidth % 8) == 0);
	unsigned int displaySizeBytes = (Machine::kDisplayWidth / 8) * Machine::kDisplayHeight;
	pMachine->pDisplayBuffer = new uint8_t[displaySizeBytes];
	// #TODO: Zero display buffer?

	*ppMachine = pMachine;
	return true;
}

void DestroyMachine(Machine* pMachine)
{
	HP_ASSERT(pMachine);

	delete[] pMachine->cpu.pMemory;
	pMachine->cpu.pMemory = nullptr;

	delete[] pMachine->pDisplayBuffer;
	pMachine->pDisplayBuffer = nullptr;

	delete pMachine;
}

// returns true if still running
bool StepFrame(Machine* pMachine, bool debug)
{
	HP_ASSERT(pMachine);

	const double cyclesPerSecond = kClockSpeed;
	const double secondsPerFrame = 1.0 / kRefreshRate;
	const double cyclesPerFrame = cyclesPerSecond * secondsPerFrame;
	const double cyclesPerScanLine = cyclesPerFrame / Machine::kDisplayHeight; // #TODO: Account for VBLANK time

	unsigned int cycleCount = 0;
	unsigned int prevScanLine = 0;
	while(cycleCount < cyclesPerFrame)
	{
		unsigned int scanLine = (unsigned int)((double)cycleCount / cyclesPerScanLine);

		// #TODO: Interrupt $cf (RST 1) at the start of vblank http://www.emutalk.net/threads/38177-Space-Invaders?s=e58df01e41111c4efc6f3207b2890054&p=359411&viewfull=1#post359411
		//        OR..
		//        ScanLine96:
		//            Interrupt brings us here when the beam is* near* the middle of the screen. The real middle
		//            would be 224 / 2 = 112. The code pretends this interrupt happens at line 128.
		//        http://computerarcheology.com/Arcade/SpaceInvaders/Code.html
		if(scanLine == 96 && prevScanLine < scanLine)
		{
			Generate8080Interrupt(pMachine->cpu, 1);
		}

		if(debug)
		{
			Disassemble8080(pMachine->cpu.pMemory, kPhysicalMemorySizeBytes, pMachine->cpu.PC);
			printf("    ");
			Print8080State(pMachine->cpu);

			// #TODO: Print scanline number
		}

		unsigned int instructionCycleCount = Emulate8080Instruction(pMachine->cpu);
		cycleCount += instructionCycleCount;

		prevScanLine = scanLine;
	}

	// #TODO: Interrupts $d7 (RST 2) at the end of vblank. http://www.emutalk.net/threads/38177-Space-Invaders?s=e58df01e41111c4efc6f3207b2890054&p=359411&viewfull=1#post359411
	//        OR..
	//        ScanLine224:
	//            Interrupt brings us here when the beam is at the end of the screen(line 224) when 
	//            the VBLANK begins.
	//        http://computerarcheology.com/Arcade/SpaceInvaders/Code.html
	unsigned int scanLine = (unsigned int)((double)cycleCount / cyclesPerScanLine);
	HP_ASSERT(scanLine == 224);
	Generate8080Interrupt(pMachine->cpu, 2);

	return true;
}

/*
http://www.emutalk.net/threads/38177-Space-Invaders?s=e58df01e41111c4efc6f3207b2890054&p=359411&viewfull=1#post359411
http://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html#DedicatedShiftHardware

Space Invaders, (C) Taito 1978, Midway 1979

CPU: Intel 8080 @ 2MHz (CPU similar to the (newer) Zilog Z80)

Interrupts: $cf (RST 8) at the start of vblank, $d7 (RST $10) at the end of vblank.

Video: 256(x)*224(y) @ 60Hz, vertical monitor.
Colours are simulated with a plastic transparent overlay and a background picture.
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

#include "Display.h"
#include "Audio.h"
#include "Input.h"
#include "hp_assert.h"
#include "Helpers.h"

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

bool WriteByteToMemory(void* userdata, uint16_t address, uint8_t val, bool fatalOnFail /*= false*/)
{
	HP_ASSERT(userdata);
	Machine* pMachine = (Machine*)userdata;
	uint8_t* pMemory = pMachine->pMemory;
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

uint8_t ReadByteFromMemory(const void* userdata, uint16_t address, bool fatalOnFail /*= false*/)
{
	HP_ASSERT(userdata);
	Machine* pMachine = (Machine*)userdata;
	uint8_t* pMemory = pMachine->pMemory;
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

#ifndef RELEASE

static void TestMemory(uint8_t* pMemory)
{
	HP_ASSERT(pMemory);

	// ROM 0 - $1fff
	for(uint16_t address = 0; address < 0x2000; address++)
	{
		HP_ASSERT(WriteByteToMemory(pMemory, address, 0xcc) == false);
	}

	// RAM $2000 - $3fff
	for(uint16_t address = 0x2000; address < 0x4000; address++)
	{
		uint8_t val = (uint8_t)address;
		HP_ASSERT(WriteByteToMemory(pMemory, address, val) == true);
		HP_ASSERT(ReadByteFromMemory(pMemory, address) == val);
	}

	// RAM mirror $4000 - $5fff
	for(uint16_t address = 0x4000; address < 0x6000; address++)
	{
		uint8_t val = (uint8_t)address + 1;
		HP_ASSERT(WriteByteToMemory(pMemory, address, val) == true);
		HP_ASSERT(ReadByteFromMemory(pMemory, address) == val);
	}
}
#endif

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
	MachineInput& input = pMachine->input;

	// http://www.emutalk.net/threads/38177-Space-Invaders?s=e58df01e41111c4efc6f3207b2890054&p=359411&viewfull=1#post359411

	// Read port 1=$01 and 2=$00 will make the game run, but but only in attract mode.

	if(port == 1)
	{
		// Read 1
		// BIT 0    coin(0 when active)
		// BIT 1    P2 start button
		// BIT 2    P1 start button
		// BIT 3 ?
		// BIT 4    P1 shoot button
		// BIT 5    P1 joystick left
		// BIT 6    P1 joystick right
		// BIT 7 ?
		uint8_t val = 0x00;
		val |= input.coinInserted ? 0 : (1 << 0);
		val |= input.player2StartButton ? (1 << 1) : 0;
		val |= input.player1StartButton ? (1 << 2) : 0;
		val |= input.player1ShootButton ? (1 << 4) : 0;
		val |= input.player1JoystickLeft ? (1 << 5) : 0;
		val |= input.player1JoystickRight ? (1 << 6) : 0;
		return val;
	}
	else if(port == 2)
	{
		uint8_t val = 0x00;

		// Read 2
		// BITS 0 and 1  dipswitch number of lives (0:3, 1 : 4, 2 : 5, 3 : 6)
		// BIT 2         tilt 'button'
		// BIT 3         dipswitch bonus life at 1 : 1000, 0 : 1500
		// BIT 4         P2 shoot button
		// BIT 5         P2 joystick left
		// BIT 6         P2 joystick right
		// BIT 7         dipswitch coin info 1 : off, 0 : on

		// copy in the required DIP switch bits
		uint8_t dipSwitchMask = 0b10001011; // see Machine.dipSwitchBits 
		val = pMachine->dipSwitchBits & dipSwitchMask;

		val |= input.tilt ? (1<<2) : 0;
		val |= input.player2ShootButton ? (1 << 4) : 0;
		val |= input.player2JoystickLeft ? (1 << 5) : 0;
		val |= input.player2JoystickRight ? (1 << 6) : 0;

		return val;
	}
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
		// #TODO: Trigger samples
		// bit 0 = UFO (repeats)       SX0 0.raw
		// bit 1 = Shot                SX1 1.raw
		// bit 2 = Flash (player die)  SX2 2.raw
		// bit 3 = Invader die         SX3 3.raw
		// bit 4 = Extended play       SX4
		// bit 5 = AMP enable          SX5
		// bit 6 = NC (not wired)
		// bit 7 = NC (not wired)
		// http://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html
		
		if((val & (1<<0)) && !(pMachine->prevOut3 & (1<<0)))
		{
			PlaySample(0);
		}
		else if(!(val & (1<<0)) && (pMachine->prevOut3 & (1<<0)))
		{
			StopSample(0);
		}

		if((val & (1<<1)) && !(pMachine->prevOut3 & (1<<1)))
			PlaySample(1); // Shot

		if((val & (1<<2)) && !(pMachine->prevOut3 & (1<<2)))
			PlaySample(2); // Flash (player die)

		if((val & (1<<3)) && !(pMachine->prevOut3 & (1<<3)))
			PlaySample(3); // Invader die

		if((val & (1<<4)) && !(pMachine->prevOut3 & (1<<4)))
		{
			static bool first = true;
			if(first)
			{
				printf("Out port 3 bit 4 Extended play SX4 not implemented\n");
				first = false;
			}
		}

		if((val & (1<<5)) && !(pMachine->prevOut3 & (1<<5)))
		{
			static bool first = true;
			if(first)
			{
				printf("Out port 3 bit 5 AMP enable SX5 not implemented\n");
				first = false;
			}
		}

		pMachine->prevOut3 = val;
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
		// #TODO: Trigger samples
		// bit 0 = Fleet movement 1     SX6 4.raw
		// bit 1 = Fleet movement 2     SX7 5.raw
		// bit 2 = Fleet movement 3     SX8 6.raw
		// bit 3 = Fleet movement 4     SX9 7.raw
		// bit 4 = UFO Hit              SX10 8.raw
		// bit 5 = NC (Cocktail mode control ... to flip screen)
		// bit 6 = NC (not wired)
		// bit 7 = NC (not wired)
		// http://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html

		if((val & (1<<0)) && !(pMachine->prevOut5 & (1<<0)))
			PlaySample(4); // Fleet movement 1

		if((val & (1<<1)) && !(pMachine->prevOut5 & (1<<1)))
			PlaySample(5); // Fleet movement 2

		if((val & (1<<2)) && !(pMachine->prevOut5 & (1<<2)))
			PlaySample(6); // Fleet movement 3

		if((val & (1<<3)) && !(pMachine->prevOut5 & (1<<3)))
			PlaySample(7); // Fleet movement 4

		if((val & (1<<4)) && !(pMachine->prevOut5 & (1<<4)))
			PlaySample(8); // UFO Hit

		if((val & (1<<5)) && !(pMachine->prevOut5 & (1<<5)))
		{
			static bool first = true;
			if(first)
			{
				printf("Out port 5 bit 5 (Cocktail mode control ... to flip screen) not implemented\n");
				first = false;
			}
		}

		pMachine->prevOut5 = val;
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
			fprintf(stderr, "ROM file \"%s\" is incorrect size. Expected %u, got %u\n", fileName, (unsigned int)rom.fileSizeBytes, (unsigned int)fileSizeBytes);
			return false;
		}
		fseek(pFile, 0, SEEK_SET);

		if(memorySizeBytes - address < fileSizeBytes)
		{
			fprintf(stderr, "Insufficient memory to load ROM file: %s\n", fileName);
			return false;
		}

		if(fread(pMemory + address, 1, fileSizeBytes, pFile) != fileSizeBytes)
		{
			fprintf(stderr, "Failed to read file: %s\n", fileName);
			return false;
		}
		fclose(pFile);
		pFile = nullptr;

		address += fileSizeBytes;
	}

	return true;
}

static void copyVideoMemoryToDisplayBuffer(Machine& machine)
{
	HP_ASSERT((Machine::kDisplayWidth % 8) == 0);
	const unsigned int bytesPerRow = Machine::kDisplayWidth >> 3; // div 8
	unsigned int displaySizeBytes = bytesPerRow * Machine::kDisplayHeight;
	for(uint16_t i = 0; i < displaySizeBytes; i++)
	{
		const uint16_t kVideoAddress = 0x2400;
		uint16_t address = kVideoAddress + i;
		uint8_t val = ReadByteFromMemory(&machine, address, /*fatalOnFail*/true);
		machine.pDisplay->SetByte(i, val);
	}
}

bool CreateMachine(Machine** ppMachine, Display* pDisplay)
{
	Machine* pMachine = new Machine;
	*pMachine = {};
	pMachine->cpu = {};

	pMachine->cpu.userdata = pMachine; // for callbacks

	// Init memory
	// #TODO: Should really assert that address space regions don't overlap, or if they do that
	//        they are of the same type? 
	pMachine->pMemory = new uint8_t[kPhysicalMemorySizeBytes];
	pMachine->memorySizeBytes = kPhysicalMemorySizeBytes;
	pMachine->cpu.readByteFromMemory = ReadByteFromMemory;
	pMachine->cpu.writeByteToMemory = WriteByteToMemory;

	//	TestMemory(state.pMemory);

	pMachine->pDisplay = pDisplay;

	if(!loadRoms(pMachine->pMemory, pMachine->memorySizeBytes))
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
		pMachine->pMemory[address] = 0x00;
	}
#endif

	// set up machine inputs and outputs, for assembly IN and OUT instructions
	pMachine->cpu.in = In;
	pMachine->cpu.out = Out;

	*ppMachine = pMachine;
	return true;
}

void DestroyMachine(Machine* pMachine)
{
	HP_ASSERT(pMachine);

	delete[] pMachine->pMemory;
	pMachine->pMemory = nullptr;

	delete pMachine;
}

void ResetMachine(Machine* pMachine)
{
	HP_ASSERT(pMachine);
	Reset(pMachine->cpu);
	pMachine->frameCount = 0;
	pMachine->frameCycleCount = 0;
	pMachine->scanLine = 0;
}

static void getInput(Machine* pMachine)
{
	HP_ASSERT(pMachine);

	// #TODO: Controller input too
 	MachineInput& input = pMachine->input;
	input = {};
	input.coinInserted = Input::IsKeyDownThisFrame(SDL_SCANCODE_5);
 	input.player1StartButton = Input::IsKeyDownThisFrame(SDL_SCANCODE_1) || Input::IsButtonDownThisFrame(0, SDL_CONTROLLER_BUTTON_START) || Input::IsButtonDownThisFrame(0, SDL_CONTROLLER_BUTTON_A);
 	input.player2StartButton = Input::IsKeyDownThisFrame(SDL_SCANCODE_2) || Input::IsButtonDownThisFrame(1, SDL_CONTROLLER_BUTTON_START) || Input::IsButtonDownThisFrame(1, SDL_CONTROLLER_BUTTON_A);
 	input.player1ShootButton = Input::GetKeyState(SDL_SCANCODE_SPACE) || Input::GetButtonState(0, SDL_CONTROLLER_BUTTON_A);
 	input.player1JoystickLeft = Input::GetKeyState(SDL_SCANCODE_LEFT) || Input::GetButtonState(0, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || Input::GetAxisValue(0, SDL_CONTROLLER_AXIS_LEFTX) < -0.5f;
 	input.player1JoystickRight = Input::GetKeyState(SDL_SCANCODE_RIGHT) || Input::GetButtonState(0, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || Input::GetAxisValue(0, SDL_CONTROLLER_AXIS_LEFTX) > 0.5f;
 	input.player2ShootButton = Input::GetKeyState(SDL_SCANCODE_Q) || Input::GetButtonState(1, SDL_CONTROLLER_BUTTON_A);
 	input.player2JoystickLeft = Input::GetKeyState(SDL_SCANCODE_O) || Input::GetButtonState(1, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || Input::GetAxisValue(1, SDL_CONTROLLER_AXIS_LEFTX) < -0.5f;
 	input.player2JoystickRight = Input::GetKeyState(SDL_SCANCODE_P) || Input::GetButtonState(1, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || Input::GetAxisValue(1, SDL_CONTROLLER_AXIS_LEFTX) > 0.5f;
#if 1
	// Player 2 share Player controls
	input.player2ShootButton |= input.player1ShootButton;
	input.player2JoystickLeft |= input.player1JoystickLeft;
	input.player2JoystickRight |= input.player1JoystickRight;
#endif

 	input.tilt = Input::GetKeyState(SDL_SCANCODE_T);
}

void StepFrame(Machine* pMachine, bool verbose)
{
	HP_ASSERT(pMachine);
//	HP_ASSERT(pMachine->frameCycleCount == 0); // Assert not valid - it is perfectly valid to step to end of frame from part way through

	getInput(pMachine);

	do 
	{
		if(pMachine->running)
			StepInstruction(pMachine, verbose);

	} while (pMachine->frameCycleCount != 0 && pMachine->running);
}

void StepInstruction(Machine* pMachine, bool verbose)
{
	HP_ASSERT(pMachine);

	const double cyclesPerSecond = State8080::kClockRate;
	const double secondsPerFrame = 1.0 / kRefreshRate;
	const double cyclesPerFrame = cyclesPerSecond * secondsPerFrame;
	const double cyclesPerScanLine = cyclesPerFrame / Machine::kDisplayHeight; // #TODO: Account for VBLANK time

	const unsigned int prevScanLine = (unsigned int)((double)pMachine->frameCycleCount / cyclesPerScanLine);
	unsigned int instructionCycleCount = Emulate8080Instruction(pMachine->cpu);
	pMachine->frameCycleCount += instructionCycleCount;
	pMachine->scanLine = (unsigned int)((double)pMachine->frameCycleCount / cyclesPerScanLine);

	// Generate interrupt RST 1 at ScanLine96
	// Interrupt brings us here when the beam is* near* the middle of the screen. 
	// http://computerarcheology.com/Arcade/SpaceInvaders/Code.html
	// #TODO: Should this be at the start of vblank? http://www.emutalk.net/threads/38177-Space-Invaders?s=e58df01e41111c4efc6f3207b2890054&p=359411&viewfull=1#post359411
	if(pMachine->scanLine == 96 && prevScanLine < pMachine->scanLine)
	{
		if(verbose)
			printf("Generating Interrupt RST 1 at scanline 96\n");

		// #TODO: Interrupt callback
		Generate8080Interrupt(pMachine->cpu, 1);
	}

	// Generate interrupt RST 3 at ScanLine 224 (end of screen / start of vblank)
	// http://computerarcheology.com/Arcade/SpaceInvaders/Code.html
	// #TODO: Should this be at the end of the VBLANK? http://www.emutalk.net/threads/38177-Space-Invaders?s=e58df01e41111c4efc6f3207b2890054&p=359411&viewfull=1#post359411
	if(pMachine->scanLine == 224 && prevScanLine < pMachine->scanLine)
	{
		if(verbose)
			printf("Generating Interrupt RST 2 at scanline 224\n");

		Generate8080Interrupt(pMachine->cpu, 2);

		// Approximate display buffer by copying instantaneously from RAM at and of frame.
		// #TODO: Accurate display buffer generation by copying pixel by pixel as the CPU / raster progresses. 
		copyVideoMemoryToDisplayBuffer(*pMachine);

		// #TODO: Should there be a VBLANK delay here?
		pMachine->frameCount++;
		pMachine->frameCycleCount = 0;
		pMachine->scanLine = 0;
	}

	if(pMachine->debugHook)
		pMachine->debugHook(pMachine);
}

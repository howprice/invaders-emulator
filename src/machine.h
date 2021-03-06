// Midway 8080 Black & White machine
// See Mame mw8080bw.cpp

#pragma once

#include "8080.h"

struct Machine;
class Display;

typedef void (*DebugHookFuncPtr)(Machine* pMachine);

struct MachineInput
{
	bool coinInserted;
	bool player1StartButton;
	bool player2StartButton;
	bool player1ShootButton;
	bool player1JoystickLeft;
	bool player1JoystickRight;
	bool player2ShootButton;
	bool player2JoystickLeft;
	bool player2JoystickRight;
	bool tilt;
};

struct Machine
{
	static const unsigned int kDisplayWidth = 256;
	static const unsigned int kDisplayHeight = 224;

	bool running = true;
	State8080 cpu;

	uint8_t* pMemory;
	uint32_t memorySizeBytes; // #TODO: Get rid of this. The memory map should allow gaps, so memory should be accessed in chunks

	Display* pDisplay;

	unsigned int frameCount;
	unsigned int frameCycleCount;
	unsigned int scanLine;

	uint16_t shiftRegisterValue;
	uint8_t shiftRegisterOffset;        // [0,7]

	uint16_t prevOut3 = 0; // #TODO: Move into Audio?
	uint16_t prevOut5 = 0; // #TODO: Move into Audio?

	MachineInput input;

	// bits 0 and 1  number of lives
	// bit 2  Check RAM and sound
	// bit 3  Bonus life at 1 : 1000, 0 : 1500
	// bits 4,5,6  Unknown
	// bit 7  No pricing on screen
	uint8_t dipSwitchBits;

	DebugHookFuncPtr debugHook;
};

// pDisplay can be null
bool CreateMachine(Machine** ppMachine, Display* pDisplay);
void DestroyMachine(Machine* pMachine);

void ResetMachine(Machine* pMachine);

bool WriteByteToMemory(void* userdata, uint16_t address, uint8_t val, bool fatalOnFail = false);
uint8_t ReadByteFromMemory(const void* userdata, uint16_t address, bool fatalOnFail = false);

void StepFrame(Machine* pMachine, bool verbose);
void StepInstruction(Machine* pMachine, bool verbose);

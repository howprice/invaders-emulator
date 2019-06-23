// Midway 8080 Black & White machine
// See Mame mw8080bw.cpp

#pragma once

#include "8080.h"

struct Machine
{
	static const unsigned int kDisplayWidth = 256;
	static const unsigned int kDisplayHeight = 224;

	State8080 cpu;
	unsigned int frameCycleCount;
	unsigned int scanLine;

	uint16_t shiftRegisterValue;
	uint8_t shiftRegisterOffset;        // [0,7]

	uint8_t* pDisplayBuffer;            // w * h * 1 bit per pixel

	bool coinInserted;
	bool player2StartButton;
	bool player1StartButton;
	bool player1ShootButton;
	bool player1JoystickLeft;
	bool player1JoystickRight;
	bool player2ShootButton;
	bool player2JoystickLeft;
	bool player2JoystickRight;
	bool tilt;

	// bits 0 and 1  number of lives
	// bit 2  Check RAM and sound
	// bit 3  Bonus life at 1 : 1000, 0 : 1500
	// bits 4,5,6  Unknown
	// bit 7  No pricing on screen
	uint8_t dipSwitchBits;
};

bool CreateMachine(Machine** ppMachine);
void DestroyMachine(Machine* pMachine);

void ResetMachine(Machine* pMachine);
void StartFrame(Machine* pMachine);

void StepFrame(Machine* pMachine, bool verbose);
void StepInstruction(Machine* pMachine, bool verbose);

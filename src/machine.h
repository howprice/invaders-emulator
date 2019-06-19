// Midway 8080 Black & White machine
// See Mame mw8080bw.cpp

#pragma once

#include "8080.h"

struct Machine
{
	static const unsigned int kDisplayWidth = 256;
	static const unsigned int kDisplayHeight = 224;

	State8080 cpu;
	uint16_t shiftRegisterValue;
	uint8_t shiftRegisterOffset;        // [0,7]
	uint8_t* pDisplayBuffer;            // w * h * 1 bit per pixel
};

bool CreateMachine(Machine** ppMachine);
void DestroyMachine(Machine* pMachine);

// returns true if still running
bool StepFrame(Machine* pMachine, bool debug);

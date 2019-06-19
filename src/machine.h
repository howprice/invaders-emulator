// Midway 8080 Black & White machine
// See Mame mw8080bw.cpp

#pragma once

#include "8080.h"

struct Machine
{
	State8080 cpu;
	uint16_t shiftRegisterValue = 0x0000;
	uint8_t shiftRegisterOffset = 0;        // [0,7]
};

bool CreateMachine(Machine** ppMachine);
void DestroyMachine(Machine* pMachine);

// returns true if still running
bool StepFrame(Machine* pMachine, bool debug);

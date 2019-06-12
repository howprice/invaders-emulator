#include <stdint.h>

// https://en.wikipedia.org/wiki/Intel_8080#Registers

// #TODO: What is the correct initial value of registers? Zero or garbage?
// #TODO: How to represent flags?

struct State8080
{
	uint8_t A;         // A and Flags are the Program Status Word (PSW)
	uint8_t Flags;

	uint8_t B;
	uint8_t C;

	uint8_t D;
	uint8_t E;

	uint8_t H;
	uint8_t L;

	uint16_t SP;
	uint16_t PC;

	uint8_t* pMemory;

	// #TODO: interrupts enabled state
};

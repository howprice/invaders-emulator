#ifndef HP_8080_H
#define HP_8080_H

#include <stdint.h>

// https://en.wikipedia.org/wiki/Intel_8080#Registers

// #TODO: What is the correct initial value of registers? Zero or garbage?
// #TODO: How to represent flags?

typedef uint8_t(*ReadByteFromMemoryFuncPtr)(uint8_t* pMemory, size_t address, bool fatalOnFail);
typedef bool(*WriteByteToMemoryFuncPtr)(uint8_t* pMemory, size_t address, uint8_t val, bool fatalOnFail);

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
	uint32_t memorySizeBytes;

	// #TODO: interrupts enabled state

	ReadByteFromMemoryFuncPtr readByteFromMemory;
	WriteByteToMemoryFuncPtr writeByteToMemory;
};

// returns instruction size in bytes
unsigned int Disassemble8080(const uint8_t* buffer, const size_t bufferSize, unsigned int pc);

void Emulate8080Instruction(State8080& state);

#endif

#ifndef HP_8080_H
#define HP_8080_H

#include <stdint.h>

// https://en.wikipedia.org/wiki/Intel_8080#Registers

// #TODO: What is the correct initial value of registers? Zero or garbage?
// #TODO: How to represent flags?

typedef uint8_t(*ReadByteFromMemoryFuncPtr)(uint8_t* pMemory, size_t address, bool fatalOnFail);
typedef bool(*WriteByteToMemoryFuncPtr)(uint8_t* pMemory, size_t address, uint8_t val, bool fatalOnFail);

struct Flags8080
{
	// LSB
	uint8_t C : 1;          // Carry - Set if the last addition operation resulted in a carry or if the last subtraction operation required a borrow
	uint8_t unusedBit1 : 1;
	uint8_t P : 1;          // Parity - Set if the number of 1 bits in the result is even.
	uint8_t unusedBit3 : 1;
	uint8_t AC : 1;         // Auxiliary Carry - used for binary-coded decimal arithmetic (BCD). Not required for Space Invaders
	uint8_t unusedBit5 : 1;
	uint8_t Z : 1;          // Zero - Set if the result is zero
	uint8_t S : 1;          // Sign - Set if the result is negative i.e. bit 7 is set
	// MSB
};
static_assert(sizeof(Flags8080) == 1, "Flags should be 1 byte in size");

struct State8080
{
	uint8_t A;         // A and Flags togther make up the Program Status Word (PSW)
	Flags8080 flags;

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

	uint8_t interruptsEnabled; // the 8080 "INTE" bit

	ReadByteFromMemoryFuncPtr readByteFromMemory;
	WriteByteToMemoryFuncPtr writeByteToMemory;
};

// returns instruction size in bytes
unsigned int Disassemble8080(const uint8_t* buffer, const size_t bufferSize, unsigned int pc);

void Emulate8080Instruction(State8080& state);
void Generate8080Interrupt(State8080& state, unsigned int interruptNumber);

#endif

#ifndef HP_8080_H
#define HP_8080_H

#include <stdint.h>

static const unsigned int kMinInstructionSizeBytes = 1;
static const unsigned int kMaxInstructionSizeBytes = 3;

// https://en.wikipedia.org/wiki/Intel_8080#Registers

// #TODO: What is the correct initial value of registers? Zero or garbage?

typedef uint8_t(*ReadByteFromMemoryFuncPtr)(void* userdata, uint16_t address, bool fatalOnFail);
typedef bool(*WriteByteToMemoryFuncPtr)(void* userdata, uint16_t address, uint8_t val, bool fatalOnFail);
typedef uint8_t(*InFuncPtr)(uint8_t port, void* userdata);
typedef void(*OutFuncPtr)(uint8_t port, uint8_t val, void* userdata);

struct Flags8080
{
	// LSB
	uint8_t C : 1;          // Carry - Set if the last addition operation resulted in a carry or if the last subtraction operation required a borrow
	uint8_t unusedBit1 : 1;
	uint8_t P : 1;          // Parity - Set if the number of 1 bits in the result is even.
	uint8_t unusedBit3 : 1;
	uint8_t AC : 1;         // Auxiliary Carry. Indicates a carry out of bit 3 of the accumulator. Affected by all add, subtract, increment, decrement, compare, and all logical AND, OR, XOR instructions. Only read by DAA instruction.
	uint8_t unusedBit5 : 1;
	uint8_t Z : 1;          // Zero - Set if the result is zero
	uint8_t S : 1;          // Sign - Set if the result is negative i.e. bit 7 is set
	// MSB
};
static_assert(sizeof(Flags8080) == 1, "Flags should be 1 byte in size");

struct State8080
{
	static const unsigned int kClockRate = 2000000; // 2MHz

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

	uint8_t INTE; // "INTE", the 8080 interrupt enable flip-flop [Data Book]

	uint8_t halt; // true if the HLT instruction has been called

	ReadByteFromMemoryFuncPtr readByteFromMemory;
	WriteByteToMemoryFuncPtr writeByteToMemory;
	InFuncPtr in;
	OutFuncPtr out;
	void* userdata = nullptr;
};

// returns instruction size in bytes
unsigned int Disassemble8080(const uint8_t* buffer, const size_t bufferSize, unsigned int pc);

const char* GetInstructionMnemonic(uint8_t opcode);
unsigned int GetInstructionSizeBytes(uint8_t opcode);

// returns the number of cycles that the instruction took to execute
unsigned int Emulate8080Instruction(State8080& state);

void Generate8080Interrupt(State8080& state, unsigned int interruptNumber);

// Forces execution of commands located at address 0000. The content of other processor registers is not modified.
// https://en.wikipedia.org/wiki/Intel_8080#Pin_use
void Reset(State8080& state);

// helper function
uint16_t GetNextInstructionAddress(const State8080& state);

bool CurrentInstructionIsACall(const State8080& state);

#endif

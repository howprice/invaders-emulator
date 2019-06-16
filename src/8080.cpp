// #TODO: Implement OUT, as seen at address 090E

#include "8080.h"

static const unsigned int kMinInstructionSizeBytes = 1;
static const unsigned int kMaxInstructionSizeBytes = 3;

#include "Helpers.h"
#include "Assert.h"

typedef void(*ExecuteInstruction)(State8080& state);

struct Instruction
{
	uint8_t opcode;
	const char* mnemonic;
	uint16_t sizeBytes;
	ExecuteInstruction execute;
};

static uint8_t readByteFromMemory(const State8080& state, uint16_t address)
{
	if(state.readByteFromMemory != nullptr)
	{
		uint8_t d8 = state.readByteFromMemory(state.pMemory, address, /*fatalOnFail*/true);
		return d8;
	}
	else
	{
		HP_ASSERT(address < state.memorySizeBytes);
		uint8_t d8 = state.pMemory[address];
		return d8;
	}
}

static void writeByteToMemory(const State8080& state, uint16_t address, uint8_t val)
{
	if(state.writeByteToMemory != nullptr)
	{
		state.writeByteToMemory(state.pMemory, address, val, /*fatalOnFail*/true);
	}
	else
	{
		HP_ASSERT(state.pMemory);
		HP_ASSERT(address < state.memorySizeBytes); // this is the best we can do with no access to memory map
		state.pMemory[address] = val;
	}
}

static uint8_t getInstructionD8(const State8080& state)
{
	// When this helper function is called during instruction execution,
	// the PC has already been advanced to the next sequential instruction
	// so the data has been skipped over
	HP_ASSERT(state.PC >= 1);
	uint16_t address = state.PC - 1;
	uint8_t d8 = readByteFromMemory(state, address);
	return d8;
}

static uint16_t getInstructionD16(const State8080& state)
{
	// see comment in getInstructionD8
	HP_ASSERT(state.PC >= 2);

	// data word (or address) is stored in little-endian
	uint8_t lsb = readByteFromMemory(state, state.PC - 2);
	uint8_t msb = readByteFromMemory(state, state.PC - 1);
	uint16_t d16 = (msb << 8) | lsb;
	return d16;
}

static uint16_t getInstructionAddress(const State8080& state)
{
	uint16_t address = getInstructionD16(state);
	HP_ASSERT(address < state.memorySizeBytes);
	return address;
}

static uint8_t calculateZeroFlag(uint8_t val)
{
	uint8_t Z = (val == 0) ? 1 : 0;
	return Z;
}

static uint8_t calculateSignFlag(uint8_t val)
{
	uint8_t S = (val & 0x80) != 0;
	return S;
}

static uint8_t calculateParityFlag(uint8_t val)
{
	unsigned int numBitsSet = 0;
	for(unsigned int bitIndex = 0; bitIndex < 8; bitIndex++)
	{
		if(val & (1 << bitIndex))
			numBitsSet++;
	}
	bool isEven = (numBitsSet & 1) == 0;
	uint8_t parity = isEven ? 1 : 0;
	return parity;
}

// 0x00 NOP
static void execute00(State8080& /*state*/)
{
	
}

// 0x01  LXI B,<d16>  aka LXI BC,<d16>  aka LD BC,<d16>
static void execute01(State8080& state)
{
	uint16_t d16 = getInstructionD16(state);
	uint8_t msb = (uint8_t)(d16 >> 8);
	uint8_t lsb = (uint8_t)(d16 & 0xff);

	// the first register in the register pair always holds the MSB
	state.B = msb;
	state.C = lsb;
}

// 0x05 DCR B
// B <- B-1
static void execute05(State8080& state)
{
	state.B--;

	state.flags.Z = calculateZeroFlag(state.B);
	state.flags.S = calculateSignFlag(state.B);
	state.flags.P = calculateParityFlag(state.B);
//	state.flags.AC = ; #TODO: Implement if required
}

// 0x09  DAD BC  aka ADD HL,BC
// HL <- HL + BC
// Sets Carry flag
static void execute09(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | (state.L);
	uint16_t BC = ((uint16_t)state.B << 8) | (state.C);

	// perform addition in 32-bits so can set carry flag
	uint32_t result = (uint32_t)HL + (uint32_t)BC;

	state.H = (uint8_t)(result >> 8);   // MSB
	state.L = (uint8_t)(result & 0xff); // LSB

	state.flags.C = (result & 0xffff0000) == 0 ? 0 : 1; // #TODO: Just test bit 16?
}

// 0x0D  DCR C  aka DEC C
// C <- C-1
// Sets: Z, S, P, AC	  
static void execute0D(State8080& state)
{
	state.C--;
	state.flags.Z = calculateZeroFlag(state.C);
	state.flags.S = calculateSignFlag(state.C);
	state.flags.P = calculateParityFlag(state.C);
	// #TODO: AC flag
}

// 0x0E  MVI C,d8
static void execute0E(State8080& state)
{
	uint8_t d8 = getInstructionD8(state);
	state.C = d8;
}

// 0x0F  RRC  aka RRCA
// Rotate Accumulator Right
// "The carry bit is set equal to the low-order bit of the accumulator.
// The contents of the accumulator are rotated on bit position to the right,
// with the low-order bit being transferred to the high-order bit position of
// the accumulator.
// Condition bits affected: Carry
// A = A >> 1  bit 7 = prev bit 0  Carry Flag = prev bit 0
static void execute0F(State8080& state)
{
	state.flags.C = state.A & 1;
	state.A = state.A >> 1;
	state.A |= (state.flags.C << 7);
}

// 0x06 MVI B, <d8>
static void execute06(State8080& state)
{
	state.B = getInstructionD8(state);
}

// 0x11 LXI D,<d16>
// D refers to the 16-bit register pair DE
// Encoding: 0x11 <lsb> <msb>
// D <- msb, E <- lsb
static void execute11(State8080& state)
{
	uint16_t d16 = getInstructionD16(state);
	state.D = (uint8_t)(d16 >> 8);
	state.E = (uint8_t)(d16 & 0xff);
}

// 0x13 INX D
// DE <- DE + 1
// Increment 16-bit number stored in DE by one
static void execute13(State8080& state)
{
	uint16_t D = ((uint16_t)state.D << 8) | (uint16_t)state.E;
	D++;
	state.D = (uint8_t)(D >> 8);
	state.E = (uint8_t)(D & 0xff);
}

// 0x19  DAD D aka ADD HL,DE
// HL <- HL + DE
// Sets Carry flag
static void execute19(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | (state.L);
	uint16_t DE = ((uint16_t)state.D << 8) | (state.E);

	// perform addition in 32-bits so can set carry flag
	uint32_t result = (uint32_t)HL + (uint32_t)DE;

	state.H = (uint8_t)(result >> 8);   // MSB
	state.L = (uint8_t)(result & 0xff); // LSB

	state.flags.C = (result & 0xffff0000) == 0 ? 0 : 1; // #TODO: Just test bit 16?
}

// 0x1A LDAX D
// D refers to the 16-bit register pair DE
// Copy byte from memory at address in DE into A
// A <- (DE)
static void execute1A(State8080& state)
{
	// MSB is always in the first register of the pair
	uint16_t address = (uint16_t)(state.D << 8) | (uint16_t)state.E;
	state.A = readByteFromMemory(state, address);
}

// 0x21 LXI H,<d16>
// H refers to the 16-bit register pair HL
// Encoding: 0x21 <lsb> <msb>
// H <- msb, L <- lsb
static void execute21(State8080& state)
{
	uint16_t d16 = getInstructionD16(state);
	state.H = (uint8_t)(d16 >> 8);
	state.L = (uint8_t)(d16 & 0xff);
}

// 0x23 INX H
// HL <- HL + 1
// Increment 16-bit number stored in HL by one
static void execute23(State8080& state)
{
	uint16_t H = ((uint16_t)state.H << 8) | (uint16_t)state.L;
	H++;
	state.H = (uint8_t)(H >> 8);
	state.L = (uint8_t)(H & 0xff);
}

// 0x26  MVI H,d8
// H <- byte 2
static void execute26(State8080& state)
{
	uint8_t d8 = getInstructionD8(state);
	state.H = d8;
}

// 0x29  DAD HL aka ADD HL,HL 
// HL <- HL + HL
// Sets Carry flag
static void execute29(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | (state.L);

	// perform addition in 32-bits so can set carry flag
	uint32_t result = (uint32_t)HL + (uint32_t)HL;

	state.H = (uint8_t)(result >> 8);   // MSB
	state.L = (uint8_t)(result & 0xff); // LSB

	state.flags.C = (result & 0xffff0000) == 0 ? 0 : 1; // #TODO: Just test bit 16?
}

// 0x31  LXI SP,d32
static void execute31(State8080& state)
{
	uint16_t address = getInstructionAddress(state);
	// #TODO: Assert that address is in RAM
	state.SP = address;
}

// 0x36  MVI M,d8
// The byte of immediate data is stored in the memory byte address stored in HL
// (HL) <- byte 2
static void execute36(State8080& state)
{
	uint16_t address = ((uint16_t)state.H << 8) | (uint16_t)state.L;
	uint8_t d8 = getInstructionD8(state);
	writeByteToMemory(state, address, d8);
}

// 0x56  MOV D,M
// D <- (HL)
static void execute56(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | state.L;
	uint8_t val = readByteFromMemory(state, HL);
	state.D = val;
}

// 0x5E  MOV E,M  aka LD E,(HL)
// E < -(HL)
static void execute5E(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | state.L;
	uint8_t val = readByteFromMemory(state, HL);
	state.E = val;
}

// 0x66  MOV H,M
// H <- (HL)
static void execute66(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | state.L;
	uint8_t val = readByteFromMemory(state, HL);
	state.H = val;
}

// 0x6F  MOV L,A
// L <- A
static void execute6F(State8080& state)
{
	state.L = state.A;
}

// 0x7A  MOV A,D
// A <- D
static void execute7A(State8080& state)
{
	state.A = state.D;
}

// 0x7C  MOV A,H
// A <- H
static void execute7C(State8080& state)
{
	state.A = state.H;
}

// 0x7E  MOV A,M
// A <- (HL)
static void execute7E(State8080& state)
{
	uint16_t HL = (uint16_t)(state.H << 8) | (uint16_t)state.L;
	state.A = readByteFromMemory(state, HL);
}

// 0x77  MOV M,A
// (HL) <- A
// Moves the value in A to address HL, where H is MSB.
static void execute77(State8080& state)
{
	uint16_t address = (uint16_t)(state.H << 8) | (uint16_t)state.L;
	writeByteToMemory(state, address, state.A);
}

// 0xC1  POP BC
static void executeC1(State8080& state)
{
	// stored in memory in little endian format
	HP_ASSERT(state.SP < state.memorySizeBytes - 2);
	uint8_t lsb = readByteFromMemory(state, state.SP);
	uint8_t msb = readByteFromMemory(state, state.SP + 1);

	// First register of register pair always contains the MSB
	state.B = msb;
	state.C = lsb;
	state.SP += 2;
}

// 0xC2 JNZ <address>
// if NZ, PC <- adr
static void executeC2(State8080& state)
{
	if(state.flags.Z == 0)
		state.PC = getInstructionAddress(state);
}

// 0xC3 JMP <address>
// PC <- adr
static void executeC3(State8080& state)
{
	state.PC = getInstructionAddress(state);
}

// 0xC5  PUSH BC
static void executeC5(State8080& state)
{
	// First register of register pair always contains the MSB
	// Store in memory in little-endian format.
	uint8_t msb = state.B;
	uint8_t lsb = state.C;
	HP_ASSERT(state.SP >= 2);
	writeByteToMemory(state, state.SP - 2, lsb);
	writeByteToMemory(state, state.SP - 1, msb);
	state.SP -= 2;
}

// 0xCD CALL <address>
static void executeCD(State8080& state)
{
	// push return address onto stack
	uint16_t returnAddress = state.PC; // the PC has been advanced prior to instruction execution
	state.PC = getInstructionAddress(state); //  PC = <address>

	// store return address in little-endian
	uint8_t msb = (uint8_t)(returnAddress >> 8);
	uint8_t lsb = (uint8_t)(returnAddress & 0xff);
	HP_ASSERT(state.SP >= 2);
	writeByteToMemory(state, state.SP - 2, lsb);
	writeByteToMemory(state, state.SP - 1, msb);
	state.SP -= 2;
}

// 0xC9 RET
static void executeC9(State8080& state)
{
	// return address is stored in stack memory in little-endian format
	uint8_t lsb = readByteFromMemory(state, state.SP);
	uint8_t msb = readByteFromMemory(state, state.SP + 1);
	state.SP += 2;
	uint16_t address = ((uint8_t)msb << 8) | (uint8_t)lsb;
	HP_ASSERT(address < state.memorySizeBytes);
	state.PC = address;
}

// 0xD1  POP DE
static void executeD1(State8080& state)
{
	// stored in memory in little endian format
	HP_ASSERT(state.SP < state.memorySizeBytes - 2);
	uint8_t lsb = readByteFromMemory(state, state.SP);
	uint8_t msb = readByteFromMemory(state, state.SP + 1);

	// First register of register pair always contains the MSB
	state.D = msb;
	state.E = lsb;
	state.SP += 2;
}

// 0xD3  OUT d8  aka OUT d8,A
// The contents of A are sent to output device number <d8> 
static void executeD3(State8080& /*state*/)
{

}

// 0xD5  PUSH DE
static void executeD5(State8080& state)
{
	// First register of register pair always contains the MSB
	// Store in memory in little-endian format.
	uint8_t msb = state.D;
	uint8_t lsb = state.E;
	HP_ASSERT(state.SP >= 2);
	writeByteToMemory(state, state.SP - 2, lsb);
	writeByteToMemory(state, state.SP - 1, msb);
	state.SP -= 2;
}

// 0xE1  POP HL
static void executeE1(State8080& state)
{
	// stored in memory in little endian format
	HP_ASSERT(state.SP < state.memorySizeBytes - 2);
	uint8_t lsb = readByteFromMemory(state, state.SP);
	uint8_t msb = readByteFromMemory(state, state.SP + 1);

	// First register of register pair always contains the MSB
	state.H = msb;
	state.L = lsb;
	state.SP += 2;
}

// 0xE5  PUSH HL
static void executeE5(State8080& state)
{
	// First register of register pair always contains the MSB
	// Store in memory in little-endian format.
	uint8_t msb = state.H;
	uint8_t lsb = state.L;
	HP_ASSERT(state.SP >= 2);
	writeByteToMemory(state, state.SP - 2, lsb);
	writeByteToMemory(state, state.SP - 1, msb);
	state.SP -= 2;
}

// 0xEB  XCHG  aka EX DE,HL
// The 16 bits of data held in the H and L registers are exchanged
// with the 16 bits of data held in the D and E registers.
// Condition bits affected: none
static void executeEB(State8080& state)
{
	uint8_t temp = state.H;
	state.H = state.D;
	state.D = temp;

	temp = state.L;
	state.L = state.E;
	state.E = temp;
}

// 0xF5  PUSH PSW  aka PUSH AF
static void executeF5(State8080& state)
{
	// First register of register pair always contains the MSB
	// Store in memory in little-endian format.
	uint8_t msb = state.A;
	uint8_t lsb = *(uint8_t*)&state.flags;
	HP_ASSERT(state.SP >= 2);
	writeByteToMemory(state, state.SP - 2, lsb);
	writeByteToMemory(state, state.SP - 1, msb);
	state.SP -= 2;
}

// 0xFE  CPI d8
// Compare immediate with accumulator.
// The comparison is performed by internally subtracting the data from the accumulator,
// leaving the accumulator unchanged but setting the condition bits by the result.
// Condition bits affected: Carry, Zero, Sign, Parity, Auxiliary Carry
static void executeFE(State8080& state)
{
	uint8_t d8 = getInstructionD8(state);

	// The Carry bit will be set if the immediate data is greater than A, and reset otherwise. [Data Book, p29]
	state.flags.C = (d8 > state.A) ? 1 : 0;

	uint8_t res = state.A - d8;
	state.flags.Z = calculateZeroFlag(res);
	state.flags.S = calculateSignFlag(res);
	state.flags.P = calculateParityFlag(res);
}

static const Instruction s_instructions[] =
{
	{ 0x00, "NOP", 1, execute00 },
	{ 0x01, "LXI B,%04X", 3, execute01 },
	{ 0x02, "STAX B", 1, nullptr }, //		(BC) <- A
	{ 0x03, "INX B", 1, nullptr }, //		BC < -BC + 1
	{ 0x04, "INR B", 1, nullptr }, //	Z, S, P, AC	B < -B + 1
	{ 0x05, "DCR B", 1, execute05 }, // Z, S, P, AC	B < -B - 1
	{ 0x06, "MVI B, %02X", 2, execute06 }, // B <- byte 2
	{ 0x07, "RLC",	1, nullptr }, //		CY	A = A << 1; bit 0 = prev bit 7; CY = prev bit 7
	{ 0x08, "-", 1, nullptr },
	{ 0x09, "DAD BC", 1, execute09 }, // aka ADD HL,BC   HL <- HL + BC  Sets Carry flag
	{ 0x0a, "LDAX B",	1, nullptr }, //			A < -(BC)
	{ 0x0b, "DCX B",	1, nullptr }, //			BC = BC - 1
	{ 0x0c, "INR C",	1, nullptr }, //		Z, S, P, AC	C < -C + 1
	{ 0x0d, "DCR C", 1, execute0D }, // aka DEC C	 C <- C-1	Sets: Z, S, P, AC	  
	{ 0x0e, "MVI C,%02X", 2, execute0E }, // C <- byte 2
	{ 0x0f, "RRC",	1, execute0F }, // A = A >> 1  bit 7 = prev bit 0  CY = prev bit 0  Sets Carry flag
	{ 0x10, "-", 1, nullptr }, //	
	{ 0x11, "LXI D,%04X",	3, execute11 }, // D <- byte 3, E <- byte 2
	{ 0x12, "STAX D",	1, nullptr }, //			(DE) < -A
	{ 0x13, "INX D",	1, execute13 }, //			DE < -DE + 1
	{ 0x14, "INR D",	1, nullptr }, //		Z, S, P, AC	D < -D + 1
	{ 0x15, "DCR D",	1, nullptr }, //		Z, S, P, AC	D < -D - 1
	{ 0x16, "MVI D, %02X",	2, nullptr }, //			D < -byte 2
	{ 0x17, "RAL",	1, nullptr }, //		CY	A = A << 1; bit 0 = prev CY; CY = prev bit 7
	{ 0x18, "-", 1, nullptr }, //	
	{ 0x19, "DAD D", 1, execute19 }, // aka ADD HL,DE   HL <- HL + DE   Sets Carry flag
	{ 0x1a, "LDAX D", 1, execute1A }, // A <- (DE)
	{ 0x1b, "DCX D",	1, nullptr }, //			DE = DE - 1
	{ 0x1c, "INR E",	1, nullptr }, //		Z, S, P, AC	E < -E + 1
	{ 0x1d, "DCR E",	1, nullptr }, //		Z, S, P, AC	E < -E - 1
	{ 0x1e, "MVI E,%02X",	2, nullptr }, //			E < -byte 2
	{ 0x1f, "RAR",	1, nullptr }, //		CY	A = A >> 1; bit 7 = prev bit 7; CY = prev bit 0
	{ 0x20, "-", 1, nullptr }, //	
	{ 0x21, "LXI H,%04X", 3, execute21 }, // H <- byte 3, L <-byte 2
	{ 0x22, "SHLD %04X",	3, nullptr }, //			(adr) < -L; (adr + 1) < -H
	{ 0x23, "INX H",	1, execute23 }, // HL <- HL + 1
	{ 0x24, "INR H",	1, nullptr }, //		Z, S, P, AC	H < -H + 1
	{ 0x25, "DCR H",	1, nullptr }, //		Z, S, P, AC	H < -H - 1
	{ 0x26, "MVI H,%02X", 2, execute26 }, // H <- byte 2
	{ 0x27, "DAA",	1, nullptr }, //			special
	{ 0x28, "-", 1, nullptr }, //	
	{ 0x29, "DAD HL", 1, execute29 }, // aka ADD HL,HL   HL <- HL + HL   Sets Carry flag
	{ 0x2a, "LHLD %04X",	3, nullptr }, //			L < -(adr); H < -(adr + 1)
	{ 0x2b, "DCX H", 1, nullptr }, //			HL = HL - 1
	{ 0x2c, "INR L", 1, nullptr }, //		Z, S, P, AC	L < -L + 1
	{ 0x2d, "DCR L", 1, nullptr }, //		Z, S, P, AC	L < -L - 1
	{ 0x2e, "MVI L, %02X", 2, nullptr }, //			L < -byte 2
	{ 0x2f, "CMA", 1, nullptr }, //			A < -!A
	{ 0x30, "-", 1, nullptr }, //	
	{ 0x31, "LXI SP, %04X",	3, execute31 }, // SP.hi <- byte 3, SP.lo <- byte 2
	{ 0x32, "STA %04X",	3, nullptr }, //			(adr) < -A
	{ 0x33, "INX SP",	1, nullptr }, //			SP = SP + 1
	{ 0x34, "INR M",	1, nullptr }, //		Z, S, P, AC(HL) < -(HL)+1
	{ 0x35, "DCR M",	1, nullptr }, //		Z, S, P, AC(HL) < -(HL)-1
	{ 0x36, "MVI M,%02X",	2, execute36 }, // (HL) <- byte 2
	{ 0x37, "STC",	1, nullptr }, //		CY	CY = 1
	{ 0x38, "-", 1},
	{ 0x39, "DAD SP",	1, nullptr }, //		CY	HL = HL + SP
	{ 0x3a, "LDA %04X",	3, nullptr }, //			A < -(adr)
	{ 0x3b, "DCX SP",	1, nullptr }, //			SP = SP - 1
	{ 0x3c, "INR A",	1, nullptr }, //		Z, S, P, AC	A < -A + 1
	{ 0x3d, "DCR A",	1, nullptr }, //		Z, S, P, AC	A < -A - 1
	{ 0x3e, "MVI A,%02X",	2, nullptr }, //			A < -byte 2
	{ 0x3f, "CMC",	1, nullptr }, //		CY	CY = !CY
	{ 0x40, "MOV B,B", 1, nullptr }, //			B < -B
	{ 0x41, "MOV B,C", 1, nullptr }, //			B < -C
	{ 0x42, "MOV B,D", 1, nullptr }, //			B < -D
	{ 0x43, "MOV B,E", 1, nullptr }, //			B < -E
	{ 0x44, "MOV B,H", 1, nullptr }, //			B < -H
	{ 0x45, "MOV B,L", 1, nullptr }, //			B < -L
	{ 0x46, "MOV B,M", 1, nullptr }, //			B < -(HL)
	{ 0x47, "MOV B,A", 1, nullptr }, //			B < -A
	{ 0x48, "MOV C,B", 1, nullptr }, //			C < -B
	{ 0x49, "MOV C,C", 1, nullptr }, //			C < -C
	{ 0x4a, "MOV C,D", 1, nullptr }, //			C < -D
	{ 0x4b, "MOV C,E", 1, nullptr }, //			C < -E
	{ 0x4c, "MOV C,H", 1, nullptr }, //			C < -H
	{ 0x4d, "MOV C,L", 1, nullptr }, //			C < -L
	{ 0x4e, "MOV C,M", 1, nullptr }, //			C < -(HL)
	{ 0x4f, "MOV C,A", 1, nullptr }, //			C < -A
	{ 0x50, "MOV D,B", 1, nullptr }, //			D < -B
	{ 0x51, "MOV D,C", 1, nullptr }, //			D < -C
	{ 0x52, "MOV D,D", 1, nullptr }, //			D < -D
	{ 0x53, "MOV D,E", 1, nullptr }, //			D < -E
	{ 0x54, "MOV D,H", 1, nullptr }, //			D < -H
	{ 0x55, "MOV D,L", 1, nullptr }, //			D < -L
	{ 0x56, "MOV D,M", 1, execute56 }, // D <- (HL)
	{ 0x57, "MOV D,A", 1, nullptr }, //			D < -A
	{ 0x58, "MOV E,B", 1, nullptr }, //			E < -B
	{ 0x59, "MOV E,C", 1, nullptr }, //			E < -C
	{ 0x5a, "MOV E,D", 1, nullptr }, //			E < -D
	{ 0x5b, "MOV E,E", 1, nullptr }, //			E < -E
	{ 0x5c, "MOV E,H", 1, nullptr }, //			E < -H
	{ 0x5d, "MOV E,L", 1, nullptr }, //			E < -L
	{ 0x5e, "MOV E,M", 1, execute5E }, // aka LD E,(HL)	 E < -(HL)
	{ 0x5f, "MOV E,A", 1, nullptr }, //			E < -A
	{ 0x60, "MOV H,B", 1, nullptr }, //			H < -B
	{ 0x61, "MOV H,C", 1, nullptr }, //			H < -C
	{ 0x62, "MOV H,D", 1, nullptr }, //			H < -D
	{ 0x63, "MOV H,E", 1, nullptr }, //			H < -E
	{ 0x64, "MOV H,H", 1, nullptr }, //			H < -H
	{ 0x65, "MOV H,L", 1, nullptr }, //			H < -L
	{ 0x66, "MOV H,M", 1, execute66 }, // H <- (HL)
	{ 0x67, "MOV H,A", 1, nullptr }, //			H < -A
	{ 0x68, "MOV L,B", 1, nullptr }, //			L < -B
	{ 0x69, "MOV L,C", 1, nullptr }, //			L < -C
	{ 0x6a, "MOV L,D", 1, nullptr }, //			L < -D
	{ 0x6b, "MOV L,E", 1, nullptr }, //			L < -E
	{ 0x6c, "MOV L,H", 1, nullptr }, //			L < -H
	{ 0x6d, "MOV L,L", 1, nullptr }, //			L < -L
	{ 0x6e, "MOV L,M", 1, nullptr }, //			L < -(HL)
	{ 0x6f, "MOV L,A", 1, execute6F }, // L <- A
	{ 0x70, "MOV M,B", 1, nullptr }, //			(HL) < -B
	{ 0x71, "MOV M,C", 1, nullptr }, //			(HL) < -C
	{ 0x72, "MOV M,D", 1, nullptr }, //			(HL) < -D
	{ 0x73, "MOV M,E", 1, nullptr }, //			(HL) < -E
	{ 0x74, "MOV M,H", 1, nullptr }, //			(HL) < -H
	{ 0x75, "MOV M,L", 1, nullptr }, //			(HL) < -L
	{ 0x76, "HLT",	1, nullptr }, //			special
	{ 0x77, "MOV M,A", 1, execute77 }, // (HL) < -A
	{ 0x78, "MOV A,B", 1, nullptr }, //			A < -B
	{ 0x79, "MOV A,C", 1, nullptr }, //			A < -C
	{ 0x7a, "MOV A,D", 1, execute7A }, // A <- D
	{ 0x7b, "MOV A,E", 1, nullptr }, //			A < -E
	{ 0x7c, "MOV A,H", 1, execute7C }, // A <- H
	{ 0x7d, "MOV A,L", 1, nullptr }, //			A < -L
	{ 0x7e, "MOV A,M", 1, execute7E }, // A <- (HL)
	{ 0x7f, "MOV A,A", 1, nullptr }, //			A < -A
	{ 0x80, "ADD B", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + B
	{ 0x81, "ADD C", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + C
	{ 0x82, "ADD D", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + D
	{ 0x83, "ADD E", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + E
	{ 0x84, "ADD H", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + H
	{ 0x85, "ADD L", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + L
	{ 0x86, "ADD M", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + (HL)
	{ 0x87, "ADD A", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + A
	{ 0x88, "ADC B", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + B + CY
	{ 0x89, "ADC C", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + C + CY
	{ 0x8a, "ADC D", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + D + CY
	{ 0x8b, "ADC E", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + E + CY
	{ 0x8c, "ADC H", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + H + CY
	{ 0x8d, "ADC L", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + L + CY
	{ 0x8e, "ADC M", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + (HL)+CY
	{ 0x8f, "ADC A", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + A + CY
	{ 0x90, "SUB B", 1, nullptr }, //		Z, S, P, CY, AC	A < -A - B
	{ 0x91, "SUB C", 1, nullptr }, //		Z, S, P, CY, AC	A < -A - C
	{ 0x92, "SUB D", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + D
	{ 0x93, "SUB E", 1, nullptr }, //		Z, S, P, CY, AC	A < -A - E
	{ 0x94, "SUB H", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + H
	{ 0x95, "SUB L", 1, nullptr }, //		Z, S, P, CY, AC	A < -A - L
	{ 0x96, "SUB M", 1, nullptr }, //		Z, S, P, CY, AC	A < -A + (HL)
	{ 0x97, "SUB A", 1, nullptr }, //		Z, S, P, CY, AC	A < -A - A
	{ 0x98, "SBB B", 1, nullptr }, //		Z, S, P, CY, AC	A < -A - B - CY
	{ 0x99, "SBB C", 1, nullptr }, //		Z, S, P, CY, AC	A < -A - C - CY
	{ 0x9a, "SBB D", 1, nullptr }, //		Z, S, P, CY, AC	A < -A - D - CY
	{ 0x9b, "SBB E", 1, nullptr }, //		Z, S, P, CY, AC	A < -A - E - CY
	{ 0x9c, "SBB H", 1, nullptr }, //		Z, S, P, CY, AC	A < -A - H - CY
	{ 0x9d, "SBB L", 1, nullptr }, //		Z, S, P, CY, AC	A < -A - L - CY
	{ 0x9e, "SBB M", 1, nullptr }, //		Z, S, P, CY, AC	A < -A - (HL)-CY
	{ 0x9f, "SBB A", 1, nullptr }, //		Z, S, P, CY, AC	A < -A - A - CY
	{ 0xa0, "ANA B", 1, nullptr }, //		Z, S, P, CY, AC	A < -A & B
	{ 0xa1, "ANA C", 1, nullptr }, //		Z, S, P, CY, AC	A < -A & C
	{ 0xa2, "ANA D", 1, nullptr }, //		Z, S, P, CY, AC	A < -A & D
	{ 0xa3, "ANA E", 1, nullptr }, //		Z, S, P, CY, AC	A < -A & E
	{ 0xa4, "ANA H", 1, nullptr }, //		Z, S, P, CY, AC	A < -A & H
	{ 0xa5, "ANA L", 1, nullptr }, //		Z, S, P, CY, AC	A < -A & L
	{ 0xa6, "ANA M", 1, nullptr }, //		Z, S, P, CY, AC	A < -A & (HL)
	{ 0xa7, "ANA A", 1, nullptr }, //		Z, S, P, CY, AC	A < -A & A
	{ 0xa8, "XRA B", 1, nullptr }, //		Z, S, P, CY, AC	A < -A ^ B
	{ 0xa9, "XRA C", 1, nullptr }, //		Z, S, P, CY, AC	A < -A ^ C
	{ 0xaa, "XRA D", 1, nullptr }, //		Z, S, P, CY, AC	A < -A ^ D
	{ 0xab, "XRA E", 1, nullptr }, //		Z, S, P, CY, AC	A < -A ^ E
	{ 0xac, "XRA H", 1, nullptr }, //		Z, S, P, CY, AC	A < -A ^ H
	{ 0xad, "XRA L", 1, nullptr }, //		Z, S, P, CY, AC	A < -A ^ L
	{ 0xae, "XRA M", 1, nullptr }, //		Z, S, P, CY, AC	A < -A ^ (HL)
	{ 0xaf, "XRA A", 1, nullptr }, //		Z, S, P, CY, AC	A < -A ^ A
	{ 0xb0, "ORA B", 1, nullptr }, //		Z, S, P, CY, AC	A < -A | B
	{ 0xb1, "ORA C", 1, nullptr }, //		Z, S, P, CY, AC	A < -A | C
	{ 0xb2, "ORA D", 1, nullptr }, //		Z, S, P, CY, AC	A < -A | D
	{ 0xb3, "ORA E", 1, nullptr }, //		Z, S, P, CY, AC	A < -A | E
	{ 0xb4, "ORA H", 1, nullptr }, //		Z, S, P, CY, AC	A < -A | H
	{ 0xb5, "ORA L", 1, nullptr }, //		Z, S, P, CY, AC	A < -A | L
	{ 0xb6, "ORA M", 1, nullptr }, //		Z, S, P, CY, AC	A < -A | (HL)
	{ 0xb7, "ORA A", 1, nullptr }, //		Z, S, P, CY, AC	A < -A | A
	{ 0xb8, "CMP B", 1, nullptr }, //		Z, S, P, CY, AC	A - B
	{ 0xb9, "CMP C", 1, nullptr }, //		Z, S, P, CY, AC	A - C
	{ 0xba, "CMP D", 1, nullptr }, //		Z, S, P, CY, AC	A - D
	{ 0xbb, "CMP E", 1, nullptr }, //		Z, S, P, CY, AC	A - E
	{ 0xbc, "CMP H", 1, nullptr }, //		Z, S, P, CY, AC	A - H
	{ 0xbd, "CMP L", 1, nullptr }, //		Z, S, P, CY, AC	A - L
	{ 0xbe, "CMP M", 1, nullptr }, //		Z, S, P, CY, AC	A - (HL)
	{ 0xbf, "CMP A", 1, nullptr }, //		Z, S, P, CY, AC	A - A
	{ 0xc0, "RNZ", 1, nullptr }, //			if NZ, RET
	{ 0xc1, "POP BC", 1, executeC1 }, // C < -(sp); B < -(sp + 1); sp < -sp + 2
	{ 0xc2, "JNZ %04X", 3, executeC2 }, // if NZ, PC <- adr
	{ 0xc3, "JMP %04X", 3, executeC3 }, //			PC <= adr
	{ 0xc4, "CNZ %04X", 3, nullptr }, //			if NZ, CALL adr
	{ 0xc5, "PUSH BC", 1, executeC5 }, // (sp - 2) < -C; (sp - 1) < -B; sp < -sp - 2
	{ 0xc6, "ADI %02X", 2, nullptr }, //		Z, S, P, CY, AC	A < -A + byte
	{ 0xc7, "RST 0",	1, nullptr }, //			CALL $0
	{ 0xc8, "RZ	1",	1, nullptr }, //		if Z, RET
	{ 0xc9, "RET",	1, executeC9 }, // PC.lo <- (sp); PC.hi <- (sp + 1); SP <- SP + 2
	{ 0xca, "JZ %04X", 3, nullptr }, //			if Z, PC < -adr
	{ 0xcb, "-", 1, nullptr }, //	
	{ 0xcc, "CZ %04X",	3, nullptr }, //			if Z, CALL adr
	{ 0xcd, "CALL %04X", 3, executeCD }, // (SP - 1) <- PC.hi; (SP - 2) <- PC.lo; SP <- SP - 2; PC = adr
	{ 0xce, "ACI %02X",	2, nullptr }, //		Z, S, P, CY, AC	A < -A + data + CY
	{ 0xcf, "RST 1", 1, nullptr }, //			CALL $8
	{ 0xd0, "RNC",	1, nullptr }, //			if NCY, RET
	{ 0xd1, "POP DE", 1, executeD1 }, // E < -(sp); D < -(sp + 1); sp < -sp + 2
	{ 0xd2, "JNC %04X", 3, nullptr }, //			if NCY, PC < -adr
	{ 0xd3, "OUT %02X",	2, executeD3 }, // special
	{ 0xd4, "CNC %04X",	3, nullptr }, //			if NCY, CALL adr
	{ 0xd5, "PUSH DE",	1, executeD5 }, // (sp - 2) <- E; (sp - 1) <- D; sp <- sp-2
	{ 0xd6, "SUI %02X", 2, nullptr }, //		Z, S, P, CY, AC	A < -A - data
	{ 0xd7, "RST 2",	1, nullptr }, //			CALL $10
	{ 0xd8, "RC", 1, nullptr }, //			if CY, RET
	{ 0xd9, "-", 1, nullptr }, //	
	{ 0xda, "JC %04X",	3, nullptr }, //			if CY, PC < -adr
	{ 0xdb, "IN %02X",	2, nullptr }, //			special
	{ 0xdc, "CC %04X",	3, nullptr }, //			if CY, CALL adr
	{ 0xdd, "-", 1, nullptr }, //	
	{ 0xde, "SBI %02X",	2, nullptr }, //		Z, S, P, CY, AC	A < -A - data - CY
	{ 0xdf, "RST 3",	1, nullptr }, //			CALL $18
	{ 0xe0, "RPO",	1, nullptr }, //			if PO, RET
	{ 0xe1, "POP HL", 1, executeE1 }, // L <- (sp); H <- (sp+1); sp <- sp+2
	{ 0xe2, "JPO %04X", 3, nullptr }, //			if PO, PC < -adr
	{ 0xe3, "XTHL",	1, nullptr }, //			L < -> (SP); H < -> (SP + 1)
	{ 0xe4, "CPO %04X", 3, nullptr }, //			if PO, CALL adr
	{ 0xe5, "PUSH HL",	1, executeE5 }, // (sp-2) <-L ; (sp-1) <- H; sp <- sp-2
	{ 0xe6, "ANI %02X", 2, nullptr }, //		Z, S, P, CY, AC	A < -A & data
	{ 0xe7, "RST 4",	1, nullptr }, //			CALL $20
	{ 0xe8, "RPE",	1, nullptr }, //			if PE, RET
	{ 0xe9, "PCHL",	1, nullptr }, //			PC.hi < -H; PC.lo < -L
	{ 0xea, "JPE %04X",	3, nullptr }, //			if PE, PC < -adr
	{ 0xeb, "XCHG",	1, executeEB }, // H <-> D; L <-> E
	{ 0xec, "CPE %04X", 3, nullptr }, //			if PE, CALL adr
	{ 0xed, "-", 1, nullptr }, //	
	{ 0xee, "XRI %02X",	2, nullptr }, //		Z, S, P, CY, AC	A < -A ^ data
	{ 0xef, "RST 5",	1, nullptr }, //			CALL $28
	{ 0xf0, "RP", 1, nullptr }, //			if P, RET
	{ 0xf1, "POP PSW",	1, nullptr }, //			flags < -(sp); A < -(sp + 1); sp < -sp + 2
	{ 0xf2, "JP %04X", 3, nullptr }, //			if P = 1 PC < -adr
	{ 0xf3, "DI",	1, nullptr }, //			special
	{ 0xf4, "CP %04X",	3, nullptr }, //			if P, PC < -adr
	{ 0xf5, "PUSH PSW",	1, executeF5 }, // aka PUSH AF  (sp - 2) < -flags; (sp - 1) < -A; sp < -sp - 2
	{ 0xf6, "ORI %02X", 2, nullptr }, //		Z, S, P, CY, AC	A < -A | data
	{ 0xf7, "RST 6",	1, nullptr }, //			CALL $30
	{ 0xf8, "RM",	1, nullptr }, //			if M, RET
	{ 0xf9, "SPHL",	1, nullptr }, //			SP = HL
	{ 0xfa, "JM %04X",	3, nullptr }, //			if M, PC < -adr
	{ 0xfb, "EI",	1, nullptr }, //			special
	{ 0xfc, "CM %04X",	3, nullptr }, //			if M, CALL adr
	{ 0xfd, "-", 1, nullptr }, //	
	{ 0xfe, "CPI %02X",	2, executeFE }, // Compare immediate with accumulator  Z, S, P, CY, AC    A - data
	{ 0xff, "RST 7",	1, nullptr }, //			CALL $38
};
static_assert(COUNTOF_ARRAY(s_instructions) == 256, "Array size incorrect");

// returns instruction size in bytes
unsigned int Disassemble8080(const uint8_t* buffer, const size_t bufferSize, unsigned int pc)
{
	HP_ASSERT(buffer);
	HP_ASSERT(pc < bufferSize);

	const uint8_t* pPC = &buffer[pc];
	const uint8_t opcode = *pPC;
	const Instruction& instruction = s_instructions[opcode];
	HP_ASSERT(instruction.opcode == opcode);
	HP_ASSERT(instruction.sizeBytes > 0 && instruction.sizeBytes <= kMaxInstructionSizeBytes);

	// address
	printf("0x%04X  ", pc);

	// hex
	for(unsigned int byteIndex = 0; byteIndex < instruction.sizeBytes; byteIndex++)
		printf("%02X ", *(pPC + byteIndex));
	for(unsigned int byteIndex = instruction.sizeBytes; byteIndex < kMaxInstructionSizeBytes; byteIndex++)
		printf("   ");

	// mnemonic
	char text[64];
	if(instruction.sizeBytes == 1)
		sprintf(text, "%s", instruction.mnemonic);
	else if(instruction.sizeBytes == 2)
	{
		uint8_t d8 = *(pPC + 1);
		sprintf(text, instruction.mnemonic, d8);
	}
	else if(instruction.sizeBytes == 3)
	{
		uint8_t lsb = *(pPC + 1);
		uint8_t msb = *(pPC + 2);
		uint16_t val = (msb << 8) | lsb;
		sprintf(text, instruction.mnemonic, val);
	}

	printf(" %-14s", text);
	return instruction.sizeBytes;
}

void Emulate8080Instruction(State8080& state)
{
	const uint8_t opcode = readByteFromMemory(state, state.PC);
	const Instruction& instruction = s_instructions[opcode];
	HP_ASSERT(instruction.opcode == opcode);
	HP_ASSERT(instruction.sizeBytes >= kMinInstructionSizeBytes && instruction.sizeBytes <= kMaxInstructionSizeBytes);
	HP_ASSERT(instruction.execute, "Unimplemented instruction 0x%02X at PC=%04X", instruction.opcode, state.PC);

	// "Just before each instruction is executed, the Program Counter is advanced to the
	// next sequential instruction." - Data Book, p2.
	// #TODO: May want to store PC of currently executing instruction for error reporting e.g. invalid memory access
	state.PC += instruction.sizeBytes;

	instruction.execute(state);
}

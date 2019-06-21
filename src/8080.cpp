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

// Called from the various Return instructions: RZ, RNZ etc..
static void performReturnOperation(State8080& state)
{
	// return address is stored in stack memory in little-endian format
	uint8_t lsb = readByteFromMemory(state, state.SP);
	uint8_t msb = readByteFromMemory(state, state.SP + 1);
	state.SP += 2;
	uint16_t address = ((uint8_t)msb << 8) | (uint8_t)lsb;
	HP_ASSERT(address < state.memorySizeBytes);
	state.PC = address;
}

//-----------------------------------------------------------------------------

// 0x00 NOP
static void execute00(State8080& /*state*/)
{

}

//-----------------------------------------------------------------------------
// STAX
//
// Store Accumulator
//
// The contents of the accumulator are stored in the memory location addressed 
// by registers B and C, or by registers D and E.
//
// Condition bits affected : None

// 0x02  STAX B  aka LD (BC),A
// (BC) <- A
static void execute02(State8080& state)
{
	uint16_t BC = ((uint16_t)state.B << 8) | (state.C);
	writeByteToMemory(state, BC, state.A);
}

// 0x12  STAX D  aka LD (DE),A
// (DE) <- A
static void execute12(State8080& state)
{
	uint16_t DE = ((uint16_t)state.D << 8) | (state.E);
	writeByteToMemory(state, DE, state.A);
}

//-----------------------------------------------------------------------------
// INR
//
// Increment Register or Memory
//
// The specified register or memory byte is incremented by one.
// 
// Condition bits affected : Zero, Sign, Parity, Auxiliary Carry
// n.b. Does not affect Carry

static void executeINR(State8080& state, uint8_t& regOrMemoryByte)
{
	uint8_t result = regOrMemoryByte + 1;

	state.flags.Z = calculateZeroFlag(result);
	state.flags.S = calculateSignFlag(result);
	state.flags.P = calculateParityFlag(result);
	state.flags.AC = ((regOrMemoryByte & 0xf) + 1) > 0xf;

	regOrMemoryByte = result;
}

// 0x04  INR B  aka INC B
static void execute04(State8080& state)
{
	executeINR(state, state.B);
}

// 0x0C  INR C  aka INC C
static void execute0C(State8080& state)
{
	executeINR(state, state.C);
}

// 0x14  INR D  aka INC D
static void execute14(State8080& state)
{
	executeINR(state, state.D);
}

// 0x1C  INR E  aka INC E
static void execute1C(State8080& state)
{
	executeINR(state, state.E);
}

// 0x24  INR H  aka INC H
static void execute24(State8080& state)
{
	executeINR(state, state.H);
}

// 0x2C  INR L  aka INC L
static void execute2C(State8080& state)
{
	executeINR(state, state.L);
}

// 0x34  INR M  aka INC (HL)
// (HL) <- (HL) + 1
static void execute34(State8080& state)
{
	uint16_t address = ((uint16_t)state.H << 8) | (state.L);
	uint8_t val = readByteFromMemory(state, address);
	executeINR(state, val);
	writeByteToMemory(state, address, val);
}

// 0x3C  INR A  aka INC A
static void execute3C(State8080& state)
{
	executeINR(state, state.A);
}

//-----------------------------------------------------------------------------
// DCR
//
// Decrement Register or Memory
//
// The specified register or memory byte is decremented by one.
//
// Condition bits affected: Zero, Sign, Parity, Auxiliary Carry

static void executeDCR(State8080& state, uint8_t& val)
{
	uint8_t result = val - 1;

	state.flags.Z = calculateZeroFlag(result);
	state.flags.S = calculateSignFlag(result);
	state.flags.P = calculateParityFlag(result);

	// The Data Book description of how this instruction affects the Carry flag is a little convoluted,
	// this is better: https://retrocomputing.stackexchange.com/questions/5953/carry-flag-in-8080-8085-subtraction
	// "The 8080 sets the carry flag when the unsigned value subtracted is greater than the unsigned value it is subtracted from."
	// We are subtracting 1, so the we will only borrow when the lower 4 bits of val are zero
	state.flags.AC = ((val & 0x0f) == 0) ? 1 : 0;

	val = result;
}

// 0x05 DCR B  aka DEC B
static void execute05(State8080& state)
{
	executeDCR(state, state.B);
}

// 0x0D  DCR C  aka DEC C
static void execute0D(State8080& state)
{
	executeDCR(state, state.C);
}

// 0x15  DCR D  aka DEC D
static void execute15(State8080& state)
{
	executeDCR(state, state.D);
}

// 0x1D  DCR E  aka DEC E
static void execute1D(State8080& state)
{
	executeDCR(state, state.E);
}

// 0x25  DCR H  aka DEC H
static void execute25(State8080& state)
{
	executeDCR(state, state.H);
}

// 0x2D  DCR L  aka DEC L
static void execute2D(State8080& state)
{
	executeDCR(state, state.L);
}

// 0x35  DCR M  aka DEC (HL)
static void execute35(State8080& state)
{
	uint16_t address = ((uint16_t)state.H << 8) | (state.L);
	uint8_t val = readByteFromMemory(state, address);
	executeDCR(state, val);
	writeByteToMemory(state, address, val);
}

// 0x3D  DCR A  aka DEC A
static void execute3D(State8080& state)
{
	executeDCR(state, state.A);
}

//-----------------------------------------------------------------------------
// CALL

// Called from the various Call instructions: CALL, CZ, CNZ, etc

static void executeCALL(State8080& state)
{
	// Push return address onto stack, stored in little endian
	uint16_t returnAddress = state.PC;  // the PC has been advanced prior to instruction execution
	uint8_t msb = (uint8_t)(returnAddress >> 8);
	uint8_t lsb = (uint8_t)(returnAddress & 0xff);
	HP_ASSERT(state.SP >= 2);
	writeByteToMemory(state, state.SP - 2, lsb);
	writeByteToMemory(state, state.SP - 1, msb);
	state.SP -= 2;

	state.PC = getInstructionAddress(state); //  PC = <address>
}

// 0xCD CALL <address>
static void executeCD(State8080& state)
{
	executeCALL(state);
}

// 0xDC  CC <addr>
// Call If Carry
static void executeDC(State8080& state)
{
	if(state.flags.C == 1)
		executeCALL(state);
}

// 0xD4  CNC <addr>  aka CALL NC,<address>
// Call If No Carry
static void executeD4(State8080& state)
{
	if(state.flags.C == 0)
		executeCALL(state);
}

// 0xCC  CZ <adr>
// Call If Zero
// If the Zero bit is zero, a call operation is performed to subroutine sub.
// *** Surely if the Zero bit is *1* !!
static void executeCC(State8080& state)
{
	if(state.flags.Z == 1)
		executeCALL(state);
}

// 0xC4  CNZ <address>
// Call If Not Zero
// If the Zero bit is one, a call operation is performed to the subroutine at the specified address.
// *** Surely if the Zero bit is *0* !!
static void executeC4(State8080& state)
{
	if(state.flags.Z == 0)
		executeCALL(state);
}

// 0xFC  CM
// Call If Minus
// If the Sign bit is one (indicating a minus result), a call operation is performed to subroutine sub.
static void executeFC(State8080& state)
{
	if(state.flags.S == 1)
		executeCALL(state);
}

// 0xF4 CP
// Call If Plus
// If the Sign bit is zero (indicating a positive result), a call operation is performed to subroutine sub.
static void executeF4(State8080& state)
{
	if(state.flags.S == 0)
		executeCALL(state);
}

// 0xEC  CPE
// Call If Parity Even
// If the Parity bit is one (indicating even parity), a call operation is performed to subroutine sub.
static void executeEC(State8080& state)
{
	if(state.flags.P == 1)
		executeCALL(state);
}

// 0xE4  CPO
// Call If Parity Odd
// If the Parity bit is zero (indicating odd parity), a call operation is performed to subroutine sub.
static void executeE4(State8080& state)
{
	if(state.flags.P == 0)
		executeCALL(state);
}

//-----------------------------------------------------------------------------

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

// 0x03  INX B  aka INC BC
// Increment Register Pair BC
// BC <- BC+1
// Condition bits affected: None
static void execute03(State8080& state)
{
	uint16_t BC = ((uint16_t)state.B << 8) | (state.C);
	BC++;
	state.B = (uint8_t)(BC >> 8);
	state.C = (uint8_t)(BC & 0xff);
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

// 0x0A  LDAX B  aka LD A,(BC)
// A <- (BC)
static void execute0A(State8080& state)
{
	// MSB is always in the first register of the pair
	uint16_t address = (uint16_t)(state.B << 8) | (uint16_t)state.C;
	state.A = readByteFromMemory(state, address);
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

// 0x07  RLC  aka RLCA
// Rotate Accumulator left
// A = A << 1; bit 0 = prev bit 7; CY = prev bit 7
// Condition bits affected: Carry
static void execute07(State8080& state)
{
	state.flags.C = (state.A & 0x80) ? 1 : 0;
	state.A = state.A << 1;
	state.A |= state.flags.C;
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

// 0x16  MVI D,<d8>
// D <- byte 2
static void execute16(State8080& state)
{
	uint8_t d8 = getInstructionD8(state);
	state.D = d8;
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

// 0x1F  RAR  aka RRA
// Rotate Accumulator Right Through Carry
// The contents of the accumulator are rotated one bit position to the right.
// The low-order bit of the accumulator replaces the carry bit, while the 
// carry bit replaces the high - order bit of the accumulator.
// Condition bits affected : Carry
// A = A >> 1; bit 7 = prev bit 7; CY = prev bit 0
static void execute1F(State8080& state)
{
	uint8_t prevCarry = state.flags.C;
	state.flags.C = state.A & 1;
	state.A = state.A >> 1;
	state.A |= prevCarry << 7;
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

// 0x22  SHLD <address>  aka LD (adr),HL
// Store H and L Direct
// Condition bits affected : None
// (adr) <- L; (adr+1) <- H
static void execute22(State8080& state)
{
	uint16_t address = getInstructionAddress(state);
	writeByteToMemory(state, address, state.L); // LSB
	writeByteToMemory(state, address + 1, state.H); // MSB
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

//------------------------------------------------------------------------------
// 0x27  DAA
//
// Decimal Adiust Accumulator
//
// The eight-bit hexadecimal number in the accumulator is adjusted to form two
// four-bit binary-coded-decimal (BCD) digits by the following process:
//
// 1. If the least significant four bits of the accumulator represents a number 
//    greater than 9, or if the Auxiliary Carry bit is equal to one, the accumulator 
//    is incremented by six. Otherwise, no incrementing occurs.
//
// 2. If the most significant four bits of the accumulator now represent a number 
//    greater than 9, or if the normal carry bit is equal to one, the most
//    significant four bits of the accumulator are incremented by six. Otherwise,
//    no incrementing occurs.
//
// If a carry out of the least significant four bits occurs during Step(1), the 
// Auxiliary Carry bit is set; otherwise it is reset. Likewise, if a carry out of 
// the most significant four bits occurs during Step (2) the normal Carry bit is 
// set otherwise, it is unaffected.
//
// Condition bits affected: Zero, Sign, Parity, Carry, Auxiliary Carry

static void execute27(State8080& state)
{
	uint16_t result16 = (uint16_t)state.A; // 16 bit arithmetic so can detect carry easily

	bool setAC = false;
	bool setC = false;
	uint8_t lowerNibble = (result16 & 0xf);
	if(lowerNibble > 9 || state.flags.AC == 1)
	{
		// add 6 to the lower nibble for ease setting the AC flag
		lowerNibble += 6; // 16 - 10 - 6
		setAC = (lowerNibble > 0xf);

		// now add 6 to the actual result
		result16 += 6;

		uint8_t upperNibble = (result16 & 0xf0) >> 4;
		if(upperNibble > 9 || state.flags.C == 1)
		{
			result16 += 0x60;
			setC = (result16 > 0xff);
		}
	}

	state.flags.AC = setAC ? 1 : 0;
	state.flags.C = setC ? 1 : 0;
	state.A = (uint8_t)result16;
}

//------------------------------------------------------------------------------
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

// 0x2A  LHLD <address>   aka LD HL,(addr)
// Load H and L Direct
// The byte at the memory address replaces the contents
// of the L register. The byte at the next higher memory
// address replaces the contents of the H register.
// L <- (adr)       LSB
// H <- (adr + 1)   MSB
static void execute2A(State8080& state)
{
	uint16_t address = getInstructionAddress(state);
	state.L = readByteFromMemory(state, address);
	state.H = readByteFromMemory(state, address + 1);
}

//------------------------------------------------------------------------------
// DCX
//
// Decrement Register Pair
//
// The 16-bit number held in the specified register pair is decremented by one.
//
// Condition bits affected: None

// 0x0B  DCX B  aka DEC BC
// BC <- BC - 1
static void execute0B(State8080& state)
{
	uint16_t BC = ((uint16_t)state.B << 8) | (state.C);
	uint16_t result = BC - 1;
	state.B = (uint8_t)(result >> 8);   // MSB
	state.C = (uint8_t)(result & 0xff); // LSB
}

// 0x1B  DCX D  aka DEC DE
// DE <- DE - 1
static void execute1B(State8080& state)
{
	uint16_t DE = ((uint16_t)state.D << 8) | (state.E);
	uint16_t result = DE - 1;
	state.D = (uint8_t)(result >> 8);   // MSB
	state.E = (uint8_t)(result & 0xff); // LSB
}

// 0x2B  DCX H  aka DEC HL
// HL <- HL-1
static void execute2B(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | (state.L);
	uint16_t result = HL - 1;
	state.H = (uint8_t)(result >> 8);   // MSB
	state.L = (uint8_t)(result & 0xff); // LSB
}

// 0x3B  DCX SP   aka DEC SP
// SP <- SP - 1
static void execute3B(State8080& state)
{
	state.SP--;
}

//------------------------------------------------------------------------------
// 0x2E  MVI L,<d8>,
// L <- <d8>
static void execute2E(State8080& state)
{
	uint8_t d8 = getInstructionD8(state);
	state.L = d8;
}

// 0x2F  CMA   aka CPL
// Complement Accumulator
// Each bit of the contents of the accumulator
// is complemented (producing the one's complement).
// A <- !A
static void execute2F(State8080& state)
{
	state.A = ~state.A;
}

// 0x31  LXI SP,d32
static void execute31(State8080& state)
{
	uint16_t address = getInstructionAddress(state);
	// #TODO: Assert that address is in RAM
	state.SP = address;
}

// 0x32  STA <address>   aka LD (adress),A
// Store Accumulator Direct
// (adr) <- A
static void execute32(State8080& state)
{
	uint16_t address = getInstructionAddress(state);
	writeByteToMemory(state, address, state.A);
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

// 0x37  STC
// Set Carry
// The Carry bit is set to 1
static void execute37(State8080& state)
{
	state.flags.C = 1;
}

// 0x3A  LDA <addr>
// Load Accumulator Direct
// The byte at the specified address replaces the accumulator
// A <- (adr)
static void execute3A(State8080& state)
{
	uint16_t address = getInstructionAddress(state);
	state.A = readByteFromMemory(state, address);
}

// 0x3E  MVI A,<d8>
// A <- <d8>
static void execute3E(State8080& state)
{
	uint8_t d8 = getInstructionD8(state);
	state.A = d8;
}

//------------------------------------------------------------------------------
// MOV
//
// One byte of data is moved from the register specified by src(the source 
// register) to the register specified by dst(the destination register). The 
// data replaces the contents of the destination register; the source remains 
// unchanged.
//
// Condition bits affected: None
//
// Format: MOV D,S   01DDDSSS  where Dest and Source reg fields :
//                             111 = A(Accumulator)
//                             000 = B
//                             001 = C
//                             010 = D
//                             011 = E
//                             100 = H
//                             101 = L
//                             110 = M (Memory reference through address in HL)

// 0x40  MOV B,B
static void execute40(State8080& /*state*/)
{
	//state.B = state.B;
}

// 0x41  MOV B,C
static void execute41(State8080& state)
{
	state.B = state.C;
}

// 0x42  MOV B,D
static void execute42(State8080& state)
{
	state.B = state.D;
}

// 0x43  MOV B,E
static void execute43(State8080& state)
{
	state.B = state.E;
}

// 0x44  MOV B,H
static void execute44(State8080& state)
{
	state.B = state.H;
}

// 0x45  MOV B,L
static void execute45(State8080& state)
{
	state.B = state.L;
}

// 0x46  MOV B,M  aka MOV B,(HL)
// B <- (HL)
static void execute46(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | (state.L);
	state.B = readByteFromMemory(state, HL);
}

// 0x47 MOV B,A
// B <- A
static void execute47(State8080& state)
{
	state.B = state.A;
}

// 0x48  MOV C,B
static void execute48(State8080& state)
{
	state.C = state.B;
}

// 0x49  MOV C,C
static void execute49(State8080& /*state*/)
{

}

// 0x4A  MOV C,D
static void execute4A(State8080& state)
{
	state.C = state.D;
}

// 0x4B  MOV C,E
static void execute4B(State8080& state)
{
	state.C = state.E;
}

// 0x4C  MOV C,H
static void execute4C(State8080& state)
{
	state.C = state.H;
}

// 0x4D  MOV C,L
static void execute4D(State8080& state)
{
	state.C = state.L;
}

// 0x4E  MOV C,M
// C <- (HL)
static void execute4E(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | (state.L);
	state.C = readByteFromMemory(state, HL);
}

// 0x4F  MOV C,A
// C <- A
static void execute4F(State8080& state)
{
	state.C = state.A;
}

// 0x50  MOV D,B
static void execute50(State8080& state)
{
	state.D = state.B;
}

// 0x51  MOV D,C
static void execute51(State8080& state)
{
	state.D = state.C;
}

// 0x52  MOV D,D
static void execute52(State8080& /*state*/)
{

}

// 0x53  MOV D,E
static void execute53(State8080& state)
{
	state.D = state.E;
}

// 0x54  MOV D,H
static void execute54(State8080& state)
{
	state.D = state.H;
}

// 0x55  MOV D,L
static void execute55(State8080& state)
{
	state.D = state.L;
}

// 0x56  MOV D,M
// D <- (HL)
static void execute56(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | state.L;
	uint8_t val = readByteFromMemory(state, HL);
	state.D = val;
}

// 0x57  MOV D,A
// D <- A
static void execute57(State8080& state)
{
	state.D = state.A;
}

// 0x58  MOV E,B
static void execute58(State8080& state)
{
	state.E = state.B;
}

// 0x59  MOV E,C
static void execute59(State8080& state)
{
	state.E = state.C;
}

// 0x5A  MOV E,D
static void execute5A(State8080& state)
{
	state.E = state.D;
}

// 0x5B  MOV E,E
static void execute5B(State8080& /*state*/)
{
	
}

// 0x5C  MOV E,H
static void execute5C(State8080& state)
{
	state.E = state.H;
}

// 0x5D  MOV E,L
static void execute5D(State8080& state)
{
	state.E = state.L;
}

// 0x5E  MOV E,M  aka LD E,(HL)
// E < -(HL)
static void execute5E(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | state.L;
	uint8_t val = readByteFromMemory(state, HL);
	state.E = val;
}

// 0x5F  MOV E,A
// E <- A
static void execute5F(State8080& state)
{
	state.E = state.A;
}

// 0x60  MOV H,B
static void execute60(State8080& state)
{
	state.H = state.B;
}

// 0x61  MOV H,C
// H <- C
static void execute61(State8080& state)
{
	state.H = state.C;
}

// 0x62  MOV H,D
static void execute62(State8080& state)
{
	state.H = state.D;
}

// 0x63  MOV H,E
static void execute63(State8080& state)
{
	state.H = state.E;
}

// 0x64  MOV H,H
static void execute64(State8080& /*state*/)
{

}

// 0x65  MOV H,L
static void execute65(State8080& state)
{
	state.H = state.L;
}

// 0x66  MOV H,M
// H <- (HL)
static void execute66(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | state.L;
	uint8_t val = readByteFromMemory(state, HL);
	state.H = val;
}

// 0x67  MOV H,A
// H <- A
static void execute67(State8080& state)
{
	state.H = state.A;
}

// 0x68  MOV L,B
// L <- B
static void execute68(State8080& state)
{
	state.L = state.B;
}

// 0x69  MOV L,C
// L <- C
static void execute69(State8080& state)
{
	state.L = state.C;
}

// 0x6A  MOV L,D
static void execute6A(State8080& state)
{
	state.L = state.D;
}

// 0x6B  MOV L,E
static void execute6B(State8080& state)
{
	state.L = state.E;
}

// 0x6C  MOV L,H
static void execute6C(State8080& state)
{
	state.L = state.H;
}

// 0x6D  MOV L,L
static void execute6D(State8080& /*state*/)
{

}

// 0x6E  MOV L,M
static void execute6E(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | state.L;
	uint8_t val = readByteFromMemory(state, HL);
	state.L = val;
}

// 0x6F  MOV L,A
// L <- A
static void execute6F(State8080& state)
{
	state.L = state.A;
}

// 0x70  MOV M,B
// (HL) <- B
static void execute70(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | (state.L);
	writeByteToMemory(state, HL, state.B);
}

// 0x71  MOV M,C
static void execute71(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | (state.L);
	writeByteToMemory(state, HL, state.C);
}

// 0x72  MOV M,D
static void execute72(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | (state.L);
	writeByteToMemory(state, HL, state.D);
}

// 0x73  MOV M,E
static void execute73(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | (state.L);
	writeByteToMemory(state, HL, state.E);
}

// 0x74  MOV M,H
static void execute74(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | (state.L);
	writeByteToMemory(state, HL, state.H);
}

// 0x75  MOV M,L
static void execute75(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | (state.L);
	writeByteToMemory(state, HL, state.L);
}

// 0x76  HALT
static void execute76(State8080& /*state*/)
{
	HP_FATAL_ERROR("HALT");
}

// 0x77  MOV M,A
// (HL) <- A
// Moves the value in A to address HL, where H is MSB.
static void execute77(State8080& state)
{
	uint16_t address = (uint16_t)(state.H << 8) | (uint16_t)state.L;
	writeByteToMemory(state, address, state.A);
}

// 0x78  MOV A,B
// A <- B
static void execute78(State8080& state)
{
	state.A = state.B;
}

// 0x79  MOV A,C
// A <- C
static void execute79(State8080& state)
{
	state.A = state.C;
}

// 0x7A  MOV A,D
// A <- D
static void execute7A(State8080& state)
{
	state.A = state.D;
}

// 0x7B  MOV A,E
// A <- E
static void execute7B(State8080& state)
{
	state.A = state.E;
}

// 0x7C  MOV A,H
// A <- H
static void execute7C(State8080& state)
{
	state.A = state.H;
}

// 0x7D  MOV A,L
// A <- L
static void execute7D(State8080& state)
{
	state.A = state.L;
}

// 0x7E  MOV A,M
// A <- (HL)
static void execute7E(State8080& state)
{
	uint16_t HL = (uint16_t)(state.H << 8) | (uint16_t)state.L;
	state.A = readByteFromMemory(state, HL);
}

// 0x7F  MOV A,A
static void execute7F(State8080& /*state*/)
{
	
}

//------------------------------------------------------------------------------
// ADD
//
// ADD Register or Memory To Accumulator
//
// The specified byte is added to the con tents of the accumulator using two's 
// complement arithmetic.
//
// Condition bits affected : Carry, Sign, Zero, Parity, Auxiliary Carry

static void executeADD(State8080& state, uint8_t val)
{
	uint16_t result16 = (uint16_t)state.A + (uint16_t)val; // 16 bit arithmetic so can test for carry
	uint8_t result8 = (uint8_t)result16;

	state.flags.C = result16 > 0xff;
	state.flags.Z = calculateZeroFlag(result8);
	state.flags.S = calculateSignFlag(result8);
	state.flags.P = calculateParityFlag(result8);
	state.flags.AC = ((state.A & 0xf) + (val & 0xf)) > 0xf ? 1 : 0;

	state.A = result8;
}

// 0x80  ADD B  aka ADD A,B
// A <- A + B
static void execute80(State8080& state)
{
	executeADD(state, state.B);
}

// 0x81  ADD C  aka ADD A,C
// A <- A + C
static void execute81(State8080& state)
{
	executeADD(state, state.C);
}

// 0x82  ADD D  aka ADD A,D
// A <- A + D
static void execute82(State8080& state)
{
	executeADD(state, state.D);
}

// 0x83  ADD E  aka ADD A,E
// A <- A + E
static void execute83(State8080& state)
{
	executeADD(state, state.E);
}

// 0x84  ADD H  aka ADD A,H
// A <- A + H
static void execute84(State8080& state)
{
	executeADD(state, state.H);
}

// 0x85  ADD L  aka ADD A,L
// A <- A + L
static void execute85(State8080& state)
{
	executeADD(state, state.L);
}

// 0x86  ADD M  aka ADD A,(HL)
// A <- A + (HL)
static void execute86(State8080& state)
{
	uint16_t HL = (uint16_t)(state.H << 8) | (uint16_t)state.L;
	uint8_t val = readByteFromMemory(state, HL);
	executeADD(state, val);
}

// 0x87  ADD A  aka ADD A,A
// A <- A + A
static void execute87(State8080& state)
{
	executeADD(state, state.A);
}

//------------------------------------------------------------------------------
// ADC
//
// ADD Register or Memory To Accumulator With Carry
// 
// The specified byte plus the content of the Carry bit is added to the contents 
// of the accumulator.
// 
// Condition bits affected : Carry, Sign, Zero, Parity, Auxiliary Carry

static void executeADC(State8080& state, uint8_t val)
{
	uint16_t result16 = (uint16_t)state.A + (uint16_t)val + (uint16_t)state.flags.C; // 16 bit arithmetic so can test for carry
	uint8_t result8 = (uint8_t)result16;

	state.flags.C = result16 > 0xff;
	state.flags.Z = calculateZeroFlag(result8);
	state.flags.S = calculateSignFlag(result8);
	state.flags.P = calculateParityFlag(result8);
	state.flags.AC = ((state.A & 0xf) + (val & 0xf) + state.flags.C) > 0xf ? 1 : 0;

	state.A = result8;
}

// 0x88  ADC B
// A <- A + B + CY
static void execute88(State8080& state)
{
	executeADC(state, state.B);
}

// 0x89  ADC C
// A <- A + C + CY
static void execute89(State8080& state)
{
	executeADC(state, state.C);
} 

// 0x8a  ADC D
// A <- A + D + CY
static void execute8a(State8080& state)
{
	executeADC(state, state.D);
}

// 0x8b  ADC E
// A <- A + E + CY
static void execute8b(State8080& state)
{
	executeADC(state, state.E);
} 

// 0x8c  ADC H
// A <- A + H + CY
static void execute8c(State8080& state)
{
	executeADC(state, state.H);
} 

// 0x8d  ADC L
// A <- A + L + CY
static void execute8d(State8080& state)
{
	executeADC(state, state.L);
} 

// 0x8e  ADC M
// A <- A + (HL) + CY
static void execute8e(State8080& state)
{
	uint16_t HL = (uint16_t)(state.H << 8) | (uint16_t)state.L;
	uint8_t val = readByteFromMemory(state, HL);
	executeADC(state, val);
} 

// 0x8f  ADC A
// A <- A + A + CY
static void execute8f(State8080& state)
{
	executeADC(state, state.A);
} 

//------------------------------------------------------------------------------
// SUB
//
// Subtract Register or Memory From Accumulator
//
// The specified byte is subtracted from the accumulator using two's complement 
// arithmetic.
//
// Condition bits affected: Carry, Sign, Zero, Parity, Auxiliary Carry

static void executeSUB(State8080& state, uint8_t val)
{
	uint8_t result = state.A - val;

	// The Data Book description of how this instruction affects the Carry flag is a little convoluted,
	// this is better: https://retrocomputing.stackexchange.com/questions/5953/carry-flag-in-8080-8085-subtraction
	// "The 8080 sets the carry flag when the unsigned value subtracted is greater than the unsigned value it is subtracted from."
	state.flags.C = (val > state.A) ? 1 : 0;
	state.flags.AC = (val & 0xf) > (state.A & 0xf) ? 1 : 0;

	state.flags.S = calculateSignFlag(result);
	state.flags.Z = calculateZeroFlag(result);
	state.flags.P = calculateParityFlag(result);

	state.A = result;
}

// 0x90  SUB B   A <- A - B
static void execute90(State8080& state)
{
	executeSUB(state, state.B);
}

// 0x91  SUB C   A <- A - C
static void execute91(State8080& state)
{
	executeSUB(state, state.C);
}

// 0x92  SUB D   A <- A - D
static void execute92(State8080& state)
{
	executeSUB(state, state.D);
}

// 0x93  SUB E   A <- A - E
static void execute93(State8080& state)
{
	executeSUB(state, state.E);
}

// 0x94  SUB H   A <- A - H 
static void execute94(State8080& state)
{
	executeSUB(state, state.H);
}

// 0x95  SUB L   A <- A - L
static void execute95(State8080& state)
{
	executeSUB(state, state.L);
}

// 0x96  SUB M   A <- A - (HL)	
static void execute96(State8080& state)
{
	uint16_t address = ((uint16_t)state.H << 8) | (state.L);
	HP_ASSERT(address < state.memorySizeBytes);
	uint8_t val = readByteFromMemory(state, address);
	executeSUB(state, val);
	writeByteToMemory(state, address, val);
}

// 0x97  SUB A   A <- A - A
static void execute97(State8080& state)
{
	executeSUB(state, state.A);
}

//------------------------------------------------------------------------------
// ANA
//
// Logical and Register or Memory With Accumulator
//
// The specified byte is logically ANDed bit by bit with the contents of the 
// accumulator.The Carry bit is reset to zero.
//
// Condition bits affected: Carry, Zero, Sign, Parity
//
// n.b. The "Intel 8080 Assembly Language Programming Manual" states that the
// Auxiliary Carry flag is not affected, but the "Intel 8080-8085 Assembly 
// Language Programming 1977 Intel" states:
//
// "There is some difference in the handling of the auxiliary carry flag by 
// the logical AND instructions In the 8080 processor and the 8085 processor.
// The 8085 logical AND instructions always set the auxiliary flag ON. The 8080 
// logical AND instructions sets the flag to reflect the logical OR of bit 3 
// of the values involved in the AND operation.

static void executeANA(State8080& state, uint8_t val)
{
	uint8_t result = state.A & val;
	
	state.flags.C = 0;
	state.flags.Z = calculateZeroFlag(result);
	state.flags.S = calculateSignFlag(result);
	state.flags.P = calculateParityFlag(result);

	// logical OR of bit 3 of the values involved in the AND operation (see above note)
	state.flags.AC = ((state.A & (1<<3)) | (val & (1<<3))) >> 3;

	state.A = result;
}

// 0xA0  ANA B  aka AND B
// A <- A & B
static void executeA0(State8080& state)
{
	executeANA(state, state.B);
}

// 0xA1  ANA C  aka AND C
// A <- A & C	
static void executeA1(State8080& state)
{
	executeANA(state, state.C);
} 

// 0xA2  ANA D  aka AND D
// A <- A & D	
static void executeA2(State8080& state)
{
	executeANA(state, state.D);
} 

// 0xA3  ANA E  aka AND E
// A <- A & E	
static void executeA3(State8080& state)
{
	executeANA(state, state.D);
} 

// 0xA4  ANA H  aka AND H
// A <- A & H
static void executeA4(State8080& state)
{
	executeANA(state, state.H);
}

// 0xA5  ANA L  aka AND L
// A <- A & L	
static void executeA5(State8080& state)
{
	executeANA(state, state.L);
} 

// 0xA6  ANA M  aka AND (HL)
// A <- A & (HL)
static void executeA6(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | (state.L);
	uint8_t val = readByteFromMemory(state, HL);
	executeANA(state, val);
}

// 0xA7  ANA A  aka AND A
// A <- A & A  (does not change value of A)
// n.b. This is a convenient way of testing if A is zero; it doesn't affect the value but does affect the flags
static void executeA7(State8080& state)
{
	executeANA(state, state.A);
}

//------------------------------------------------------------------------------
// XRA 
//
// Logical Exclusive-Or Register or Memory With Accumulator(Zero Accumulator)
//
// The specified byte is EXCLUSIVE-ORed bit by bit with the contents of the 
// accumulator. The Carry bit is reset to zero.
//
// Condition bits affected: Carry, Zero, Sign, Parity[, Auxiliary Carry (see below)]
//

static void executeXOR(State8080& state, uint8_t val)
{
	state.A = state.A ^ val;
	state.flags.C = 0;
	state.flags.Z = calculateZeroFlag(state.A);
	state.flags.S = calculateSignFlag(state.A);
	state.flags.P = calculateParityFlag(state.A);
	state.flags.AC = 0; // "The carry and auxilary carry flags are reset to zero." - Intel 8080/8085 Assembly Language Programming
}

// 0xA8  XRA B  aka XOR B
// A <- A^B
static void executeA8(State8080& state)
{
	executeXOR(state, state.B);
}

// 0xA9  XRA C  aka XOR C
// A <- A ^ C
static void executeA9(State8080& state)
{
	executeXOR(state, state.C);
}

// 0xAA  XRA D  aka XOR D
// A <- A ^ D
static void executeAA(State8080& state)
{
	executeXOR(state, state.D);
}

// 0xAB  XRA E  aka XOR E
// A <- A ^ E
static void executeAB(State8080& state)
{
	executeXOR(state, state.E);
}

// 0xAC  XRA H  aka XOR H
// A <- A ^ H
static void executeAC(State8080& state)
{
	executeXOR(state, state.H);
}

// 0xAD  XRA L  aka XOR L
// A <- A ^ L
static void executeAD(State8080& state)
{
	executeXOR(state, state.L);
}

// 0xAE  XRA M  aka XOR (HL)
// A <- A ^ (HL)
static void executeAE(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | (state.L);
	uint8_t val = readByteFromMemory(state, HL);
	executeXOR(state, val);
}

// 0xAF  XRA A  aka XOR A
// Logical Exclusive-Or Accumulator with self 
// A <- A ^ A
// n.b. A XOR A will always be zero. This is the best choice for zeroing a register on all CPUs!
// https://stackoverflow.com/questions/33666617/what-is-the-best-way-to-set-a-register-to-zero-in-x86-assembly-xor-mov-or-and
static void executeAF(State8080& state)
{
	executeXOR(state, state.A);
}

//------------------------------------------------------------------------------
// ORA 
//
// Logical or Register or Memory With Accumulator
// 
// The specified byte is logically ORed bit by bit with the contents of the 
// accumulator. The carry bit is reset to zero.
//
// Condition bits affected : Carry, zero, sign, parity
//
// n.b. The "Intel 8080 Assembly Language Programming Manual" states that the
// Auxiliary Carry flag is not affected, but the "Intel 8080-8085 Assembly 
// Language Programming 1977 Intel" states "The carry and auxiliary carry
// flags are reset to zero"

static void executeORA(State8080& state, uint8_t data)
{
	state.A |= data;
	state.flags.Z = calculateZeroFlag(state.A);
	state.flags.S = calculateSignFlag(state.A);
	state.flags.P = calculateParityFlag(state.A);
	state.flags.C = 0;
	state.flags.AC = 0; // "Intel 8080-8085 Assembly Language Programming 1977 Intel"
}

// 0xB0  ORA B
// A <- A|B
static void executeB0(State8080& state)
{
	executeORA(state, state.B);
}

// 0xB1  ORA C  aka OR C
// A <- A | C
static void executeB1(State8080& state)
{
	executeORA(state, state.C);
}

// 0xB2  ORA D  aka OR D
// A <- A | D
static void executeB2(State8080& state)
{
	executeORA(state, state.D);
}

// 0xB3  ORA E  aka OR E
// A <- A | E
static void executeB3(State8080& state)
{
	executeORA(state, state.E);
}

// 0xB4  ORA H  aka OR H
// A <- A | H
static void executeB4(State8080& state)
{
	executeORA(state, state.H);
}

// 0xB5  ORA L  aka OR L 
// A < -A | L
static void executeB5(State8080& state)
{
	executeORA(state, state.L);
}

// 0xB6  ORA M  aka OR (HL)
// A <- A|(HL)
static void executeB6(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | (state.L);
	uint8_t val = readByteFromMemory(state, HL);
	executeORA(state, val);
}

// 0xB7  ORA A  aka OR A 
// A < -A | A
static void executeB7(State8080& state)
{
	executeORA(state, state.A);
}

//------------------------------------------------------------------------------
// CMP
// Compare Register or Memory With Accumulator
//
// The specified byte is compared to the contents of the accumulator.
// The comparison is performed by internally subtracting the contents of REG 
// from the accumulator (leaving both unchanged) and setting the condition
// bits according to the result. In particular, the Zero bit is
// set if the quantities are equal, and reset if they are unequal.
// Since a subtract operation is performed, the Carry bit will be
// set if there is no carry out of bit 7, indicating that the
// contents of REG are greater than the contents of the accumulator,
// and reset otherwise.
// NOTE: If the two quantities to be compared differ in sign,
//    the sense of the Carry bit is reversed.
//
// Condition bits affected : Carry, Zero, Sign, Parity, Auxiliary Carry

static void executeCMP(State8080& state, uint8_t val)
{
	uint8_t diff = state.A - val;

	// The Data Book description of how this instruction affects the Carry flag is a little convoluted,
	// this is better: https://retrocomputing.stackexchange.com/questions/5953/carry-flag-in-8080-8085-subtraction
	// "The 8080 sets the carry flag when the unsigned value subtracted is greater than the unsigned value it is subtracted from."
	state.flags.C = val > state.A ? 1 : 0;
	state.flags.AC = (val & 0xf) > (state.A & 0xf) ? 1 : 0;

	state.flags.S = calculateSignFlag(diff);
	state.flags.Z = calculateZeroFlag(diff);
	state.flags.P = calculateParityFlag(diff);
}

// 0xB8  CMP B  aka CP B
static void executeB8(State8080& state)
{
	executeCMP(state, state.B);
}

// 0xB9  CMP C  aka CP C
static void executeB9(State8080& state)
{
	executeCMP(state, state.C);
}

// 0xBA  CMP D  aka CP D
static void executeBA(State8080& state)
{
	executeCMP(state, state.D);
}

// 0xBB  CMP E  aka CP E
static void executeBB(State8080& state)
{
	executeCMP(state, state.E);
}

// 0xBC  CMP H  aka CP H
static void executeBC(State8080& state)
{
	executeCMP(state, state.H);
}

// 0xBD  CMP L  aka CP L
static void executeBD(State8080& state)
{
	executeCMP(state, state.L);
}

// 0xBE  CMP M  aka CP (HL)
static void executeBE(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | (state.L);
	uint8_t val = readByteFromMemory(state, HL);
	executeCMP(state, val);
}

// 0xBF  CMP A  aka CP A
static void executeBF(State8080& state)
{
	executeCMP(state, state.A);
}

//------------------------------------------------------------------------------

// 0xC0  RNZ
// Return If Not Zero
// If the Zero bit is 0, a return operation is performed
static void executeC0(State8080& state)
{
	if(state.flags.Z == 0)
		performReturnOperation(state);
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

// 0xC6  ADI <d8>  aka ADD A,<d8>
// Add Immediate To Accumulator
// Condition bits affected: Carry, Sign, Zero, Parity, Auxiliary Carry
static void executeC6(State8080& state)
{
	uint8_t d8 = getInstructionD8(state);
	
	// perform 16 bit addition so can easily get carry bit
	uint16_t result = (uint16_t)state.A + (uint16_t)d8;
	state.A = (uint8_t)result;
	state.flags.C = (result & 0xffff0000) == 0 ? 0 : 1; // #TODO: Just test bit 16?
	state.flags.S = calculateSignFlag(state.A);
	state.flags.Z = calculateZeroFlag(state.A);
	state.flags.P = calculateParityFlag(state.A);
	// #TODO: Calculate Auxiliary Carry
}

// 0xC8  RZ	
// Return If Zero
// If the Zero bit is 1, a return operation is performed.
static void executeC8(State8080& state)
{
	if(state.flags.Z == 1)
		performReturnOperation(state);
}

// 0xC9 RET
static void executeC9(State8080& state)
{
	performReturnOperation(state);
}

// 0xCA  JZ <address>
// Jump If Zero
// If the Zero bit is one, program execution continues at the memory address
// if Z, PC <- adr
static void executeCA(State8080& state)
{
	uint16_t address = getInstructionAddress(state);
	HP_ASSERT(address < state.memorySizeBytes); // #TODO: Should really assert that is in ROM, or maybe ROM or RAM via callback to machine
	if(state.flags.Z)
		state.PC = address;
}

// 0xd0 RNC
// Return If No Carry
// If the Carry bit is zero, a return operation is performed
static void executeD0(State8080& state)
{
	if(state.flags.C == 0)
		performReturnOperation(state);
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

// 0xD2  JNC <address>
// Jump If No Carry
// If the Carry bit is zero, program execution continues at the specified memory address
// if NCY, PC <- adr
static void executeD2(State8080& state)
{
	if(state.flags.C == 0)
	{
		uint16_t address = getInstructionAddress(state);
		HP_ASSERT(address < state.memorySizeBytes); // this is the best we can do with no access to memory map
		state.PC = address;
	}
}

// 0xD3  OUT d8  aka OUT d8,A
// The contents of A are sent to output device number <d8> 
static void executeD3(State8080& state)
{
	uint8_t port = getInstructionD8(state);
	if(state.out != nullptr)
		state.out(port, state.A, state.userdata);
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

// 0xD6  SUI d8  aka SUB
// Subtract Immediate From Accumulator
// A <- A - d8
// Condition bits affected: Carry, Sign, Zero, Parity, Auxiliary Carry
static void executeD6(State8080& state)
{
	uint8_t val = getInstructionD8(state);
	uint8_t result = state.A - val;

	// The Data Book description of how this instruction affects the Carry flag is a little convoluted,
	// this is better: https://retrocomputing.stackexchange.com/questions/5953/carry-flag-in-8080-8085-subtraction
	// "The 8080 sets the carry flag when the unsigned value subtracted is greater than the unsigned value it is subtracted from."
	state.flags.C = (val > state.A) ? 1 : 0;

	state.flags.AC = (val & 0xf) > (state.A & 0xf) ? 1 : 0;

	state.flags.S = calculateSignFlag(result);
	state.flags.Z = calculateZeroFlag(result);
	state.flags.P = calculateParityFlag(result);

	state.A = result;
}

// 0xd8  RC
// Return If Carry
// If the Carry bit is 1, a return operation is performed
static void executeD8(State8080& state)
{
	if(state.flags.C == 1)
		performReturnOperation(state);
}

// 0xDA  JC <address>
// Jump If Carry
// If the Carry bit is 1, PC <- adr
static void executeDA(State8080& state)
{
	if(state.flags.C == 1)
	{
		uint16_t address = getInstructionAddress(state);
		HP_ASSERT(address < state.memorySizeBytes); // this is the best we can do with no access to memory map
		state.PC = address;
	}
}

// 0xDB  IN <d8>  aka IN A,<d8>
// An 8-bit data byte is read from the numberd input device into the accumulator.
static void executeDB(State8080& state)
{
	uint8_t port = getInstructionD8(state);

	if(state.in)
		state.A = state.in(port, state.userdata);
	else
		state.A = 0;
}

// 0xDE  SBI d8  aka SBC A,d8
// Subtract Immediate from Accumulator With Borrow
// The Carry bit is internally added to the byte of immediate data.
// This value is then subtracted from the accumulator using two's complement arithmetic.
// Since this is a subtraction operation, the carry bit is
// set if there is no carry out of the high - order position, and
// reset if there is a carry out.
// Condition bits affected : Carry, Sign, Zero, Parity, Auxiliary Carry
// A <- A-d8-CY
static void executeDE(State8080& state)
{
	uint8_t d8 = getInstructionD8(state);
	d8 += state.flags.C;

	// The Data Book description of how this instruction affects the Carry flag is a little convoluted,
	// this is better: https://retrocomputing.stackexchange.com/questions/5953/carry-flag-in-8080-8085-subtraction
	// "The 8080 sets the carry flag when the unsigned value subtracted is greater than the unsigned value it is subtracted from."
	state.flags.C = (d8 > state.A) ? 1 : 0;

	state.A -= d8;

	state.flags.S = calculateSignFlag(state.A);
	state.flags.Z = calculateZeroFlag(state.A);
	state.flags.P = calculateParityFlag(state.A);
	// #TODO: state.flags.A
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

// 0xE3  XTHL  aka EX (SP),HL
// Exchange Stack
// The contents of the L register are exchanged
// with the contents of the memory byte whose address
// is held in the stack pointer SP.The contents of the H
// register are exchanged with the contents of the memory
// byte whose address is one greater than that held in the stack
// pointer.
// 
// L <-> (SP)       swap LSB
// H <-> (SP + 1)   swap MSB
static void executeE3(State8080& state)
{
	// LSB
	uint8_t temp = state.L;
	state.L = readByteFromMemory(state, state.SP);
	writeByteToMemory(state, state.SP, temp);

	// MSB
	temp = state.H;
	state.H = readByteFromMemory(state, state.SP + 1);
	writeByteToMemory(state, state.SP + 1, temp);
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

// 0xE6  ANI <d8>  aka AND <d8>
// And Immediate With Accumulator
// The Carry bit is reset to zero
// A <- A & d8
// Condition bits affected: Carry, Zero, Sign, Parity	
static void executeE6(State8080& state)
{
	uint8_t d8 = getInstructionD8(state);
	state.A &= d8;
	state.flags.C = 0;
	state.flags.Z = calculateZeroFlag(state.A);
	state.flags.S = calculateSignFlag(state.A);
	state.flags.P = calculateParityFlag(state.A);
}

// 0xE9  PCHL  aka JP (HL)
// The contents of the H register replace the
// most significant 8 bits of the program counter, and the contents
// of the L register replace the least significant 8 bits of
// the program counter.This causes program execution to continue
// at the address contained in the Hand L registers.
// PC.hi <- H; PC.lo <- L
static void executeE9(State8080& state)
{
	uint16_t HL = ((uint16_t)state.H << 8) | (state.L);
	HP_ASSERT(HL < state.memorySizeBytes); // #TODO: Should really test vs memory map to ensure jump is into ROM
	state.PC = HL;
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

// 0xF1  POP PSW  aka POP AF
static void executeF1(State8080& state)
{
	// stored in memory in little endian format
	HP_ASSERT(state.SP < state.memorySizeBytes - 2);
	uint8_t lsb = readByteFromMemory(state, state.SP);
	uint8_t msb = readByteFromMemory(state, state.SP + 1);

	// First register of register pair always contains the MSB
	state.A = msb;
	state.flags = *(Flags8080*)&lsb;
	state.SP += 2;
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

//------------------------------------------------------------------------------
// 0xF6  ORI d8
//
// Or Immediate With Accumulator
// 
// The byte of immediate data is logically ORed with the contents of the accumulator.
// The result is stored in the accumulator. The Carry bit
// is reset to zero, while the Zero, Sign, and Parity bits are set
// according to the result.
//
// Condition bits affected : Carry, Zero, Sign, Parity[, Auxiliary Carry (see below)]

// A <- A|data

static void executeF6(State8080& state)
{
	uint8_t d8 = getInstructionD8(state);
	state.A |= d8;
	state.flags.C = 0;
	state.flags.AC = 0; // "ORI also resets the carry and auxiliary carry flags to zero." - "Intel 8080 / 8085 Assembly Language Programming"
	state.flags.Z = calculateZeroFlag(state.A);
	state.flags.S = calculateSignFlag(state.A);
	state.flags.P = calculateParityFlag(state.A);
}

//------------------------------------------------------------------------------

// 0xFA  JM <address>   aka JP M,<adr>
// Jump If Minus
// If the Sign bit is one (indicating a negative
// result), program execution continues at the memory
// address adr.
// Condition bits affected : None
static void executeFA(State8080& state)
{
	if(state.flags.S == 1)
	{
		uint16_t address = getInstructionAddress(state);
		HP_ASSERT(address < state.memorySizeBytes);
		state.PC = address;
	}
}

// 0xFB  EI
// enable interrupts
static void executeFB(State8080& state)
{
	state.INTE = 1;
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
	{ 0x02, "STAX B", 1, execute02 }, // aka LD (BC),A  (BC) <- A
	{ 0x03, "INX B", 1, execute03 }, // aka INC BC    BC <- BC+1
	{ 0x04, "INR B", 1, execute04 }, // aka INC B   B <- B + 1    Z, S, P, AC
	{ 0x05, "DCR B", 1, execute05 }, // aka DEC B    B <- B - 1   Z, S, P, AC
	{ 0x06, "MVI B, %02X", 2, execute06 }, // B <- byte 2
	{ 0x07, "RLC",	1, execute07 }, // aka RLCA	  A = A << 1; bit 0 = prev bit 7; CY = prev bit 7   Sets Carry flag
	{ 0x08, "-", 1, nullptr },
	{ 0x09, "DAD BC", 1, execute09 }, // aka ADD HL,BC   HL <- HL + BC  Sets Carry flag
	{ 0x0a, "LDAX B", 1, execute0A}, // aka LD A,(BC)  A <- (BC)
	{ 0x0b, "DCX B", 1, execute0B }, // aka DEC BC    BC = BC - 1
	{ 0x0c, "INR C", 1, execute0C }, // aka INC C    C <- C + 1    Z, S, P, AC
	{ 0x0d, "DCR C", 1, execute0D }, // aka DEC C	 C <- C - 1    Z, S, P, AC
	{ 0x0e, "MVI C,%02X", 2, execute0E }, // C <- byte 2
	{ 0x0f, "RRC",	1, execute0F }, // aka RRCA  Rotate Accumulator Right
	{ 0x10, "-", 1, nullptr }, //	
	{ 0x11, "LXI D,%04X",	3, execute11 }, // D <- byte 3, E <- byte 2
	{ 0x12, "STAX D", 1, execute12 }, // aka LD (DE),A   (DE) <- A
	{ 0x13, "INX D", 1, execute13 }, //			DE < -DE + 1
	{ 0x14, "INR D", 1, execute14 }, // aka INC D    D <- D + 1   Z, S, P, AC	
	{ 0x15, "DCR D", 1, execute15 }, // aka DEC D	 D <- D - 1    Z, S, P, AC
	{ 0x16, "MVI D, %02X",	2, execute16 }, // D <- byte 2
	{ 0x17, "RAL",	1, nullptr }, //		CY	A = A << 1; bit 0 = prev CY; CY = prev bit 7
	{ 0x18, "-", 1, nullptr }, //	
	{ 0x19, "DAD D", 1, execute19 }, // aka ADD HL,DE   HL <- HL + DE   Sets Carry flag
	{ 0x1a, "LDAX D", 1, execute1A }, // A <- (DE)
	{ 0x1b, "DCX D", 1, execute1B }, // aka DEC DE   DE = DE - 1
	{ 0x1c, "INR E", 1, execute1C }, // aka INC E    E <- E + 1    Z, S, P, AC
	{ 0x1d, "DCR E", 1, execute1D }, // aka DEC E	 E <- E - 1    Z, S, P, AC
	{ 0x1e, "MVI E,%02X",	2, nullptr }, //			E < -byte 2
	{ 0x1f, "RAR",	1, execute1F }, // A = A >> 1; bit 7 = prev bit 7; CY = prev bit 0		CY	
	{ 0x20, "-", 1, nullptr }, //	
	{ 0x21, "LXI H,%04X", 3, execute21 }, // H <- byte 3, L <-byte 2
	{ 0x22, "SHLD %04X", 3, execute22 }, // aka LD (adr),HL  (adr) < -L; (adr + 1) < -H
	{ 0x23, "INX H",	1, execute23 }, // HL <- HL + 1
	{ 0x24, "INR H", 1, execute24}, // aka INC H    H <- H + 1    Z, S, P, AC
	{ 0x25, "DCR H", 1, execute25 }, // aka DEC H	 H <- H - 1    Z, S, P, AC
	{ 0x26, "MVI H,%02X", 2, execute26 }, // H <- byte 2
	{ 0x27, "DAA",	1, execute27 }, // Decimal Adjust Accumulator
	{ 0x28, "-", 1, nullptr }, //	
	{ 0x29, "DAD HL", 1, execute29 }, // aka ADD HL,HL   HL <- HL + HL   Sets Carry flag
	{ 0x2a, "LHLD %04X", 3, execute2A }, // L <- (adr); H <- (adr + 1)
	{ 0x2b, "DCX H", 1, execute2B }, // aka DEC HL   HL <- HL-1
	{ 0x2c, "INR L", 1, execute2C }, // aka INC L    L <- L + 1    Z, S, P, AC
	{ 0x2d, "DCR L", 1, execute2D }, // aka DEC L	 L <- L - 1    Z, S, P, AC
	{ 0x2e, "MVI L, %02X", 2, execute2E }, // L <- byte 2
	{ 0x2f, "CMA", 1, execute2F }, // aka CPL  A <- !A
	{ 0x30, "-", 1, nullptr }, //	
	{ 0x31, "LXI SP, %04X",	3, execute31 }, // SP.hi <- byte 3, SP.lo <- byte 2
	{ 0x32, "STA %04X",	3, execute32 }, // aka LD (adr),A    (adr) <- A
	{ 0x33, "INX SP",	1, nullptr }, //			SP = SP + 1
	{ 0x34, "INR M", 1, execute34 }, // aka INC (HL)   (HL) <- (HL) + 1    Z, S, P, AC  
	{ 0x35, "DCR M", 1, execute35 }, // aka DEC (HL)   (HL) <- (HL) - 1    Z, S, P, AC 
	{ 0x36, "MVI M,%02X",	2, execute36 }, // (HL) <- byte 2
	{ 0x37, "STC", 1, execute37 }, // Carry = 1
	{ 0x38, "-", 1},
	{ 0x39, "DAD SP",	1, nullptr }, //		CY	HL = HL + SP
	{ 0x3a, "LDA %04X",	3, execute3A }, // A <- (adr)
	{ 0x3b, "DCX SP", 1, execute3B }, // aka DEC SP    SP = SP - 1
	{ 0x3c, "INR A", 1, execute3C }, // aka INC A   A <- A+1   Z, S, P, AC
	{ 0x3d, "DCR A", 1, execute3D }, // aka DEC A    A <- A-1    Z, S, P, AC	
	{ 0x3e, "MVI A,%02X", 2, execute3E }, // A <- byte 2
	{ 0x3f, "CMC",	1, nullptr }, //		CY	CY = !CY
	{ 0x40, "MOV B,B", 1, execute40 }, // B <- B
	{ 0x41, "MOV B,C", 1, execute41 }, // B <- C
	{ 0x42, "MOV B,D", 1, execute42 }, // B <- D
	{ 0x43, "MOV B,E", 1, execute43 }, // B <- E
	{ 0x44, "MOV B,H", 1, execute44 }, // B <- H
	{ 0x45, "MOV B,L", 1, execute45 }, // B <- L
	{ 0x46, "MOV B,M", 1, execute46 }, // B <- (HL)
	{ 0x47, "MOV B,A", 1, execute47 }, // B <- A
	{ 0x48, "MOV C,B", 1, execute48 }, // C <- B
	{ 0x49, "MOV C,C", 1, execute49 }, // C <- C
	{ 0x4a, "MOV C,D", 1, execute4A }, // C <- D
	{ 0x4b, "MOV C,E", 1, execute4B }, // C <- E
	{ 0x4c, "MOV C,H", 1, execute4C }, // C <- H
	{ 0x4d, "MOV C,L", 1, execute4D }, // C <- L
	{ 0x4e, "MOV C,M", 1, execute4E }, // C <- (HL)
	{ 0x4f, "MOV C,A", 1, execute4F }, // C <- A
	{ 0x50, "MOV D,B", 1, execute50 }, // D <- B
	{ 0x51, "MOV D,C", 1, execute51 }, // D <- C
	{ 0x52, "MOV D,D", 1, execute52 }, // D <- D
	{ 0x53, "MOV D,E", 1, execute53 }, // D <- E
	{ 0x54, "MOV D,H", 1, execute54 }, // D <- H
	{ 0x55, "MOV D,L", 1, execute55 }, // D <- L
	{ 0x56, "MOV D,M", 1, execute56 }, // D <- (HL)
	{ 0x57, "MOV D,A", 1, execute57 }, // D <- A
	{ 0x58, "MOV E,B", 1, execute58 }, // E <- B
	{ 0x59, "MOV E,C", 1, execute59 }, // E <- C
	{ 0x5a, "MOV E,D", 1, execute5A }, // E <- D
	{ 0x5b, "MOV E,E", 1, execute5B }, // E <- E
	{ 0x5c, "MOV E,H", 1, execute5C }, // E <- H
	{ 0x5d, "MOV E,L", 1, execute5D }, // E <- L
	{ 0x5e, "MOV E,M", 1, execute5E }, // E < -(HL)
	{ 0x5f, "MOV E,A", 1, execute5F }, // E <- A
	{ 0x60, "MOV H,B", 1, execute60 }, // H <- B
	{ 0x61, "MOV H,C", 1, execute61 }, // H <- C
	{ 0x62, "MOV H,D", 1, execute62 }, // H <- D
	{ 0x63, "MOV H,E", 1, execute63 }, // H <- E
	{ 0x64, "MOV H,H", 1, execute64 }, // H <- H
	{ 0x65, "MOV H,L", 1, execute65 }, // H <- L
	{ 0x66, "MOV H,M", 1, execute66 }, // H <- (HL)
	{ 0x67, "MOV H,A", 1, execute67 }, // H <- A
	{ 0x68, "MOV L,B", 1, execute68 }, // L <- B
	{ 0x69, "MOV L,C", 1, execute69 }, // L <- C
	{ 0x6a, "MOV L,D", 1, execute6A }, // L <- D
	{ 0x6b, "MOV L,E", 1, execute6B }, // L <- E
	{ 0x6c, "MOV L,H", 1, execute6C }, // L <- H
	{ 0x6d, "MOV L,L", 1, execute6D }, // L <- L
	{ 0x6e, "MOV L,M", 1, execute6E }, // L <- (HL)
	{ 0x6f, "MOV L,A", 1, execute6F }, // L <- A
	{ 0x70, "MOV M,B", 1, execute70 }, // (HL) <- B
	{ 0x71, "MOV M,C", 1, execute71 }, // (HL) <- C
	{ 0x72, "MOV M,D", 1, execute72 }, // (HL) <- D
	{ 0x73, "MOV M,E", 1, execute73 }, // (HL) <- E
	{ 0x74, "MOV M,H", 1, execute74 }, // (HL) <- H
	{ 0x75, "MOV M,L", 1, execute75 }, // (HL) <- L
	{ 0x76, "HLT",	1, nullptr }, //			special
	{ 0x77, "MOV M,A", 1, execute77 }, // (HL) <- A
	{ 0x78, "MOV A,B", 1, execute78 }, // A <- B
	{ 0x79, "MOV A,C", 1, execute79 }, // A <- C
	{ 0x7a, "MOV A,D", 1, execute7A }, // A <- D
	{ 0x7b, "MOV A,E", 1, execute7B }, // A <- E
	{ 0x7c, "MOV A,H", 1, execute7C }, // A <- H
	{ 0x7d, "MOV A,L", 1, execute7D }, // A <- L
	{ 0x7e, "MOV A,M", 1, execute7E }, // A <- (HL)
	{ 0x7f, "MOV A,A", 1, execute7F }, // A <- A
	{ 0x80, "ADD B", 1, execute80 }, //	aka ADD A,B     A <- A + B     Z, S, P, CY, AC
	{ 0x81, "ADD C", 1, execute81 }, //	aka ADD A,C     A <- A + C     Z, S, P, CY, AC
	{ 0x82, "ADD D", 1, execute82 }, //	aka ADD A,D     A <- A + D     Z, S, P, CY, AC
	{ 0x83, "ADD E", 1, execute83 }, //	aka ADD A,E     A <- A + E     Z, S, P, CY, AC
	{ 0x84, "ADD H", 1, execute84 }, //	aka ADD A,H     A <- A + H     Z, S, P, CY, AC
	{ 0x85, "ADD L", 1, execute85 }, // aka ADD A,L     A <- A + L     Z, S, P, CY, AC
	{ 0x86, "ADD M", 1, execute86 }, // aka ADD A,(HL)  A <- A + (HL)  Z, S, P, CY, AC	
	{ 0x87, "ADD A", 1, execute87 }, //	aka ADD A,A     A <- A + A     Z, S, P, CY, AC	
	{ 0x88, "ADC B", 1, execute88 }, // A <- A + B + CY     Z, S, P, CY, AC	
	{ 0x89, "ADC C", 1, execute89 }, // A <- A + C + CY     Z, S, P, CY, AC	
	{ 0x8a, "ADC D", 1, execute8a }, // A <- A + D + CY     Z, S, P, CY, AC	
	{ 0x8b, "ADC E", 1, execute8b }, // A <- A + E + CY     Z, S, P, CY, AC	
	{ 0x8c, "ADC H", 1, execute8c }, // A <- A + H + CY     Z, S, P, CY, AC	
	{ 0x8d, "ADC L", 1, execute8d }, // A <- A + L + CY     Z, S, P, CY, AC	
	{ 0x8e, "ADC M", 1, execute8e }, // A <- A + (HL)+CY    Z, S, P, CY, AC	
	{ 0x8f, "ADC A", 1, execute8f }, // A <- A + A + CY     Z, S, P, CY, AC	
	{ 0x90, "SUB B", 1, execute90 }, // A <- A - B       Z, S, P, CY, AC	
	{ 0x91, "SUB C", 1, execute91 }, // A <- A - C       Z, S, P, CY, AC	
	{ 0x92, "SUB D", 1, execute92 }, // A <- A - D       Z, S, P, CY, AC	
	{ 0x93, "SUB E", 1, execute93 }, // A <- A - E       Z, S, P, CY, AC	
	{ 0x94, "SUB H", 1, execute94 }, // A <- A - H       Z, S, P, CY, AC	
	{ 0x95, "SUB L", 1, execute95 }, // A <- A - L       Z, S, P, CY, AC	
	{ 0x96, "SUB M", 1, execute96 }, // A <- A - (HL)    Z, S, P, CY, AC	
	{ 0x97, "SUB A", 1, execute97 }, // A <- A - A       Z, S, P, CY, AC	
	{ 0x98, "SBB B", 1, nullptr }, // A <- A - B - CY		Z, S, P, CY, AC	  Not used in invaders
	{ 0x99, "SBB C", 1, nullptr }, // A <- A - C - CY		Z, S, P, CY, AC	
	{ 0x9a, "SBB D", 1, nullptr }, // A <- A - D - CY		Z, S, P, CY, AC	
	{ 0x9b, "SBB E", 1, nullptr }, // A <- A - E - CY		Z, S, P, CY, AC	
	{ 0x9c, "SBB H", 1, nullptr }, // A <- A - H - CY		Z, S, P, CY, AC	
	{ 0x9d, "SBB L", 1, nullptr }, // A <- A - L - CY		Z, S, P, CY, AC	
	{ 0x9e, "SBB M", 1, nullptr }, // A <- A - (HL) - CY		Z, S, P, CY, AC	
	{ 0x9f, "SBB A", 1, nullptr }, // A <- A - A - CY		Z, S, P, CY, AC	
	{ 0xa0, "ANA B", 1, executeA0 }, // aka AND B     A <- A&B          Z, S, P, CY, AC	
	{ 0xa1, "ANA C", 1, executeA1 }, //	aka AND C     A <- A & C        Z, S, P, CY, AC	
	{ 0xa2, "ANA D", 1, executeA2 }, //	aka AND D     A <- A & D        Z, S, P, CY, AC	
	{ 0xa3, "ANA E", 1, executeA3 }, //	aka AND E     A <- A & E        Z, S, P, CY, AC	
	{ 0xa4, "ANA H", 1, executeA4 }, //	aka AND H     A <- A & H        Z, S, P, CY, AC	
	{ 0xa5, "ANA L", 1, executeA5 }, //	aka AND L     A <- A & L        Z, S, P, CY, AC	
	{ 0xa6, "ANA M", 1, executeA6 }, // aka AND (HL)  A <- A & (HL)     Z, S, P, CY, AC
	{ 0xa7, "ANA A", 1, executeA7 }, // aka AND A     A <- A & A        Z, S, P, CY, AC
	{ 0xa8, "XRA B", 1, executeA8 }, // aka XOR B     A <- A^B          Z, S, P, CY, AC
	{ 0xa9, "XRA C", 1, executeA9 }, //	aka XOR C     A <- A ^ C        Z, S, P, CY, AC
	{ 0xaa, "XRA D", 1, executeAA }, //	aka XOR D     A <- A ^ D        Z, S, P, CY, AC
	{ 0xab, "XRA E", 1, executeAB }, //	aka XOR E     A <- A ^ E        Z, S, P, CY, AC
	{ 0xac, "XRA H", 1, executeAC }, //	aka XOR H     A <- A ^ H        Z, S, P, CY, AC
	{ 0xad, "XRA L", 1, executeAD }, //	aka XOR L     A <- A ^ L        Z, S, P, CY, AC
	{ 0xae, "XRA M", 1, executeAE }, //	aka XOR (HL)  A <- A ^ (HL)     Z, S, P, CY, AC
	{ 0xaf, "XRA A", 1, executeAF }, // aka XOR A    A <- A ^ A    (A <- 0)    Z, S, P, CY, AC	
	{ 0xb0, "ORA B", 1, executeB0 }, // aka OR B     A <- A | B     Z, S, P, CY, AC
	{ 0xb1, "ORA C", 1, executeB1 }, //	aka OR C     A <- A | C     Z, S, P, CY, AC
	{ 0xb2, "ORA D", 1, executeB2 }, //	aka OR D     A <- A | D     Z, S, P, CY, AC
	{ 0xb3, "ORA E", 1, executeB3 }, //	aka OR E     A <- A | E     Z, S, P, CY, AC
	{ 0xb4, "ORA H", 1, executeB4 }, // aka OR H     A <- A|H       Z, S, P, CY, AC
	{ 0xb5, "ORA L", 1, executeB5 }, //	aka OR L     A < -A | L     Z, S, P, CY, AC
	{ 0xb6, "ORA M", 1, executeB6 }, // aka OR (HL)  A <- A | (HL)  Z, S, P, CY, AC
	{ 0xb7, "ORA A", 1, executeB7 }, // aka OR A     A < -A | A     Z, S, P, CY, AC
	{ 0xb8, "CMP B", 1, executeB8 }, // aka CP B     A-B   Z, S, P, CY, AC
	{ 0xb9, "CMP C", 1, executeB9 }, //	aka CP C     A-B   Z, S, P, CY, AC
	{ 0xba, "CMP D", 1, executeBA }, //	aka CP D     A-B   Z, S, P, CY, AC
	{ 0xbb, "CMP E", 1, executeBB }, //	aka CP E     A-B   Z, S, P, CY, AC
	{ 0xbc, "CMP H", 1, executeBC }, //	aka CP H     A-B   Z, S, P, CY, AC
	{ 0xbd, "CMP L", 1, executeBD }, //	aka CP L     A-B   Z, S, P, CY, AC
	{ 0xbe, "CMP M", 1, executeBE }, //	aka CP (HL)  A-B   Z, S, P, CY, AC
	{ 0xbf, "CMP A", 1, executeBF }, //	aka CP A     A-B   Z, S, P, CY, AC
	{ 0xc0, "RNZ", 1, executeC0 }, // if NZ, RET
	{ 0xc1, "POP BC", 1, executeC1 }, // C < -(sp); B < -(sp + 1); sp < -sp + 2
	{ 0xc2, "JNZ %04X", 3, executeC2 }, // if NZ, PC <- adr
	{ 0xc3, "JMP %04X", 3, executeC3 }, //			PC <= adr
	{ 0xc4, "CNZ %04X", 3, executeC4 }, // if NZ, CALL adr
	{ 0xc5, "PUSH BC", 1, executeC5 }, // (sp - 2) < -C; (sp - 1) < -B; sp < -sp - 2
	{ 0xc6, "ADI %02X", 2, executeC6 }, // aka ADD A,<d8>
	{ 0xc7, "RST 0",	1, nullptr }, //			CALL $0
	{ 0xc8, "RZ	1",	1, executeC8 }, // if Z, RET
	{ 0xc9, "RET",	1, executeC9 }, // PC.lo <- (sp); PC.hi <- (sp + 1); SP <- SP + 2
	{ 0xca, "JZ %04X", 3, executeCA }, // if Z, PC <- adr
	{ 0xcb, "-", 1, nullptr }, //	
	{ 0xcc, "CZ %04X",	3, executeCC }, // if Z, CALL adr
	{ 0xcd, "CALL %04X", 3, executeCD }, // (SP - 1) <- PC.hi; (SP - 2) <- PC.lo; SP <- SP - 2; PC = adr
	{ 0xce, "ACI %02X",	2, nullptr }, //		Z, S, P, CY, AC	A < -A + data + CY
	{ 0xcf, "RST 1", 1, nullptr }, //			CALL $8
	{ 0xd0, "RNC",	1, executeD0 }, // if NCY, RET
	{ 0xd1, "POP DE", 1, executeD1 }, // E < -(sp); D < -(sp + 1); sp < -sp + 2
	{ 0xd2, "JNC %04X", 3, executeD2 }, // if NCY, PC <- adr
	{ 0xd3, "OUT %02X",	2, executeD3 },
	{ 0xd4, "CNC %04X",	3, executeD4 }, // aka CALL NC,<address>  if NCY, CALL adr
	{ 0xd5, "PUSH DE",	1, executeD5 }, // (sp - 2) <- E; (sp - 1) <- D; sp <- sp-2
	{ 0xd6, "SUI %02X", 2, executeD6 }, // aka SUB  A <- A-d8   Z, S, P, CY, AC	
	{ 0xd7, "RST 2",	1, nullptr }, //			CALL $10
	{ 0xd8, "RC", 1, executeD8 }, // if Carry set, RET
	{ 0xd9, "-", 1, nullptr }, //	
	{ 0xda, "JC %04X",	3, executeDA }, // if CY, PC < -adr
	{ 0xdb, "IN %02X", 2, executeDB },
	{ 0xdc, "CC %04X",	3, executeDC }, // if CY, CALL adr
	{ 0xdd, "-", 1, nullptr }, //	
	{ 0xde, "SBI %02X",	2, executeDE }, // aka SBC A,d8   A <- A-d8-CY   Z, S, P, CY, AC	
	{ 0xdf, "RST 3",	1, nullptr }, //			CALL $18
	{ 0xe0, "RPO",	1, nullptr }, //			if PO, RET
	{ 0xe1, "POP HL", 1, executeE1 }, // L <- (sp); H <- (sp+1); sp <- sp+2
	{ 0xe2, "JPO %04X", 3, nullptr }, //			if PO, PC < -adr
	{ 0xe3, "XTHL",	1, executeE3 }, // L <-> (SP); H <-> (SP + 1)
	{ 0xe4, "CPO %04X", 3, executeE4 }, //			if PO, CALL adr
	{ 0xe5, "PUSH HL",	1, executeE5 }, // (sp-2) <-L ; (sp-1) <- H; sp <- sp-2
	{ 0xe6, "ANI %02X", 2, executeE6 }, // aka AND <d8>   A <- A & d8	
	{ 0xe7, "RST 4",	1, nullptr }, //			CALL $20
	{ 0xe8, "RPE",	1, nullptr }, //			if PE, RET
	{ 0xe9, "PCHL",	1, executeE9 }, // aka JP (HL)  PC.hi <- H; PC.lo <- L
	{ 0xea, "JPE %04X",	3, nullptr }, //			if PE, PC < -adr
	{ 0xeb, "XCHG",	1, executeEB }, // H <-> D; L <-> E
	{ 0xec, "CPE %04X", 3, executeEC }, //			if PE, CALL adr
	{ 0xed, "-", 1, nullptr }, //	
	{ 0xee, "XRI %02X",	2, nullptr }, //		Z, S, P, CY, AC	A < -A ^ data  "XRI also resets the carry and auxiliary carry flags to zero" - Intel 8080/8085 Assembly Language Programming
	{ 0xef, "RST 5",	1, nullptr }, //			CALL $28
	{ 0xf0, "RP", 1, nullptr }, //			if P, RET
	{ 0xf1, "POP PSW",	1, executeF1 }, // aka POP AF
	{ 0xf2, "JP %04X", 3, nullptr }, //			if P = 1 PC < -adr
	{ 0xf3, "DI",	1, nullptr }, //			special
	{ 0xf4, "CP %04X",	3, executeF4 }, //			if P, PC < -adr
	{ 0xf5, "PUSH PSW",	1, executeF5 }, // aka PUSH AF  (sp - 2) < -flags; (sp - 1) < -A; sp < -sp - 2
	{ 0xf6, "ORI %02X", 2, executeF6 }, // A <- A | data    Z, S, P, CY, AC	
	{ 0xf7, "RST 6",	1, nullptr }, //			CALL $30
	{ 0xf8, "RM",	1, nullptr }, //			if M, RET
	{ 0xf9, "SPHL",	1, nullptr }, //			SP = HL
	{ 0xfa, "JM %04X", 3, executeFA }, // Jump If Minus aka JP M,<adr>  if M, PC <- adr
	{ 0xfb, "EI", 1, executeFB }, // enable interrupts
	{ 0xfc, "CM %04X",	3, executeFC }, //			if M, CALL adr
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

// returns the number of cycles that the instruction took to execute
unsigned int Emulate8080Instruction(State8080& state)
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

	// #TODO: Add timings to all instructions
	unsigned int instructionCycleCount = 4; // TEMP placeholder!!
	return instructionCycleCount;
}

void Generate8080Interrupt(State8080& state, unsigned int interruptNumber)
{
	HP_ASSERT(interruptNumber < 8, "There are eight Restart instructions, RST 0 - RST 7");

	// Whenever INTE is equal to 0, the entire interrupt handling system is disabled,
	// and not interrupts will be accepted
	if(state.INTE == 0)
		return;

	// reset 8080 INTE bit [Data Book, p59]
	state.INTE = 0;

	// https://stackoverflow.com/questions/2165914/how-do-interrupts-work-on-the-intel-8080

	// Perform a manual RST instruction.
	// 
	// - The PC is pushed onto the stack, providing a return address for later use by a RETURN instruction within the interrupt handler
	// - Call to one of 8 eight-byte subroutines located in the first 64 bytes of memory

	// store return address in little-endian
	uint8_t msb = (uint8_t)(state.PC >> 8);
	uint8_t lsb = (uint8_t)(state.PC & 0xff);
	HP_ASSERT(state.SP >= 2);
	writeByteToMemory(state, state.SP - 2, lsb);
	writeByteToMemory(state, state.SP - 1, msb);
	state.SP -= 2;

	state.PC = (uint16_t)interruptNumber * 8;
}

#include "debugger.h"

#include "machine.h"
#include "8080.h"
#include "Helpers.h"
#include "Assert.h"

#include <stdio.h>

void Print8080State(const State8080& state)
{
#if 0
	// multi line
	printf("A: %02X  Flags: %c%c%c%c\n",
		state.A,
		state.flags.S ? 'S' : '-',
		state.flags.Z ? 'Z' : '-',
		state.flags.P ? 'P' : '-',
		state.flags.C ? 'C' : '-');
	printf("B: %02X  C: %02X\n", state.B, state.C);
	printf("D: %02X  E: %02X\n", state.D, state.E);
	printf("H: %02X  L: %02X\n", state.H, state.L);
	printf("SP: %04X\n", state.SP);
	printf("PC: %04X\n", state.PC);
#else
	// single line
	printf("A: %02X  Flags: %c%c%c%c  ", 
		state.A, 
		state.flags.S ? 'S' : '-', 
		state.flags.Z ? 'Z' : '-', 
		state.flags.P ? 'P' : '-', 
		state.flags.C ? 'C' : '-');
	printf("B: %02X  C: %02X  ", state.B, state.C);
	printf("D: %02X  E: %02X  ", state.D, state.E);
	printf("H: %02X  L: %02X  ", state.H, state.L);
	printf("SP: %04X  ", state.SP);
	printf("PC: %04X\n", state.PC);
#endif
}

void BreakMachine(Machine& machine, Debugger& debugger)
{
	HP_ASSERT(machine.running == true);
	machine.running = false;
	debugger.stepOverBreakpoint.active = false;
	debugger.stepOutActive = false;
	debugger.stepOutBreakpoint.active = false;
}

void ContinueMachine(Machine& machine)
{
	HP_ASSERT(machine.running == false);
	machine.running = true;
}

bool AddBreakpoint(Debugger& debugger, uint16_t address)
{
	if(debugger.breakpointCount == COUNTOF_ARRAY(debugger.breakpoints))
		return false;

	// #TODO: Don't add duplicates

	Breakpoint& breakpoint = debugger.breakpoints[debugger.breakpointCount++];
	breakpoint.address = address;
	breakpoint.active = true;
	return true;
}

void DeleteBreakpoint(Debugger& debugger, unsigned int index)
{
	HP_ASSERT(index < COUNTOF_ARRAY(debugger.breakpoints));
	HP_ASSERT(index < debugger.breakpointCount);

	// shuffle down to retain order
	for(unsigned int i = index; i < debugger.breakpointCount - 1; i++)
	{
		debugger.breakpoints[i] = debugger.breakpoints[i + 1];
	}
	debugger.breakpointCount--;
}

void ClearBreakpoints(Debugger& debugger)
{
	debugger.breakpointCount = 0;
}

void StepInto(Machine& machine, bool verbose)
{
	HP_ASSERT(machine.running == false);
	StepInstruction(&machine, verbose);
}

static uint16_t GetNextInstructionAddress(const Machine& machine)
{
	const uint8_t opcode = ReadByteFromMemory((void*)&machine, machine.cpu.PC, /*fatalOnFail*/true);
	unsigned int instructionSizeBytes = GetInstructionSizeBytes(opcode);
	uint16_t nextInstructionAddress = machine.cpu.PC + (uint16_t)instructionSizeBytes;
	return nextInstructionAddress;
}

static bool CurrentInstructionIsACall(const Machine& machine)
{
	const uint8_t opcode = ReadByteFromMemory((void*)&machine, machine.cpu.PC, /*fatalOnFail*/true);
	if(opcode == 0xCD)
		return true; // CALL
	if(opcode == 0xDC)
		return true; // CC
	if(opcode == 0xD4)
		return true; // CNC
	if(opcode == 0xCC)
		return true; // CZ
	if(opcode == 0xC4)
		return true; // CNZ
	if(opcode == 0xFC)
		return true; // CM
	if(opcode == 0xF4)
		return true; // CP
	if(opcode == 0xEC)
		return true; // CPE
	if(opcode == 0xE4)
		return true; // CPO

	return false;
}

void StepOver(Machine& machine, Debugger& debugger, bool verbose)
{
	HP_ASSERT(machine.running == false);

	if(CurrentInstructionIsACall(machine))
	{
		// set a hidden breakpoint on the next instruction and set the machine running
		HP_ASSERT(debugger.stepOverBreakpoint.active == false);
		debugger.stepOverBreakpoint.active = true;
		uint16_t nextInstructionAddress = GetNextInstructionAddress(machine);
		debugger.stepOverBreakpoint.address = nextInstructionAddress;
		ContinueMachine(machine);
	}
	else
	{
		StepInstruction(&machine, verbose);
	}
}

void StepOut(Machine& machine, Debugger& debugger, bool /*verbose*/)
{
	HP_ASSERT(machine.running == false);
	HP_ASSERT(debugger.stepOutActive == false);
	HP_ASSERT(debugger.stepOutBreakpoint.active == false); // just in case
	debugger.stepOutActive = true;
	ContinueMachine(machine);
}

void DebugStepFrame(Machine& machine, bool verbose)
{
	HP_ASSERT(!machine.running);
	machine.running = true;
	StepFrame(&machine, verbose);
	machine.running = false;
}

bool CurrentInstructionIsAReturnThatEvaluatesToTrue(const Machine& machine, uint16_t& returnAddress)
{
	const State8080& cpu = machine.cpu;

	const uint8_t opcode = ReadByteFromMemory((void*)&machine, cpu.PC, /*fatalOnFail*/true);

	bool willReturn = false;

	if(opcode == 0xC0)
	{
		// 0xC0  RNZ
		// Return If Not Zero
		// If the Zero bit is 0, a return operation is performed
		if(cpu.flags.Z == 0)
			willReturn = true;
	}
	else if(opcode == 0xC8)
	{
		// 0xC8  RZ	
		// Return If Zero
		// If the Zero bit is 1, a return operation is performed.
		if(cpu.flags.Z == 1)
			willReturn = true;
	}
	else if(opcode == 0xC9)
	{
		// 0xC9 RET
		willReturn = true;
	}
	else if(opcode == 0xd0)
	{
		// 0xd0 RNC
		// Return If No Carry
		// If the Carry bit is zero, a return operation is performed
		if(cpu.flags.C == 0)
			willReturn = true;
	}
	else if(opcode == 0xd8)
	{
		// 0xd8  RC
		// Return If Carry
		// If the Carry bit is 1, a return operation is performed
		if(cpu.flags.C == 1)
			willReturn = true;
	}

	if(willReturn)
	{
		// return address is stored in stack memory in little-endian format
		uint8_t lsb = ReadByteFromMemory((void*)&machine, cpu.SP, /*fatalOnFail*/true);
		uint8_t msb = ReadByteFromMemory((void*)&machine, cpu.SP + 1, /*fatalOnFail*/true);
		returnAddress = ((uint8_t)msb << 8) | (uint8_t)lsb;
		return true;
	}

	return false;
}

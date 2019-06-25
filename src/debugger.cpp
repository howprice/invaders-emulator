#include "debugger.h"

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

bool AddBreakpoint(Breakpoints& breakpoints, uint16_t address)
{
	if(breakpoints.breakpointCount == COUNTOF_ARRAY(breakpoints.breakpoints))
		return false;

	// #TODO: Don't add duplicates

	Breakpoint& breakpoint = breakpoints.breakpoints[breakpoints.breakpointCount++];
	breakpoint.address = address;
	breakpoint.active = true;
	return true;
}

void DeleteBreakpoint(Breakpoints& breakpoints, unsigned int index)
{
	HP_ASSERT(index < COUNTOF_ARRAY(breakpoints.breakpoints));
	HP_ASSERT(index < breakpoints.breakpointCount);

	// shuffle down to retain order
	for(unsigned int i = index; i < breakpoints.breakpointCount - 1; i++)
	{
		breakpoints.breakpoints[i] = breakpoints.breakpoints[i + 1];
	}
	breakpoints.breakpointCount--;
}

void ClearBreakpoints(Breakpoints& breakpoints)
{
	breakpoints.breakpointCount = 0;
}

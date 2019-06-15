#include "debugger.h"

#include "8080.h"

#include <stdio.h>

void PrintState(const State8080& state)
{
#if 0
	printf("A: %02X  Flags: %02X\n", state.A, state.Flags);
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
	// #TODO: Print individual flags S Z - AC - P - C
}

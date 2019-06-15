#include "debugger.h"

#include "8080.h"

#include <stdio.h>

void PrintState(const State8080& state)
{
	printf("A: %02X  Flags: %02X\n", state.A, state.Flags);
	printf("B: %02X  C: %02X\n", state.B, state.C);
	printf("D: %02X  E: %02X\n", state.D, state.E);
	printf("H: %02X  L: %02X\n", state.H, state.L);
	printf("SP: %04X\n", state.SP);
	printf("PC: %04X\n", state.PC);

	// #TODO: Print individual flags S Z - AC - P - C
}

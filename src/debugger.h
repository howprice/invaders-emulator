#ifndef HP_8080_DEBUGGER_H
#define HP_8080_DEBUGGER_H

#include <stdint.h>

struct State8080;

struct Breakpoint
{
	uint16_t address;
	bool active;
};

struct Breakpoints
{
	Breakpoint breakpoints[64];
	unsigned int breakpointCount = 0;
};

void Print8080State(const State8080& state);

bool AddBreakpoint(Breakpoints& breakpoints, uint16_t address);
void DeleteBreakpoint(Breakpoints& breakpoints, unsigned int index);
void ClearBreakpoints(Breakpoints& breakpoints);

#endif

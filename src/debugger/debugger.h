#ifndef HP_8080_DEBUGGER_H
#define HP_8080_DEBUGGER_H

#include <stdint.h>

struct State8080;
struct Machine;

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

struct Debugger
{

};

void Print8080State(const State8080& state);

void StepInto(Machine& machine, bool verbose);
void DebugStepFrame(Machine& machine, bool verbose);

bool AddBreakpoint(Breakpoints& breakpoints, uint16_t address);
void DeleteBreakpoint(Breakpoints& breakpoints, unsigned int index);
void ClearBreakpoints(Breakpoints& breakpoints);

#endif

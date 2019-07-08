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

struct Debugger
{
	Breakpoint breakpoints[64];
	unsigned int breakpointCount = 0;

	Breakpoint stepOverBreakpoint;

	bool stepOutActive = false;
	Breakpoint stepOutBreakpoint;
};

void Print8080State(const State8080& state);

void BreakMachine(Machine& machine, Debugger& debugger);
void ContinueMachine(Machine& machine);
void StepInto(Machine& machine, bool verbose);
void StepOver(Machine& machine, Debugger& debugger, bool verbose);
void StepOut(Machine& machine, Debugger& debugger, bool verbose);
void DebugStepFrame(Machine& machine, bool verbose);

bool AddBreakpoint(Debugger& debugger, uint16_t address);
void DeleteBreakpoint(Debugger& debugger, unsigned int index);
void ClearBreakpoints(Debugger& debugger);

// helpers
bool CurrentInstructionIsAReturnThatEvaluatesToTrue(const Machine& machine, uint16_t& returnAddress);

#endif

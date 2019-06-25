#pragma once

struct Breakpoints;
struct State8080;

class BreakpointsWindow
{
public:

	void Draw(Breakpoints& breakpoints, bool* p_open, const State8080& state8080);

private:

	char m_addressInputbuffer[64] = {};
	int m_selectedBreakpointIndex = -1;
};

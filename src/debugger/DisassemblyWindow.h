
#pragma once

#include "imgui/imgui.h"

#include <stdint.h>

struct Machine;
struct State8080;
struct Breakpoints;

class DisassemblyWindow
{
public:

	DisassemblyWindow();
	~DisassemblyWindow();

	void Refresh(const Machine& machine);
	void Draw(const char* title, bool* p_open, const State8080& state8080, const Breakpoints& breakpoints);

	void ScrollToPC() { m_scrollToPC = true; }

private:

	struct Line
	{
		uint16_t address;
		char* text;
	};

	void addLine(const Machine& machine, const uint16_t address);
	void clearLines();

	ImVector<Line> m_lines;
	bool  m_scrollToPC;
};

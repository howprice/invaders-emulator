
#pragma once

#include "imgui.h"

#include <stdint.h>

struct Machine;

class DisassemblyWindow
{
public:

	DisassemblyWindow();
	~DisassemblyWindow();

	void Refresh(const Machine& machine);
	void Draw(const char* title, bool* p_open);

private:

	void addLine(const char* fmt, ...) IM_FMTARGS(2);
	void addLine(const Machine& machine, const uint16_t address);
	void clearLines();

	ImVector<char*> m_lines;
	unsigned int m_pcLineNumber = 0;
	bool  m_scrollToPC;
};

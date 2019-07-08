
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
	void Update(const Machine& machine, const Breakpoints& breakpoints);

	void ScrollToPC() { m_scrollToPC = true; }

	bool IsVisible() const { return m_visible; }
	void SetVisible(bool visible) { m_visible = visible; }

private:

	struct Line
	{
		uint16_t address;
		char* text;
	};

	void addLine(const Machine& machine, const uint16_t address);
	void clearLines();

	bool m_visible = false;
	ImVector<Line> m_lines;
	bool  m_scrollToPC = false;
	bool  m_showHex = true;
};

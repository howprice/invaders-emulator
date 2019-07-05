#pragma once

struct Machine;
struct Breakpoints;

class DebugWindow
{
public:
	void Update(Machine& machine, Breakpoints& breakpoints, bool verbose);

	bool IsVisible() const { return m_visible; }
	void SetVisible(bool visible) { m_visible = visible; }

private:
	bool m_visible = false;
};

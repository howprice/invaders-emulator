#pragma once

struct Debugger;
struct State8080;

class BreakpointsWindow
{
public:

	void Update(Debugger& debugger, const State8080& state8080);

	bool IsVisible() const { return m_visible; }
	void SetVisible(bool visible) { m_visible = visible; }

private:

	bool m_visible = false;

	char m_addressInputbuffer[64] = {};
	int m_selectedBreakpointIndex = -1;
};

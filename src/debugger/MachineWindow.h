#pragma once

struct Machine;

class MachineWindow
{
public:
	void Update(Machine& machine);

	bool IsVisible() const { return m_visible; }
	void SetVisible(bool visible) { m_visible = visible; }

private:
	bool m_visible = false;
};

#pragma once

struct State8080;

class CpuWindow
{
public:
	void Update(State8080& state8080);

	bool IsVisible() const { return m_visible; }
	void SetVisible(bool visible) { m_visible = visible; }

private:
	bool m_visible = false;
};

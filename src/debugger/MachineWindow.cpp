#include "MachineWindow.h"

#include "machine.h"
#include "Helpers.h"

#include "imgui/imgui.h"

#include "SDL.h"

void MachineWindow::Update(Machine& machine)
{
	if(!m_visible)
		return;

	if(!ImGui::Begin("Machine", &m_visible))
	{
		ImGui::End();
		return;
	}

	ImGui::Checkbox("Running", &machine.running);

	if(ImGui::Button("Reset"))
		ResetMachine(&machine);

	ImGui::Text("Frame: %u", machine.frameCount);
	ImGui::Text("Frame cycle count: %u", machine.frameCycleCount);
	ImGui::Text("Scan line: %u", machine.scanLine);

	ImGui::Text("Shift register value:  %04X", machine.shiftRegisterValue);
	ImGui::Text("Shift register offset: ", machine.shiftRegisterOffset);

	ImGui::Spacing();
	ImGui::Text("DIP switches:");
	// DIP Switches
	static const char* dipSwitchNames[] =
	{
		"Lives bit 0",
		"Lives bit 1",
		"RAM/sound check",
		"Bonus life score",
		"?",
		"?",
		"?",
		"Coin display"
	};
	static_assert(COUNTOF_ARRAY(dipSwitchNames) == 8, "Array size incorrect");

	uint8_t& dipSwitchBits = machine.dipSwitchBits;
	for(unsigned int i = 0; i < 8; i++)
	{
		bool val = ((dipSwitchBits >> i) & 1) == 1 ? true : false;
		char label[32];
		SDL_snprintf(label, sizeof(label), "%u %s", i + 1, dipSwitchNames[i]); // they seem to be numbered 1..8 by convention
		if(ImGui::Checkbox(label, &val))
		{
			dipSwitchBits &= ~(1 << i); // clear bit
			if(val)
				dipSwitchBits |= (1 << i); // set bit
		}
	}

	ImGui::End();
}

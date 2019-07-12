#include "CpuWindow.h"

#include "8080.h"

#include "imgui/imgui.h"

void CpuWindow::Update(State8080& state)
{
	if(!m_visible)
		return;

	if(ImGui::Begin("CPU", &m_visible, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Clock rate: %u", State8080::kClockRate);
	
		// #TODO: Allow registers to be modified
		ImGui::Text("A: %02X  Flags: %02X %c%c%c%c%c\n",
			state.A, *(uint8_t*)&state.flags,
			state.flags.S ? 'S' : '-',
			state.flags.Z ? 'Z' : '-',
			state.flags.AC ? 'A' : '-',
			state.flags.P ? 'P' : '-',
			state.flags.C ? 'C' : '-');
		ImGui::Text("B: %02X  C: %02X\n", state.B, state.C);
		ImGui::Text("D: %02X  E: %02X\n", state.D, state.E);
		ImGui::Text("H: %02X  L: %02X\n", state.H, state.L);
		ImGui::Text("SP: %04X\n", state.SP);
		ImGui::Text("PC: %04X\n", state.PC);
		ImGui::Text("INTE: %u  HALT: %u\n", state.INTE, state.halt);
	}

	ImGui::End();
}

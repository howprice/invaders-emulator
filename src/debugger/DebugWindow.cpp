#include "DebugWindow.h"

#include "debugger.h"
#include "Machine.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

void DebugWindow::Update(Machine& machine, bool verbose)
{
	if(!m_visible)
		return;

	if(!ImGui::Begin("Debug", &m_visible, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	// Continue (F5)
	bool continueButtonEnabled = !machine.running;
	if(!continueButtonEnabled)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	if(ImGui::SmallButton("Continue") && continueButtonEnabled)
		machine.running = true;
	if(!continueButtonEnabled)
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}

	// Break (F5)
	ImGui::SameLine();
	bool breakButtonEnabled = machine.running;
	if(!breakButtonEnabled)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	if(ImGui::SmallButton("Break") && breakButtonEnabled)
		machine.running = false;
	if(!breakButtonEnabled)
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}

	// Step Into (F11)
	ImGui::SameLine();
	bool stepIntoButtonEnabled = !machine.running;
	if(!stepIntoButtonEnabled)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	if(ImGui::SmallButton("Step Into") && stepIntoButtonEnabled)
	{
		StepInto(machine, verbose);
	}
	if(!stepIntoButtonEnabled)
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}

	// Step Frame (F8)
	ImGui::SameLine();
	bool stepFrameButtonEnabled = !machine.running;
	if(!stepFrameButtonEnabled)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	if(ImGui::SmallButton("Step Frame") && stepFrameButtonEnabled)
	{
		DebugStepFrame(machine, verbose);
	}
	if(!stepFrameButtonEnabled)
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}

	ImGui::End();
}

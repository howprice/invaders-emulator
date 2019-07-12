#include "DebugWindow.h"

#include "debugger.h"
#include "machine.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

void DebugWindow::Update(Machine& machine, Debugger& debugger, bool verbose)
{
	if(!m_visible)
		return;

	if(!ImGui::Begin("Debug", &m_visible, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	// Continue (F5)
	bool buttonEnabled = !machine.running;
	if(!buttonEnabled)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	if(ImGui::SmallButton("Continue") && buttonEnabled)
		ContinueMachine(machine);
	if(!buttonEnabled)
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}
	if(ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted("Continue (F5)");
		ImGui::EndTooltip();
	}

	// Break (F5)
	ImGui::SameLine();
	buttonEnabled = machine.running;
	if(!buttonEnabled)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	if(ImGui::SmallButton("Break") && buttonEnabled)
	{
		BreakMachine(machine, debugger);
	}
	if(!buttonEnabled)
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}
	if(ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted("Break (F5)");
		ImGui::EndTooltip();
	}

	// Step Into (F11)
	ImGui::SameLine();
	buttonEnabled = !machine.running;
	if(!buttonEnabled)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	if(ImGui::SmallButton("Step Into") && buttonEnabled)
	{
		StepInto(machine, verbose);
	}
	if(!buttonEnabled)
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}
	if(ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted("Step Into (F11)");
		ImGui::EndTooltip();
	}

	// Step Over (F10)
	ImGui::SameLine();
	buttonEnabled = !machine.running;
	if(!buttonEnabled)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	if(ImGui::SmallButton("Step Over") && buttonEnabled)
	{
		StepOver(machine, debugger, verbose);
	}
	if(!buttonEnabled)
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}
	if(ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted("Step Over (F10)");
		ImGui::EndTooltip();
	}

	// Step Out (Shift+F11)
	ImGui::SameLine();
	buttonEnabled = !machine.running;
	if(!buttonEnabled)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	if(ImGui::SmallButton("Step Out ") && buttonEnabled)
	{
		StepOut(machine, debugger, verbose);
	}
	if(!buttonEnabled)
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}
	if(ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted("Step Out (Shift+F11)");
		ImGui::EndTooltip();
	}

	// Step Frame (F8)
	ImGui::SameLine();
	buttonEnabled = !machine.running;
	if(!buttonEnabled)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	if(ImGui::SmallButton("Step Frame") && buttonEnabled)
	{
		DebugStepFrame(machine, verbose);
	}
	if(!buttonEnabled)
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}
	if(ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted("Step Frame (F8)");
		ImGui::EndTooltip();
	}

	ImGui::End();
}

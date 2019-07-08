
#include "BreakpointsWindow.h"

#include "debugger.h"
#include "machine.h"
#include "Assert.h"
#include "Helpers.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <stdlib.h>

#ifdef _MSC_VER
#define _PRISizeT   "I"
#define ImSnprintf  _snprintf
#else
#define _PRISizeT   "z"
#define ImSnprintf  snprintf
#endif

void BreakpointsWindow::Update(Debugger& debugger, const State8080& state8080)
{
	if(!m_visible)
		return;

	ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Breakpoints", &m_visible))
	{
		ImGui::End();
		return;
	}

	// New button
	bool newButtonEnabled = debugger.breakpointCount < COUNTOF_ARRAY(debugger.breakpoints);
	if(!newButtonEnabled)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	if(ImGui::SmallButton("New"))
	{
		uint16_t address = state8080.PC;
		if(AddBreakpoint(debugger, address))
		{
			HP_ASSERT(debugger.breakpointCount > 0);
			m_selectedBreakpointIndex = (int)debugger.breakpointCount - 1;
		}
	}
	if(!newButtonEnabled)
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}

	// Delete button
	bool deleteButtonEnabled = m_selectedBreakpointIndex >= 0;
	if(!deleteButtonEnabled)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	ImGui::SameLine();
	if(ImGui::SmallButton("Delete"))
	{
		HP_ASSERT(m_selectedBreakpointIndex >= 0 && m_selectedBreakpointIndex < (int)debugger.breakpointCount);
		DeleteBreakpoint(debugger, (unsigned int)m_selectedBreakpointIndex);
		m_selectedBreakpointIndex = -1;
	}
	if(!deleteButtonEnabled)
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}

	// Delete All button
	ImGui::SameLine();
	if(ImGui::SmallButton("Delete All"))
	{
		ClearBreakpoints(debugger);
		m_selectedBreakpointIndex = -1;
	}

	// Breakpoint address entry
	ImGui::SameLine();
	ImGui::PushItemWidth(60.0f);
	if(ImGui::InputText("##address", m_addressInputbuffer, sizeof(m_addressInputbuffer), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue))
	{
		size_t address;
		if(sscanf(m_addressInputbuffer, "%" _PRISizeT "X", &address) == 1 && address < UINT16_MAX)
		{
			HP_ASSERT(m_selectedBreakpointIndex >= 0 && m_selectedBreakpointIndex < (int)debugger.breakpointCount);
			Breakpoint& breakpoint = debugger.breakpoints[m_selectedBreakpointIndex];
			breakpoint.address = (uint16_t)address;
		}
		m_addressInputbuffer[0] = '\0';
	}
	ImGui::PopItemWidth();

	ImGui::Separator();

	ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing

	for(unsigned int breakpointIndex = 0; breakpointIndex < debugger.breakpointCount; breakpointIndex++)
	{
		Breakpoint& breakpoint = debugger.breakpoints[breakpointIndex];
		ImGui::PushID(breakpointIndex);

		bool isSelectedBreakpoint = m_selectedBreakpointIndex == (int)breakpointIndex;

		ImGui::Checkbox("##Active", &breakpoint.active);

		if(isSelectedBreakpoint)
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
		ImGui::SameLine();
		ImGui::Text("%04X", breakpoint.address);
		if(isSelectedBreakpoint)
			ImGui::PopStyleColor();

		ImGui::SameLine();
		if(isSelectedBreakpoint)
		{
			ImGui::TextUnformatted(" *");
		}
		else
		{
			if(ImGui::SmallButton("Select"))
				m_selectedBreakpointIndex = (int)breakpointIndex;
		}

		ImGui::PopID();
	}

	ImGui::PopStyleVar();
	ImGui::EndChild(); // ScrollingRegion

	ImGui::End();
}

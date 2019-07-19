
#include "DisassemblyWindow.h"

#include "debugger.h"
#include "machine.h"
#include "hp_assert.h"
#include "Helpers.h"

#include "SDL.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

// Portable helpers
static int   Stricmp(const char* str1, const char* str2) { int d; while((d = toupper(*str2) - toupper(*str1)) == 0 && *str1) { str1++; str2++; } return d; }
static int   Strnicmp(const char* str1, const char* str2, int n) { int d = 0; while(n > 0 && (d = toupper(*str2) - toupper(*str1)) == 0 && *str1) { str1++; str2++; n--; } return d; }
static char* Strdup(const char* str) { size_t len = strlen(str) + 1; void* buf = malloc(len); IM_ASSERT(buf); return (char*)memcpy(buf, (const void*)str, len); }
static void  Strtrim(char* str) { char* str_end = str + strlen(str); while(str_end > str && str_end[-1] == ' ') str_end--; *str_end = 0; }

DisassemblyWindow::DisassemblyWindow()
{
	clearLines();
}

DisassemblyWindow::~DisassemblyWindow()
{
	clearLines();
}

void DisassemblyWindow::clearLines()
{
	for(int i = 0; i < m_lines.Size; i++)
	{
		free(m_lines[i].text);
	}
	m_lines.clear();
}


void DisassemblyWindow::addLine(const Machine& machine, const uint16_t address)
{
	char text[64] = {};
	char temp[64] = {};

	const uint8_t* pMemory = machine.pMemory;
	HP_ASSERT(pMemory);
	HP_ASSERT(address < machine.memorySizeBytes);

	const uint8_t* pInstruction = pMemory + address; // #TODO: Use accessor for safety
	const uint8_t opcode = *pInstruction;
	const char* mnemonic = GetInstructionMnemonic(opcode);
	const unsigned int instructionSizeBytes = GetInstructionSizeBytes(opcode);

	// address
	SDL_snprintf(temp, sizeof(temp), "0x%04X  ", address);
	SDL_strlcat(text, temp, sizeof(text));

	// hex
	if(m_showHex)
	{
		for(unsigned int byteIndex = 0; byteIndex < instructionSizeBytes; byteIndex++)
		{
			SDL_snprintf(temp, sizeof(temp), "%02X ", *(pInstruction + byteIndex));
			SDL_strlcat(text, temp, sizeof(text));
		}
		for(unsigned int byteIndex = instructionSizeBytes; byteIndex < kMaxInstructionSizeBytes; byteIndex++)
		{
			SDL_snprintf(temp, sizeof(temp), "   ");
			SDL_strlcat(text, temp, sizeof(text));
		}
	}

	// mnemonic
	if(instructionSizeBytes == 1)
		SDL_snprintf(temp, sizeof(temp), "%s", mnemonic);
	else if(instructionSizeBytes == 2)
	{
		uint8_t d8 = *(pInstruction + 1);
		SDL_snprintf(temp, sizeof(temp), mnemonic, d8);
	}
	else if(instructionSizeBytes == 3)
	{
		uint8_t lsb = *(pInstruction + 1);
		uint8_t msb = *(pInstruction + 2);
		uint16_t val = (msb << 8) | lsb;
		SDL_snprintf(temp, sizeof(temp), mnemonic, val);
	}

//	ImGui::Text(" %-14s", temp); TODO: What is the -14 for again?

	SDL_strlcat(text, temp, sizeof(text));

	Line line;
	line.address = address;
	line.text = Strdup(text);
	m_lines.push_back(line);
}

void DisassemblyWindow::Refresh(const Machine& machine)
{
	clearLines();

	const uint8_t* pMemory = machine.pMemory;
	HP_ASSERT(pMemory);
	uint16_t address = 0;
	while(address < 0x2000)
	{
		HP_ASSERT(address < machine.memorySizeBytes);
		const uint8_t* pInstruction = pMemory + address; // #TODO: Use accessor for safety
		const uint8_t opcode = *pInstruction;
		addLine(machine, address);
		address += (uint16_t)GetInstructionSizeBytes(opcode);
	}
}

void DisassemblyWindow::Update(const Machine& machine, const Debugger& debugger)
{
	if(!m_visible)
		return;

	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Disassembly", &m_visible))
	{
		ImGui::End();
		return;
	}

	ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

	if(ImGui::BeginPopupContextWindow())
	{
		if(ImGui::Selectable("Show Next Instruction"))
			m_scrollToPC = true;

		if(ImGui::Checkbox("Show Hex", &m_showHex))
		{
			Refresh(machine);
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	// Display every line as a separate entry so we can change their color or add custom widgets. If you only want raw text you can use ImGui::TextUnformatted(log.begin(), log.end());
	// NB- if you have thousands of entries this approach may be too inefficient and may require user-side clipping to only process visible items.
	// You can seek and display only the lines that are visible using the ImGuiListClipper helper, if your elements are evenly spaced and you have cheap random access to the elements.
	// To use the clipper we could replace the 'for (int i = 0; i < Items.Size; i++)' loop with:
	//     ImGuiListClipper clipper(Items.Size);
	//     while (clipper.Step())
	//         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing

	for(int lineIndex = 0; lineIndex < m_lines.Size; lineIndex++)
	{
		const Line& line = m_lines[lineIndex];

		bool pcAtLine = line.address == machine.cpu.PC;
		bool breakpointAtLine = false;
		bool breakpointActive = false;
		for(unsigned int breakpointIndex = 0; breakpointIndex < debugger.breakpointCount; breakpointIndex++)
		{
			const Breakpoint& breakpoint = debugger.breakpoints[breakpointIndex];
			if(breakpoint.address == line.address)
			{
				breakpointAtLine = true;
				breakpointActive = breakpoint.active;
				break;
			}
		}

		if(pcAtLine || breakpointAtLine)
			ImGui::Text("%c%c", pcAtLine ? '>' : ' ', breakpointAtLine ? (breakpointActive ? '*' : 'o') : ' ');
		else
			ImGui::TextUnformatted("  ");

		ImGui::SameLine();
		ImGui::TextUnformatted(line.text);

		if(pcAtLine && m_scrollToPC)
		{
			ImGui::SetScrollHereY();
			m_scrollToPC = false;
		}
	}

	ImGui::PopStyleVar();
	ImGui::EndChild(); // ScrollingRegion

	// #TODO: Add Hex-editor window like start address, which when pressed disassembles starting at that line

	ImGui::End();
}

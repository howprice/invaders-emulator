#include "debugger/DebugWindow.h"
#include "debugger/MachineWindow.h"
#include "debugger/CpuWindow.h"
#include "debugger/DisassemblyWindow.h"
#include "debugger/BreakpointsWindow.h"
#include "debugger/imgui_memory_editor.h"
#include "debugger/debugger.h"

#include "machine.h"
#include "8080.h"
#include "Renderer.h"
#include "Display.h"
#include "Input.h"
#include "Audio.h"
#include "Helpers.h"
#include "hp_assert.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_opengl3.h"

#include "SDL.h"
#include "SDL_mixer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool s_startDebugging = false;
static bool s_verbose = false;
static unsigned int s_zoom = 3;
static bool s_rotateDisplay = true; // the invaders machine display is rotated 90 degrees anticlockwise

static bool s_showDevUI = false;
static bool s_showMenuBar = true;
static CpuWindow s_cpuWindow;
static MachineWindow s_machineWindow;
static DebugWindow s_debugWindow;
static DisassemblyWindow s_disassemblyWindow;

static bool s_showMemoryEditor = false;
static uint16_t s_memoryWindowAddress = 0x0000;
static MemoryEditor s_memoryEditor;

static Debugger s_debugger; // #TODO: This may become DebuggerContext
static BreakpointsWindow s_breakpointsWindow;

static Machine* s_pSavedMachine = nullptr;
static bool s_stateSaved = false;

static void printUsage()
{
	puts("Usage: invaders [OPTIONS]");
	puts("Options:\n\n"
		"  --help                       Shows this message\n"
		"  -d or --debug                Start in debugger\n"
		"  --verbose                    \n"
		"  -z --zoom                    Pixel zoom (scale). Default = 3\n"
//		"  -r or --rotate-display       Rotate display 90 degrees anticlockwise\n"
	);
}

static void parseCommandLine(int argc, char** argv)
{
	for(int i = 1; i < argc; i++)
	{
		const char* arg = argv[i];

		if(strcmp(arg, "--help") == 0)
		{
			printUsage();
			exit(EXIT_SUCCESS);
		}
		else if(strcmp(arg, "-d") == 0 || strcmp(arg, "--debug") == 0)
		{
			s_startDebugging = true;
		}
		else if(strcmp(arg, "--verbose") == 0)
		{
			s_verbose = true;
		}
		else if(strcmp(arg, "-z") == 0 || strcmp(arg, "--zoom") == 0)
		{
			if(i + 1 == argc)
			{
				printUsage();
				exit(EXIT_FAILURE);
			}

			arg = argv[++i];
			if(!ParseUnsignedInt(arg, s_zoom))
			{
				fprintf(stderr, "ERROR: Specified zoom is invalid\n");
				printUsage();
				exit(EXIT_FAILURE);
			}
		}
		else if(strcmp(arg, "-r") == 0 || strcmp(arg, "--rotate-display") == 0)
		{
			s_rotateDisplay = true;
		}
		else
		{
			fprintf(stderr, "Unrecognised command line arg: %s\n", arg);
			printUsage();
			exit(EXIT_FAILURE);
		}
	}
}

static void restoreMachineState(Machine& machine)
{
	HP_ASSERT(s_stateSaved);
	HP_ASSERT(s_pSavedMachine);

	machine.running = s_pSavedMachine->running;
	machine.cpu = s_pSavedMachine->cpu;

	// restore memory
	// #TODO: Only restore RAM
	SDL_memcpy(machine.pMemory, s_pSavedMachine->pMemory, machine.memorySizeBytes);

	machine.frameCount = s_pSavedMachine->frameCount;
	machine.frameCycleCount = s_pSavedMachine->frameCycleCount;
	machine.scanLine = s_pSavedMachine->scanLine;

	machine.shiftRegisterValue = s_pSavedMachine->shiftRegisterValue;
	machine.shiftRegisterOffset = s_pSavedMachine->shiftRegisterOffset;

	machine.prevOut3 = s_pSavedMachine->prevOut3;
	machine.prevOut5 = s_pSavedMachine->prevOut3;

	machine.dipSwitchBits = s_pSavedMachine->dipSwitchBits;
}

static void saveMachineState(const Machine& machine)
{
	s_pSavedMachine->running = machine.running;
	s_pSavedMachine->cpu = machine.cpu;

	// save memory
	// #TODO: Only save RAM
	SDL_memcpy(s_pSavedMachine->pMemory, machine.pMemory, machine.memorySizeBytes);

	s_pSavedMachine->frameCount = machine.frameCount;
	s_pSavedMachine->frameCycleCount = machine.frameCycleCount;
	s_pSavedMachine->scanLine = machine.scanLine;

	s_pSavedMachine->shiftRegisterValue = machine.shiftRegisterValue;
	s_pSavedMachine->shiftRegisterOffset = machine.shiftRegisterOffset;

	s_pSavedMachine->prevOut3 = machine.prevOut3;
	s_pSavedMachine->prevOut5 = machine.prevOut3;

	s_pSavedMachine->dipSwitchBits = machine.dipSwitchBits;
	
	s_stateSaved = true;
}

//------------------------------------------------------------------------------

static void doMenuBar(Machine* pMachine)
{
	if(!s_showMenuBar)
		return;

	if(ImGui::BeginMainMenuBar() == false)
		return;
	
	if(ImGui::BeginMenu("Machine"))
	{
		if(ImGui::MenuItem("Reset", "F3"))
			ResetMachine(pMachine);

		if(ImGui::MenuItem("Restore State", "F7", /*pSelected*/nullptr, /*enabled*/s_stateSaved))
			restoreMachineState(*pMachine);

		if(ImGui::MenuItem("Save State", "Shift+F7"))
			saveMachineState(*pMachine);

		ImGui::EndMenu();
	}

	Display* pDisplay = pMachine->pDisplay;
	bool displayMenuEnabled = pDisplay != nullptr;
	if(ImGui::BeginMenu("Display", displayMenuEnabled))
	{
		bool bilinearSampling = pDisplay->GetBilinearSampling();
		if(ImGui::MenuItem("Bilinear sampling?", /*shorcut*/nullptr, /*pSelected*/&bilinearSampling))
			pDisplay->SetBilinearSampling(bilinearSampling);

		// #TODO: Zoom
		// #TODO: Fullscreen
		// #TODO: Vsync
		ImGui::EndMenu();
	}

	if(ImGui::BeginMenu("Debug"))
	{
		if(ImGui::MenuItem("Continue", /*shortcut*/nullptr, /*pSelected*/nullptr, /*enabled*/!pMachine->running))
			ContinueMachine(*pMachine);

		if(ImGui::MenuItem("Break", /*shortcut*/nullptr, /*pSelected*/nullptr, /*enabled*/pMachine->running))
			BreakMachine(*pMachine, s_debugger);

		ImGui::Separator();
		if(ImGui::MenuItem("Step Frame", "F8", /*pSelcted*/nullptr, /*enabled*/!pMachine->running))
			DebugStepFrame(*pMachine, s_verbose);

		if(ImGui::MenuItem("Step Into", "F11", /*pSelcted*/nullptr, /*enabled*/!pMachine->running))
			StepInto(*pMachine, s_verbose);

		if(ImGui::MenuItem("Step Over", "F10", /*pSelcted*/nullptr, /*enabled*/!pMachine->running))
			StepOver(*pMachine, s_debugger, s_verbose);

		if(ImGui::MenuItem("Step Out", "Shift+F11", /*pSelcted*/nullptr, /*enabled*/!pMachine->running))
			StepOut(*pMachine, s_debugger, s_verbose);

		ImGui::EndMenu();
	}
	if(ImGui::BeginMenu("Windows"))
	{
		bool visible;

		visible = s_machineWindow.IsVisible();
		if(ImGui::MenuItem("Machine", nullptr, &visible))
			s_machineWindow.SetVisible(visible);

		visible = s_cpuWindow.IsVisible();
		if(ImGui::MenuItem("CPU", nullptr, &visible))
			s_cpuWindow.SetVisible(visible);

		visible = s_debugWindow.IsVisible();
		if(ImGui::MenuItem("Debug", nullptr, &visible))
			s_debugWindow.SetVisible(visible);

		visible = s_disassemblyWindow.IsVisible();
		if(ImGui::MenuItem("Disassembly", nullptr, &visible))
			s_disassemblyWindow.SetVisible(visible);

		visible = s_breakpointsWindow.IsVisible();
		if(ImGui::MenuItem("Breakpoints", nullptr, &visible))
			s_breakpointsWindow.SetVisible(visible);

		ImGui::MenuItem("Memory", nullptr, &s_showMemoryEditor);

		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();
}

static void doMemoryWindow(Machine* pMachine)
{
	HP_ASSERT(pMachine);

	if(!s_showMemoryEditor)
		return;

	ImGui::Begin("Memory Editor", &s_showMemoryEditor);
	s_memoryEditor.DrawContents(pMachine->pMemory, pMachine->memorySizeBytes, s_memoryWindowAddress); // create a window and draw memory editor (if you already have a window, use DrawContents())
	ImGui::End();
}

static void doDevUI(Machine* pMachine)
{
	const bool shiftDown = Input::GetKeyState(SDL_SCANCODE_LSHIFT) || Input::GetKeyState(SDL_SCANCODE_RSHIFT);

	if(Input::IsKeyDownThisFrame(SDL_SCANCODE_TAB))
		s_showDevUI = !s_showDevUI;
	else if(Input::IsKeyDownThisFrame(SDL_SCANCODE_F3))
		ResetMachine(pMachine);
	else if(Input::IsKeyDownThisFrame(SDL_SCANCODE_F5))
		pMachine->running = !pMachine->running;
	else if(Input::IsKeyDownThisFrame(SDL_SCANCODE_F7))
	{
		if(shiftDown)
			saveMachineState(*pMachine);
		else if(s_stateSaved)
			restoreMachineState(*pMachine);
	}
	else if(Input::IsKeyDownThisFrame(SDL_SCANCODE_F8))
		DebugStepFrame(*pMachine, s_verbose);
	else if(Input::IsKeyDownThisFrame(SDL_SCANCODE_F10))
	{
		if(!pMachine->running)
			StepOver(*pMachine, s_debugger, s_verbose);
	}
	else if(Input::IsKeyDownThisFrame(SDL_SCANCODE_F11))
	{
		if(shiftDown)
		{
			if(!pMachine->running)
				StepOut(*pMachine, s_debugger, s_verbose);
		}
		else
		{
			if(!pMachine->running)
				StepInto(*pMachine, s_verbose);
		}
	}


	if(!s_showDevUI)
		return;

	doMenuBar(pMachine);
	s_cpuWindow.Update(pMachine->cpu);
	s_machineWindow.Update(*pMachine);
	s_debugWindow.Update(*pMachine, s_debugger, s_verbose);
	s_disassemblyWindow.Update(*pMachine, s_debugger);
	s_breakpointsWindow.Update(s_debugger, pMachine->cpu);
	doMemoryWindow(pMachine);
}

static void debugHook(Machine* pMachine)
{
	HP_ASSERT(pMachine);
	
	if(s_verbose)
	{
		Disassemble8080(pMachine->pMemory, pMachine->memorySizeBytes, pMachine->cpu.PC);
		printf("    ");
		Print8080State(pMachine->cpu);

		// #TODO: Print scanline number
	}

	for(unsigned int breakpointIndex = 0; breakpointIndex < s_debugger.breakpointCount; breakpointIndex++)
	{
		const Breakpoint& breakpoint = s_debugger.breakpoints[breakpointIndex];
		if(breakpoint.active && breakpoint.address == pMachine->cpu.PC)
		{
			BreakMachine(*pMachine, s_debugger);
			break;
		}
	}

	if(s_debugger.stepOverBreakpoint.active && s_debugger.stepOverBreakpoint.address == pMachine->cpu.PC)
	{
		BreakMachine(*pMachine, s_debugger);
	}

	if(s_debugger.stepOutActive)
	{
		if(s_debugger.stepOutBreakpoint.active)
		{
			// we've just hit a return so expect to break on next instruction
			HP_ASSERT(pMachine->cpu.PC == s_debugger.stepOutBreakpoint.address);
			BreakMachine(*pMachine, s_debugger);
		}
		else
		{ 
			uint16_t returnAddress;
			if(CurrentInstructionIsAReturnThatEvaluatesToTrue(*pMachine, returnAddress))
			{
				s_debugger.stepOutBreakpoint.address = returnAddress;
				s_debugger.stepOutBreakpoint.active = true;
			}
		}
	}

	if(!pMachine->running)
		s_disassemblyWindow.ScrollToPC();
}

int main(int argc, char** argv)
{
	parseCommandLine(argc, argv);

	if(SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		fprintf(stderr, "SDL failed to initialise: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	Input::Init();

	// SDL2_mixer
	unsigned int sampleRate = 11025; // from 1.wav
	Uint16 audioFormat = AUDIO_U8; // from 1.wav
	if(Mix_OpenAudio(sampleRate, audioFormat, /*numChannels*/1, /*chunkSize*/512) < 0)
	{
		fprintf(stderr, "Failed to initialise SDL2_Mixer: %s\n", Mix_GetError());
		return EXIT_FAILURE;
	}

	InitGameAudio();

	// Decide GL+GLSL versions
#if __APPLE__
	// GL 3.2 Core + GLSL 150
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
	// GL 3.0 + GLSL 130
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

	Display* pDisplay = Display::Create(Machine::kDisplayWidth, Machine::kDisplayHeight, s_zoom, s_rotateDisplay, /*bFullscreen*/false);
	if(!pDisplay)
	{
		fprintf(stderr, "Failed to create display window\n");
		return EXIT_FAILURE;
	}

	if(!Renderer::Init())
	{
		fprintf(stderr, "Failed to initialise renderer\n");
		return EXIT_FAILURE;
	}

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// Setup Platform/Renderer bindings
	ImGui_ImplSDL2_InitForOpenGL(pDisplay->GetWindow(), pDisplay->GetGLContext());

	ImGui_ImplOpenGL3_Init();

	Machine* pMachine = nullptr;
	if(!CreateMachine(&pMachine, pDisplay))
	{
		fprintf(stderr, "Failed to create Machine\n");
		return EXIT_FAILURE;
	}

	pMachine->debugHook = debugHook;

	if(!CreateMachine(&s_pSavedMachine, nullptr))
	{
		fprintf(stderr, "Failed to create save state\n");
		return EXIT_FAILURE;
	}

	s_disassemblyWindow.Refresh(*pMachine);

	bool bDone = false;
	uint64_t frameIndex = 0;
	while(!bDone)
	{
		Input::FrameStart();
		
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);

			if(event.type == SDL_QUIT)
			{
				bDone = true;
			}
			else if(event.type == SDL_KEYDOWN)
			{
				Input::OnKeyDown(event.key);

				if(event.key.keysym.sym == SDLK_ESCAPE)
					bDone = true;
			}
			else if(event.type == SDL_KEYUP)
				Input::OnKeyUp(event.key);
			else if(event.type == SDL_JOYAXISMOTION)
				Input::OnJoyAxisMotion(event.jaxis);
			else if(event.type == SDL_JOYHATMOTION)
				Input::OnJoyHatMotion(event.jhat);
			else if(event.type == SDL_JOYDEVICEADDED)
				Input::OnJoyDeviceAdded(event.jdevice);
			else if(event.type == SDL_JOYDEVICEREMOVED)
				Input::OnJoyDeviceRemoved(event.jdevice);
			else if(event.type == SDL_JOYBUTTONDOWN)
				Input::OnJoyButtonDown(event.jbutton);
			else if(event.type == SDL_JOYBUTTONUP)
				Input::OnJoyButtonUp(event.jbutton);
			else if(event.type == SDL_CONTROLLERBUTTONDOWN)
				Input::OnControllerButtonDown(event.cbutton);
			else if(event.type == SDL_CONTROLLERBUTTONUP)
				Input::OnControllerButtonUp(event.cbutton);
			else if(event.type == SDL_CONTROLLERAXISMOTION)
				Input::OnControllerAxisMotion(event.caxis);
			else if(event.type == SDL_CONTROLLERDEVICEADDED)
				Input::OnControllerDeviceAdded(event.cdevice);
			else if(event.type == SDL_CONTROLLERDEVICEREMOVED)
				Input::OnControllerDeviceRemoved(event.cdevice);
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(pDisplay->GetWindow());
		ImGui::NewFrame();

		StepFrame(pMachine, s_verbose);

		//ImGui::ShowDemoWindow();
		doDevUI(pMachine);

		// Rendering
		ImGui::Render();
		pDisplay->Clear();
		pDisplay->Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		pDisplay->Present();
		frameIndex++;
	}

	DestroyMachine(pMachine);
	pMachine = nullptr;

	DestroyMachine(s_pSavedMachine);
	s_pSavedMachine = nullptr;

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	Renderer::Shutdown();
	Display::Destroy(pDisplay);
	pDisplay = nullptr;

	// SDL2_mixer
	ShutdownGameAudio();
	Mix_CloseAudio();

	Input::Shutdown();

	SDL_Quit();

	return 0;
}

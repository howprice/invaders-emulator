
#include "debugger/DebugWindow.h"
#include "debugger/MachineWindow.h"
#include "debugger/CpuWindow.h"
#include "debugger/DisassemblyWindow.h"
#include "debugger/BreakpointsWindow.h"
#include "debugger/imgui_memory_editor.h"
#include "debugger/debugger.h"

#include "machine.h"
#include "8080.h"
#include "Input.h"
#include "Audio.h"
#include "Helpers.h"
#include "Assert.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_opengl3.h"

#include "SDL.h"
#include "SDL_mixer.h"

// About OpenGL function loaders: modern OpenGL doesn't have a standard header file and requires individual function pointers to be loaded manually.
// Helper libraries are often used for this purpose! Here we are supporting a few common ones: gl3w, glew, glad.
// You may use another loader/header of your choice (glext, glLoadGen, etc.), or chose to manually implement your own.
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>    // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>    // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>  // Initialize with gladLoadGL()
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool s_startDebugging = false;
static bool s_verbose = false;
static bool s_rotateDisplay = true; // the invaders machine display is rotated 90 degrees anticlockwise

static GLuint s_vertexShader = 0;
static GLuint s_fragmentShader = 0;
static GLuint s_program = 0;
static GLuint s_texture = 0;
static GLuint s_pointSampler = 0;
static GLuint s_bilinearSampler = 0;
static GLuint s_selectedSampler = 0;
static GLuint s_vao = 0;

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

static void printUsage()
{
	puts("Usage: invaders [OPTIONS]");
	puts("Options:\n\n"
		"  --help                       Shows this message\n"
		"  -d or --debug                Start in debugger\n"
		"  --verbose                    \n"
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

static const char s_vertexShaderSource[] =
	"#version 130\n"
	"// Generate single triangle in homogenous clip space that, when clipped, fills the screen\n"
	"out vec2 texCoord;\n"
	"\n"
	"void main()\n"
	"{\n"
	"\n"
	"	// calculate UVs such that screen space [0,1] is covered by triangle and UVs are correct (draw a picture)\n"
	"\n"
	"    vec2 position;"
	"    if(gl_VertexID == 0)\n"
	"    {\n"
	"        position = vec2( -1.0f, 1.0f );\n"
	"        texCoord = vec2( 0.0f, 0.0f );\n"
	"    }\n"
	"    else if(gl_VertexID == 1)\n"
	"    {\n"
	"        position = vec2( -1.0f, -3.0f );\n"
	"        texCoord = vec2(  0.0f, 2.0f );\n"
	"    }\n"
	"    else\n"
	"    {\n"
	"        position = vec2( 3.0f, 1.0f );\n"
	"        texCoord = vec2( 2.0f, 0.0f );\n"
	"    }\n"
	"	 vec4 posPS = vec4(position, 0.0f, 1.0f);\n"
	"	 gl_Position = posPS;\n"
	"}\n";

static const char s_fragmentShaderSource[] =
	"#version 130\n"
	"uniform sampler2D sampler0;\n"
	"in vec2 texCoord;\n"
	"out vec4 Out_Color;\n"
	"void main()\n"
	"{\n"
	"	float r = texture2D(sampler0, texCoord).r; // 1 channel texture\n"
	"	Out_Color = vec4(r,r,r,1);\n"
	"}\n";

static void CheckShader(GLuint shader)
{
	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if(status == GL_FALSE)
	{
		fprintf(stderr, "Failed to compile shader\n");
		GLint logLength;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		if(logLength > 0)
		{
			GLchar szLog[1024];
			GLsizei length;
			glGetShaderInfoLog(shader, sizeof(szLog), &length, szLog);
			fprintf(stderr, "%s", szLog);
		}
		glDeleteShader(shader);
		HP_FATAL_ERROR("Shader compilation failed");
	}
}

static void CheckProgram(GLuint program)
{
	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if(status == GL_FALSE)
	{
		GLint infoLogLength;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);

		GLchar* strInfoLog = new GLchar[infoLogLength + 1];
		glGetProgramInfoLog(program, infoLogLength, NULL, strInfoLog);
		HP_FATAL_ERROR("GL linker failure: %s\n", strInfoLog);
	}
}

static void createOpenGLObjects(unsigned int textureWidth, unsigned int textureHeight)
{
	s_vertexShader = glCreateShader(GL_VERTEX_SHADER);
	const GLchar* vertexShaderSources[] = { s_vertexShaderSource };
	glShaderSource(s_vertexShader, COUNTOF_ARRAY(vertexShaderSources), vertexShaderSources, NULL);
	glCompileShader(s_vertexShader); // no return value
	CheckShader(s_vertexShader);

	s_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	const GLchar* fragmentShaderSources[] = { s_fragmentShaderSource };
	glShaderSource(s_fragmentShader, COUNTOF_ARRAY(fragmentShaderSources), fragmentShaderSources, NULL);
	glCompileShader(s_fragmentShader); // no return value
	CheckShader(s_fragmentShader);

	s_program = glCreateProgram();
	glAttachShader(s_program, s_vertexShader);
	glAttachShader(s_program, s_fragmentShader);
	glLinkProgram(s_program);
	CheckProgram(s_program);

	// No need to keep the shaders attached now that the program is linked
	glDetachShader(s_program, s_vertexShader);
	glDetachShader(s_program, s_fragmentShader);

	// When drawing "A non-zero Vertex Array Object must be bound (though no arrays have to be enabled, so it can be a freshly-created vertex array object)."
	// https://devtalk.nvidia.com/default/topic/561172/opengl/gldrawarrays-without-vao-for-procedural-geometry-using-gl_vertexid-doesn-t-work-in-debug-context/#
	glGenVertexArrays(1, &s_vao);
	HP_ASSERT(s_vao != 0);

	// display texture
	glGenTextures(1, &s_texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s_texture);

	glTexStorage2D(GL_TEXTURE_2D, 1/*mipLevels*/, GL_R8, textureWidth, textureHeight);

	glGenSamplers(1, &s_pointSampler);
	glBindSampler(0, s_pointSampler);
	glSamplerParameteri(s_pointSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glSamplerParameteri(s_pointSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glGenSamplers(1, &s_bilinearSampler);
	glBindSampler(1, s_bilinearSampler);
	glSamplerParameteri(s_bilinearSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glSamplerParameteri(s_bilinearSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	s_selectedSampler = s_bilinearSampler;
}

static void deleteOpenGLObjects()
{
	glDeleteProgram(s_program);
	s_program = 0;

	glDeleteShader(s_vertexShader);
	s_vertexShader = 0;

	glDeleteShader(s_fragmentShader);
	s_fragmentShader = 0;

	glDeleteVertexArrays(1, &s_vao);
	s_vao = 0;

	glDeleteTextures(1, &s_texture);
	s_texture = 0;

	glDeleteSamplers(1, &s_pointSampler);
	s_pointSampler = 0;

	glDeleteSamplers(1, &s_bilinearSampler);
	s_bilinearSampler = 0;
}

//------------------------------------------------------------------------------

static Machine* s_pSavedMachine = nullptr;
static bool s_stateSaved = false;

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

	if(ImGui::BeginMenu("Display"))
	{
		bool bilinearSampling = s_selectedSampler == s_bilinearSampler;
		if(ImGui::MenuItem("Bilinear sampling?", /*shorcut*/nullptr, /*pSelected*/&bilinearSampling))
			s_selectedSampler = bilinearSampling ? s_bilinearSampler : s_pointSampler;

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

static void updateDisplayTexture(const Machine* pMachine, unsigned int textureWidth, unsigned int textureHeight)
{
	if(s_rotateDisplay)
	{
		HP_ASSERT(textureHeight == Machine::kDisplayWidth && textureWidth == Machine::kDisplayHeight);
	}
	else
	{
		HP_ASSERT(textureWidth == Machine::kDisplayWidth && textureHeight == Machine::kDisplayHeight);
	}

	const uint8_t* pDisplayBuffer = pMachine->pDisplayBuffer; // src - 1 bit per pixel
	static uint8_t s_texturePixelsR8[Machine::kDisplayWidth * Machine::kDisplayHeight]; // dst - 1 byte per pixel
	
	const unsigned int srcBytesPerRow = Machine::kDisplayWidth >> 3; // div 8
	for(unsigned int srcY = 0; srcY < Machine::kDisplayHeight; srcY++)
	{
		for(unsigned int srcX = 0; srcX < Machine::kDisplayWidth; srcX++)
		{
			unsigned int srcRowByteIndex = srcX >> 3; // div 8
			uint8_t byteVal = pDisplayBuffer[(srcY * srcBytesPerRow) + srcRowByteIndex];
			unsigned int bitIndex = srcX & 7;
			uint8_t mask = 1 << bitIndex;
			uint8_t val = (byteVal & mask) ? 255 : 0;

			if(s_rotateDisplay)
			{
				// n.b. display is rotated 90 degrees so width and height are deliberately switched
				unsigned int dstX = srcY;
				unsigned int dstY = Machine::kDisplayWidth - 1 - srcX;
				s_texturePixelsR8[dstY * textureWidth + dstX] = val;
			}
			else
			{
				// unrotated
				unsigned int dstX = srcX;
				unsigned int dstY = srcY;
				s_texturePixelsR8[dstY * textureWidth + dstX] = val;
			}
		}
	}

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureWidth, textureHeight, GL_RED, GL_UNSIGNED_BYTE, &s_texturePixelsR8); // w and h deliberately swapped
}

static void drawTexture()
{
	glBindVertexArray(s_vao);
	glUseProgram(s_program);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glBindSampler(0, s_selectedSampler);
	glDrawArrays(GL_TRIANGLES, 0, 3);
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
	const char* glsl_version = "#version 150";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
	// GL 3.0 + GLSL 130
	const char* glsl_version = "#version 130";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

	// Create window with graphics context
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	
	const unsigned int displayWidth = s_rotateDisplay ? Machine::kDisplayHeight : Machine::kDisplayWidth;
	const unsigned int displayHeight = s_rotateDisplay ? Machine::kDisplayWidth : Machine::kDisplayHeight;

	static const unsigned int kPixelScale = 3;
	unsigned int windowWidth = kPixelScale * displayWidth;
	unsigned int windowHeight = kPixelScale * displayHeight;

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_Window* pWindow = SDL_CreateWindow("invaders-emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, windowWidth, windowHeight, 
		window_flags);

	SDL_GLContext gl_context = SDL_GL_CreateContext(pWindow);
	SDL_GL_SetSwapInterval(1); // Enable vsync
	const int swapInterval = SDL_GL_GetSwapInterval();
	const bool hasVsync = swapInterval > 0;
	if(hasVsync)
		printf("Display has VSYNC. SwapInterval=%i\n", swapInterval);
	else
		printf("Display does not have VSYNC\n");

	// Initialize OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
	bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
	bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
	bool err = gladLoadGL() == 0;
#else
	bool err = false; // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to requires some form of initialization.
#endif
	if(err)
	{
		fprintf(stderr, "Failed to initialize OpenGL loader!\n");
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
	ImGui_ImplSDL2_InitForOpenGL(pWindow, gl_context);
	ImGui_ImplOpenGL3_Init(glsl_version);


	Machine* pMachine = nullptr;
	if(!CreateMachine(&pMachine))
	{
		fprintf(stderr, "Failed to create Machine\n");
		return EXIT_FAILURE;
	}

	pMachine->debugHook = debugHook;

	if(!CreateMachine(&s_pSavedMachine))
	{
		fprintf(stderr, "Failed to create save state\n");
		return EXIT_FAILURE;
	}

	createOpenGLObjects(displayWidth, displayHeight);

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
		ImGui_ImplSDL2_NewFrame(pWindow);
		ImGui::NewFrame();

		StepFrame(pMachine, s_verbose);

		//ImGui::ShowDemoWindow();
		doDevUI(pMachine);

		// Rendering
		ImGui::Render();
		SDL_GL_MakeCurrent(pWindow, gl_context);
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		updateDisplayTexture(pMachine, displayWidth, displayHeight);
		drawTexture();

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(pWindow);

		frameIndex++;
	}

	deleteOpenGLObjects();

	DestroyMachine(pMachine);
	pMachine = nullptr;

	DestroyMachine(s_pSavedMachine);
	s_pSavedMachine = nullptr;

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(gl_context);

	SDL_DestroyWindow(pWindow);
	pWindow = nullptr;

	// SDL2_mixer
	ShutdownGameAudio();
	Mix_CloseAudio();

	Input::Shutdown();

	SDL_Quit();

	return 0;
}

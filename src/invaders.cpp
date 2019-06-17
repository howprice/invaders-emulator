// #TODO: Implement basic command line debugger
//        - 
// #TODO: Is RAM mirror at $4000 required by invaders roms??

#include "Assert.h"
#include "Helpers.h"
#include "debugger.h"
#include "8080.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_opengl3.h"

#include "SDL.h"

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

static const unsigned int kDisplayWidth = 1280;
static const unsigned int kDisplayHeight = 720;

enum class MemoryType
{
	ROM,
	RAM
};

struct Chunk
{
	size_t addressStart; // in machine address space
	size_t physicalMemoryStart; 
	size_t sizeInBytes;
	MemoryType memoryType;
};

// Memory map :
//   ROM
//   $0000 - $07ff : invaders.h
//   $0800 - $0fff : invaders.g
//   $1000 - $17ff : invaders.f
//   $1800 - $1fff : invaders.e
//
//   RAM
//   $2000 - $23ff : work RAM
//   $2400 - $3fff : video RAM
//   
//   $4000 - : RAM mirror
//
// http://www.emutalk.net/threads/38177-Space-Invaders?s=e58df01e41111c4efc6f3207b2890054&p=359411&viewfull=1#post359411

static const Chunk kChunks[] =
{
	{0x0000, 0x0000, 0x2000, MemoryType::ROM},
	{0x2000, 0x2000, 0x2000, MemoryType::RAM},
	{0x4000, 0x2000, 0x2000, MemoryType::RAM}, // RAM mirror
};

// In a more complicated system would calculate these from Chunk array
static const size_t kRomPhysicalSizeBytes = 0x2000;
static const size_t kRamPhysicalSizeBytes = 0x2000;
static const size_t kPhysicalMemorySizeBytes = kRomPhysicalSizeBytes + kRamPhysicalSizeBytes;

static bool s_debug = false;

static bool WriteByteToMemory(uint8_t* pMemory, size_t address, uint8_t val, bool fatalOnFail = false)
{
	HP_ASSERT(pMemory);

	for(unsigned int chunkIndex = 0; chunkIndex < COUNTOF_ARRAY(kChunks); chunkIndex++)
	{
		const Chunk& chunk = kChunks[chunkIndex];
		if(chunk.memoryType == MemoryType::ROM)
			continue;

		if(address >= chunk.addressStart && address < chunk.addressStart + chunk.sizeInBytes)
		{
			size_t offset = address - chunk.addressStart;
			size_t physicalAddress = chunk.physicalMemoryStart + offset;
			HP_ASSERT(physicalAddress < kPhysicalMemorySizeBytes);
			pMemory[physicalAddress] = val;
			return true;
		}
	}

	if(fatalOnFail)
	{
		// #TODO: Maybe fail silently - think this is how MSX cartridge copy protection may work.
		HP_FATAL_ERROR("Failed to find RAM memory chunk containing address %u", address);
	}

	return false;
}

static uint8_t ReadByteFromMemory(uint8_t* pMemory, size_t address, bool fatalOnFail = false)
{
	HP_ASSERT(pMemory);

	for(unsigned int chunkIndex = 0; chunkIndex < COUNTOF_ARRAY(kChunks); chunkIndex++)
	{
		const Chunk& chunk = kChunks[chunkIndex];
		if(address >= chunk.addressStart && address < chunk.addressStart + chunk.sizeInBytes)
		{
			size_t offset = address - chunk.addressStart;
			size_t physicalAddress = chunk.physicalMemoryStart + offset;
			HP_ASSERT(physicalAddress < kPhysicalMemorySizeBytes);
			uint8_t val = pMemory[physicalAddress];
			return val;
		}
	}

	if(fatalOnFail)
	{
		// #TODO: Maybe fail silently - think this is how MSX cartridge copy protection may work.
		HP_FATAL_ERROR("Failed to find RAM memory chunk containing address %u", address);
	}

	return 0;
}

static void TestMemory(uint8_t* pMemory)
{
	HP_ASSERT(pMemory);

	// ROM 0 - $1fff
	for(size_t address = 0; address < 0x2000; address++)
	{
		HP_ASSERT(WriteByteToMemory(pMemory, address, 0xcc) == false);
	}

	// RAM $2000 - $3fff
	for(size_t address = 0x2000; address < 0x4000; address++)
	{
		uint8_t val = (uint8_t)address;
		HP_ASSERT(WriteByteToMemory(pMemory, address, val) == true);
		HP_ASSERT(ReadByteFromMemory(pMemory, address) == val);
	}

	// RAM mirror $4000 - $5fff
	for(size_t address = 0x4000; address < 0x6000; address++)
	{
		uint8_t val = (uint8_t)address + 1;
		HP_ASSERT(WriteByteToMemory(pMemory, address, val) == true);
		HP_ASSERT(ReadByteFromMemory(pMemory, address) == val);
	}
}

struct Rom
{
	const char* fileName;
	size_t fileSizeBytes;
};

static const Rom kRoms[] =
{
	{"invaders.h", 0x800},
	{"invaders.g", 0x800},
	{"invaders.f", 0x800},
	{"invaders.e", 0x800},
};

//----------------------------------------------------------------------------------------
/* IO

Dedicated Shift Hardware

The 8080 instruction set does not include opcodes for shifting. An 8 - bit pixel image must 
be shifted into a 16 - bit word for the desired bit - position on the screen. Space Invaders 
adds a hardware shift register to help with the math.

16 bit shift register:

f              0	bit
xxxxxxxxyyyyyyyy

Writing to port 4 shifts x into y, and the new value into x, eg.
$0000,
write $aa->$aa00,
write $ff->$ffaa,
write $12->$12ff, ..

Writing to port 2 (bits 0, 1, 2) sets the offset for the 8 bit result, eg.
offset 0:
rrrrrrrr		     result = xxxxxxxx
xxxxxxxxyyyyyyyy

offset 2 :
  rrrrrrrr	         result = xxxxxxyy
xxxxxxxxyyyyyyyy

offset 7 :
       rrrrrrrr      result = xyyyyyyy
xxxxxxxxyyyyyyyy

Reading from port 3 returns said result.

http://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html#DedicatedShiftHardware
*/

static uint16_t s_shiftRegisterValue = 0x0000;
static uint8_t s_shiftRegisterOffset = 0; // [0,7]

static uint8_t In(uint8_t port)
{
	// http://www.emutalk.net/threads/38177-Space-Invaders?s=e58df01e41111c4efc6f3207b2890054&p=359411&viewfull=1#post359411

	// Space Invaders input ports

	// Read 1
	// BIT 0    coin(0 when active)
	//     1    P2 start button
	//     2    P1 start button
	//     3 ?
	//     4    P1 shoot button
	//     5    P1 joystick left
	//     6    P1 joystick right
	//     7 ?
	//     
	// Read 2
	// BIT 0, 1 dipswitch number of lives(0:3, 1 : 4, 2 : 5, 3 : 6)
	//     2    tilt 'button'
	//     3    dipswitch bonus life at 1 : 1000, 0 : 1500
	//     4    P2 shoot button
	//     5    P2 joystick left
	//     6    P2 joystick right
	//     7    dipswitch coin info 1 : off, 0 : on
	//     
	// Read 3   shift register result

	// Read port 1=$01 and 2=$00 will make the game run, but but only in attract mode.

	if(port == 1)
		return 1;
	else if(port == 2)
		return 0;
	else if(port == 3)
	{
		// Reading from port 3 returns the shifted register value.

		// Writing to port 2 (bits 0, 1, 2) sets the offset for the 8 bit result, eg.
		// offset 0:
		// rrrrrrrr		     result = xxxxxxxx
		// xxxxxxxxyyyyyyyy
		// 
		// offset 2 :
		//   rrrrrrrr	         result = xxxxxxyy
		// xxxxxxxxyyyyyyyy
		// 
		// offset 7 :
		//        rrrrrrrr      result = xyyyyyyy
		// xxxxxxxxyyyyyyyy
		// 
		
		// offset | shift
		//   0    |   8
		//   1    |   7
		//   2    |   6
		//   ...
		//   7    |   1
		//
		// So shift = 8 - offset

		HP_ASSERT(s_shiftRegisterOffset <= 7);
		unsigned int shift = 8 - s_shiftRegisterOffset;
		uint8_t result = (uint8_t)(s_shiftRegisterValue >> shift);
		return result;
	}
	else
	{
		HP_FATAL_ERROR("Unexpected IN port: %u", port);
	}

	return 0;
}

static void Out(uint8_t port, uint8_t val)
{
	HP_UNUSED(val);

	// Write 2    shift register result offset (bits 0, 1, 2)
	// Write 3    sound related
	// Write 4    fill shift register
	// Write 5    sound related
	// Write 6    strange 'debug' port ? eg.it writes to this port when
	//            it writes text to the screen(0 = a, 1 = b, 2 = c, etc)
	// 
	// (write ports 3, 5, 6 can be left unemulated)
	// http://www.emutalk.net/threads/38177-Space-Invaders?s=e58df01e41111c4efc6f3207b2890054&p=359411&viewfull=1#post359411

	switch(port)
	{
	case 2:
		// Writing to port 2 (bits 0, 1, 2) sets the offset for the 8 bit result,
		HP_ASSERT(val <= 7);
		s_shiftRegisterOffset = val;
		break;
	case 3:
		// #TODO: Implement sound
		break;
	case 4:
	{
		// Write into Shift Register
		// 
		// f              0	bit
		// xxxxxxxxyyyyyyyy
		//
		// Writing to port 4 shifts x into y, and the new value into x, eg.
		//	$0000,
		//	write $aa->$aa00,
		//	write $ff->$ffaa,
		//	write $12->$12ff, ..
		//
		// http://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html#DedicatedShiftHardware

		s_shiftRegisterValue = s_shiftRegisterValue >> 8;
//		s_shiftRegisterValue &= 0x00ff; // redundant; the above shift will shift in zeros from the left
		s_shiftRegisterValue |= (uint16_t)val << 8;
		break;
	}
	case 5:
		// #TODO: Implement sound
		break;
	case 6:
		// #TODO: Is this the "Watchdog"? http://computerarcheology.com/Arcade/SpaceInvaders/Code.html
		break;
	default:
		HP_FATAL_ERROR("Unexpected OUT port: %u", port);
	}
}

static void printUsage()
{
	puts("Usage: invaders [OPTIONS]");
	puts("Options:\n\n"
		"  --help                       Shows this message\n"
		"  -d <filename>                Use debugger\n"
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
			s_debug = true;
		}
		else
		{
			fprintf(stderr, "Unrecognised command line arg: %s\n", arg);
			printUsage();
			exit(EXIT_FAILURE);
		}
	}
}

int main(int argc, char** argv)
{
	parseCommandLine(argc, argv);

	if(SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		fprintf(stderr, "SDL failed to initialise: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

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
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_Window* pWindow = SDL_CreateWindow("invaders-emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, kDisplayWidth, kDisplayHeight, window_flags);

	SDL_GLContext gl_context = SDL_GL_CreateContext(pWindow);
	SDL_GL_SetSwapInterval(1); // Enable vsync

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

	State8080 state = {};

	// Init memory
	// #TODO: Should really assert that address space regions don't overlap, or if they do that
	//        they are of the same type? 
	state.pMemory = new uint8_t[kPhysicalMemorySizeBytes];
	state.memorySizeBytes = kPhysicalMemorySizeBytes;
	state.readByteFromMemory = ReadByteFromMemory;
	state.writeByteToMemory = WriteByteToMemory;

//	TestMemory(state.pMemory);

#if 1
	// Zero RAM
	// #TODO: Is this expected or required? Maybe just helpful for debugging. Write 0xCC maybe? 
	for(size_t address = kRomPhysicalSizeBytes; address < kPhysicalMemorySizeBytes; address++)
	{
		state.pMemory[address] = 0x00;
	}
#endif

	// Load ROMs into ROM memory
	size_t address = 0;
	for(unsigned int romIndex = 0; romIndex < COUNTOF_ARRAY(kRoms); romIndex++)
	{
		const Rom& rom = kRoms[romIndex];
		
		const char* fileName = rom.fileName;
		FILE* pFile = fopen(fileName, "rb");
		if(!pFile)
		{
			fprintf(stderr, "Failed to open file: %s\n", fileName);
			return EXIT_FAILURE;
		}

		fseek(pFile, 0, SEEK_END);
		size_t fileSizeBytes = ftell(pFile);
		printf("Opened file \"%s\" size %u (0x%X)\n", fileName, (unsigned int)fileSizeBytes, (unsigned int)fileSizeBytes);

		if(fileSizeBytes != rom.fileSizeBytes)
		{
			fprintf(stderr, "ROM file \"%s\" is incorrect size. Expected %u, got %u\n", fileName, rom.fileSizeBytes, fileSizeBytes);
			return EXIT_FAILURE;
		}
		fseek(pFile, 0, SEEK_SET);
		fread(state.pMemory + address, 1, fileSizeBytes, pFile);
		fclose(pFile);
		pFile = nullptr;

		address += fileSizeBytes;
	}

	// set up machine inputs and outputs, for assembly IN and OUT instructions
	state.in = In;
	state.out = Out;

	static bool show_demo_window = true;
	bool bDone = false;
	uint64_t frameIndex = 0;
	Uint64 lastTime = SDL_GetPerformanceCounter();
	while(!bDone)
	{
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			if(event.type == SDL_QUIT)
			{
				bDone = true;
			}
			else if(event.type == SDL_KEYDOWN)
			{
				if(event.key.keysym.sym == SDLK_ESCAPE)
					bDone = true;
				else if(event.key.keysym.sym == SDLK_SPACE)
				{
				}
			}
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(pWindow);
		ImGui::NewFrame();

		// update
		if(show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);

		bool active = true; // #TODO: Is this CPU state?
		while(active)
		{
			// #TODO: Add timings to 8080 instructions and break on emulated time rather than real time
			
			// #TODO: Interrupt $cf (RST 1) at the start of vblank http://www.emutalk.net/threads/38177-Space-Invaders?s=e58df01e41111c4efc6f3207b2890054&p=359411&viewfull=1#post359411
			//        OR..
			//        ScanLine96:
			//            Interrupt brings us here when the beam is* near* the middle of the screen. The real middle
			//            would be 224 / 2 = 112. The code pretends this interrupt happens at line 128.
			//        http://computerarcheology.com/Arcade/SpaceInvaders/Code.html

			// #TODO: Interrupts $d7 (RST 2) at the end of vblank. http://www.emutalk.net/threads/38177-Space-Invaders?s=e58df01e41111c4efc6f3207b2890054&p=359411&viewfull=1#post359411
			//        OR..
			//        ScanLine224:
			//            Interrupt brings us here when the beam is at the end of the screen(line 224) when 
			//            the VBLANK begins.
			//        http://computerarcheology.com/Arcade/SpaceInvaders/Code.html

			// break every 1/60 second
			Uint64 currentTime = SDL_GetPerformanceCounter();
			Uint64 deltaTime = currentTime - lastTime;
			Uint64 countsPerSecond = SDL_GetPerformanceFrequency();
			double deltaTimeSeconds = (double)deltaTime / countsPerSecond;
			if(deltaTimeSeconds > 1.0 / 60.0)
			{
				unsigned int rstNum = (frameIndex & 1) == 0 ? 1 : 2; // TEMP: oscillate between RST 1 and RST 2 in lieu of proper timing
				Generate8080Interrupt(state, rstNum);
				lastTime = currentTime;
				break;
			}

			if(s_debug)
			{
				Disassemble8080(state.pMemory, kPhysicalMemorySizeBytes, state.PC);
				printf("    ");
				PrintState(state);
			}
			Emulate8080Instruction(state);
		}

		// Rendering
		ImGui::Render();
		SDL_GL_MakeCurrent(pWindow, gl_context);
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(pWindow);

		frameIndex++;
	}

	delete[] state.pMemory;
	state.pMemory = nullptr;

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(gl_context);

	SDL_DestroyWindow(pWindow);
	pWindow = nullptr;

	SDL_Quit();

	return 0;
}

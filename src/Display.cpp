#include "Display.h"

#include "hp_assert.h"
#include "Helpers.h"

// Display static data
// #TODO: Get rid of all of this. Make members
SDL_Window* Display::s_pWindow = nullptr;
SDL_GLContext Display::s_sdl_gl_context = nullptr;
unsigned int Display::s_width;
unsigned int Display::s_height;
uint8_t* Display::s_pDisplayBuffer = nullptr;
unsigned int Display::s_displayBufferSizeBytes = 0;
bool Display::s_hasVsync = false;
bool Display::s_rotate = false;
uint8_t* Display::s_pTexturePixelsR8 = nullptr;
GLuint Display::s_texture = 0;
unsigned int Display::s_textureWidth = 0;
unsigned int Display::s_textureHeight = 0;

// file static data
// #TODO: Get rid of all of this. Make members
static const char* s_GlslVersionString = nullptr;
static GLuint s_vertexShader = 0;
static GLuint s_fragmentShader = 0;
static GLuint s_program = 0;
static GLuint s_pointSampler = 0;
static GLuint s_bilinearSampler = 0;
static GLuint s_selectedSampler = 0;
static GLuint s_vao = 0;

static const char s_vertexShaderSource[] =
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
	"uniform sampler2D sampler0;\n"
	"in vec2 texCoord;\n"
	"out vec4 Out_Color;\n"
	"void main()\n"
	"{\n"
	"	float r = texture(sampler0, texCoord).r; // 1 channel texture\n"
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

static void createOpenGLObjects()
{
	s_vertexShader = glCreateShader(GL_VERTEX_SHADER);
	const GLchar* vertexShaderSources[] = { s_GlslVersionString, s_vertexShaderSource };
	glShaderSource(s_vertexShader, COUNTOF_ARRAY(vertexShaderSources), vertexShaderSources, NULL);
	glCompileShader(s_vertexShader); // no return value
	CheckShader(s_vertexShader);

	s_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	const GLchar* fragmentShaderSources[] = { s_GlslVersionString, s_fragmentShaderSource };
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

	glDeleteSamplers(1, &s_pointSampler);
	s_pointSampler = 0;

	glDeleteSamplers(1, &s_bilinearSampler);
	s_bilinearSampler = 0;
}


void Display::updateTexture()
{
	if(s_rotate)
	{
		HP_ASSERT(s_textureHeight == s_width && s_textureWidth == s_height);
	}
	else
	{
		HP_ASSERT(s_textureWidth == s_width && s_textureHeight == s_height);
	}

	// src - 1 bit per pixel
	HP_ASSERT(s_pDisplayBuffer);

	// dst - 1 byte per pixel
	HP_ASSERT(s_pTexturePixelsR8);
	
	const unsigned int srcBytesPerRow = s_width >> 3; // div 8
	for(unsigned int srcY = 0; srcY < s_height; srcY++)
	{
		for(unsigned int srcX = 0; srcX < s_width; srcX++)
		{
			unsigned int srcRowByteIndex = srcX >> 3; // div 8
			uint8_t byteVal = s_pDisplayBuffer[(srcY * srcBytesPerRow) + srcRowByteIndex];
			unsigned int bitIndex = srcX & 7;
			uint8_t mask = 1 << bitIndex;
			uint8_t val = (byteVal & mask) ? 255 : 0;

			if(s_rotate)
			{
				// n.b. display is rotated 90 degrees so width and height are deliberately switched
				unsigned int dstX = srcY;
				unsigned int dstY = s_width - 1 - srcX;
				s_pTexturePixelsR8[dstY * s_textureWidth + dstX] = val;
			}
			else
			{
				// unrotated
				unsigned int dstX = srcX;
				unsigned int dstY = srcY;
				s_pTexturePixelsR8[dstY * s_textureWidth + dstX] = val;
			}
		}
	}

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, s_textureWidth, s_textureHeight, GL_RED, GL_UNSIGNED_BYTE, s_pTexturePixelsR8); // w and h deliberately swapped
}

static void drawTexture()
{
	glBindVertexArray(s_vao);
	glUseProgram(s_program);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
//	glBindTexture(GL_TEXTURE_2D, s_texture); // already bound
	glBindSampler(0, s_selectedSampler);
	glDrawArrays(GL_TRIANGLES, 0, 3);
}

bool Display::Create(const unsigned int width, const unsigned int height, unsigned int zoom, bool rotate, bool bFullscreen)
{
	HP_ASSERT(s_pWindow == nullptr);
	HP_ASSERT(width > 0 && height > 0);
	s_width = width;
	s_height = height;

	// Decide GL+GLSL versions
#if __APPLE__
	// GL 3.2 Core + GLSL 150
	s_GlslVersionString = "#version 150\n";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
	// GL 3.0 + GLSL 130
	s_GlslVersionString = "#version 130\n";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

	// Create window with graphics context
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	// Create SDL window

	unsigned int windowWidth = rotate ? height : width;
	unsigned int windowHeight = rotate ? width : height;

	HP_ASSERT(zoom > 0);
	windowWidth *= zoom;
	windowHeight *= zoom;

	const char* title = "invaders-emulator";
	if(bFullscreen)
	{
		HP_FATAL_ERROR("Test this");
		s_pWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
	}
	else
	{
		Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
#ifndef __APPLE__
		// #TODO: Figure out High DPI on Mac
		window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
		s_pWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, windowWidth, windowHeight, window_flags);
	}

	if(s_pWindow == nullptr)
	{
		fprintf(stderr, "Failed to create SDL window: %s\n", SDL_GetError());
		return false;
	}

	s_sdl_gl_context = SDL_GL_CreateContext(s_pWindow);
	SDL_GL_SetSwapInterval(1); // Enable vsync
	const int swapInterval = SDL_GL_GetSwapInterval();
	s_hasVsync = swapInterval > 0;
	if(s_hasVsync)
		printf("Display has VSYNC. SwapInterval=%i\n", swapInterval);
	else
		printf("Display does not have VSYNC\n");

	// Initialize OpenGL loader
	bool err = gl3wInit() != 0;
	if(err)
	{
		fprintf(stderr, "Failed to initialize OpenGL loader!\n");
		return false;
	}

	createOpenGLObjects();

	s_rotate = rotate;

	// display texture
	glGenTextures(1, &s_texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s_texture);
	s_textureWidth = rotate ? s_height : s_width;
	s_textureHeight = rotate ? s_width : s_height;
	glTexStorage2D(GL_TEXTURE_2D, 1/*mipLevels*/, GL_R8, s_textureWidth, s_textureHeight);


	HP_ASSERT((width % 8) == 0);
	const unsigned int bytesPerRow = s_width >> 3; // div 8
	s_displayBufferSizeBytes = bytesPerRow * height;
	s_pDisplayBuffer = new uint8_t[s_displayBufferSizeBytes];
	// #TODO: Zero buffer?

	// texture upload buffer. 1 byte per pixel
	unsigned int textureSizeBytes = width * height;
	s_pTexturePixelsR8 = new uint8_t[textureSizeBytes];

	return true;
}

void Display::Destroy()
{
	delete[] s_pTexturePixelsR8;
	s_pTexturePixelsR8 = nullptr;

	delete[] s_pDisplayBuffer;
	s_pDisplayBuffer = nullptr;


	glDeleteTextures(1, &s_texture);
	s_texture = 0;
	s_textureWidth = 0;
	s_textureHeight = 0;

	deleteOpenGLObjects();

	SDL_GL_DeleteContext(s_sdl_gl_context);

	HP_ASSERT(s_pWindow != nullptr);
	SDL_DestroyWindow(s_pWindow);
	s_pWindow = nullptr;
}

void Display::Clear()
{
	HP_ASSERT(s_pWindow);
	SDL_GL_MakeCurrent(s_pWindow, s_sdl_gl_context);

	unsigned int width, height;
	GetSize(width, height);
	glViewport(0, 0, (GLsizei)width, (GLsizei)height);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Display::Render()
{
	updateTexture();
	drawTexture();
}

void Display::Present()
{
	HP_ASSERT(s_pWindow);
	SDL_GL_SwapWindow(s_pWindow);
}

void Display::GetSize(unsigned int &width, unsigned int &height)
{
	int displayWidth, displayHeight;
	SDL_GetWindowSize(s_pWindow, &displayWidth, &displayHeight);
	width = (unsigned int)displayWidth;
	height = (unsigned int)displayHeight;
}

bool Display::IsFullscreen()
{
	Uint32 windowFlags = SDL_GetWindowFlags(s_pWindow);
	bool fullScreen = (windowFlags & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN;
	return fullScreen;
}

void Display::SetFullscreen(bool fullscreen)
{
	if(fullscreen)
	{
		SDL_SetWindowFullscreen(s_pWindow, SDL_WINDOW_FULLSCREEN);
		SDL_ShowCursor(SDL_DISABLE);
	}
	else
	{
		SDL_SetWindowFullscreen(s_pWindow, 0);
		SDL_ShowCursor(SDL_ENABLE);
	}
}

void Display::ToggleFullscreen()
{
	Uint32 windowFlags = SDL_GetWindowFlags(s_pWindow);
	if(windowFlags & SDL_WINDOW_FULLSCREEN)
	{
		SDL_SetWindowFullscreen(s_pWindow, 0);
		SDL_ShowCursor(SDL_DISABLE);
	}
	else
	{
		SDL_SetWindowFullscreen(s_pWindow, SDL_WINDOW_FULLSCREEN);
		SDL_ShowCursor(SDL_ENABLE);
	}
}

unsigned int Display::GetRefreshRate()
{
	SDL_DisplayMode displayMode;
	int displayIndex = SDL_GetWindowDisplayIndex(s_pWindow);

	static const unsigned int kDefaultRefreshRate = 60;
	if(SDL_GetDesktopDisplayMode(displayIndex, &displayMode) != 0)
	{
		return kDefaultRefreshRate;
	}

	if(displayMode.refresh_rate == 0)
	{
		return kDefaultRefreshRate;
	}

	return displayMode.refresh_rate;
}

bool Display::GetBilinearSampling()
{
	bool bilinearSampling = s_selectedSampler == s_bilinearSampler;
	return bilinearSampling;
}

void Display::SetBilinearSampling(bool bilinearSampling)
{
	if(bilinearSampling)
		s_selectedSampler = s_bilinearSampler;
	else
		s_selectedSampler = s_pointSampler;
}

void Display::SetByte(unsigned int address, uint8_t value)
{
	HP_ASSERT(address < s_displayBufferSizeBytes);
	HP_ASSERT(s_pDisplayBuffer != nullptr);
	s_pDisplayBuffer[address] = value;
}

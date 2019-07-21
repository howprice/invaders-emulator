#include "Display.h"

#include "Renderer.h"
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
bool Display::s_bilinearSampling = true;

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

bool Display::Create(const unsigned int width, const unsigned int height, unsigned int zoom, bool rotate, bool bFullscreen)
{
	HP_ASSERT(s_pWindow == nullptr);
	HP_ASSERT(width > 0 && height > 0);
	s_width = width;
	s_height = height;

	// Create SDL window
	unsigned int windowWidth = rotate ? height : width;
	unsigned int windowHeight = rotate ? width : height;

	HP_ASSERT(zoom > 0);
	windowWidth *= zoom;
	windowHeight *= zoom;

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

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
	SDL_GL_MakeCurrent(s_pWindow, s_sdl_gl_context);

	bool err = gl3wInit() != 0; // // needs to be called for each GL context
	if(err)
	{
		fprintf(stderr, "Failed to initialize OpenGL loader!\n");
		return false;
	}

	SDL_GL_SetSwapInterval(1); // Enable vsync
	const int swapInterval = SDL_GL_GetSwapInterval();
	s_hasVsync = swapInterval > 0;
	if(s_hasVsync)
		printf("Display has VSYNC. SwapInterval=%i\n", swapInterval);
	else
		printf("Display does not have VSYNC\n");

	s_rotate = rotate;

	// display buffer
	HP_ASSERT((width % 8) == 0);
	const unsigned int bytesPerRow = s_width >> 3; // div 8
	s_displayBufferSizeBytes = bytesPerRow * height;
	s_pDisplayBuffer = new uint8_t[s_displayBufferSizeBytes];
	// #TODO: Zero buffer?

	// display texture
	glGenTextures(1, &s_texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s_texture);
	s_textureWidth = rotate ? s_height : s_width;
	s_textureHeight = rotate ? s_width : s_height;
	glTexStorage2D(GL_TEXTURE_2D, 1/*mipLevels*/, GL_R8, s_textureWidth, s_textureHeight);

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
	Renderer::DrawFullScreenTexture(s_texture, s_bilinearSampling);
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

void Display::SetByte(unsigned int address, uint8_t value)
{
	HP_ASSERT(address < s_displayBufferSizeBytes);
	HP_ASSERT(s_pDisplayBuffer != nullptr);
	s_pDisplayBuffer[address] = value;
}

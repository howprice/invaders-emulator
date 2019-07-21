#include "Display.h"

#include "Renderer.h"
#include "hp_assert.h"
#include "Helpers.h"

// static
Display* Display::Create(const unsigned int width, const unsigned int height, unsigned int zoom, bool rotate, bool bFullscreen)
{
	Display* pDisplay = new Display();
	if(!pDisplay->init(width, height, zoom, rotate, bFullscreen))
	{
		delete pDisplay;
		pDisplay = nullptr;
	}
	return pDisplay;
}

// static
void Display::Destroy(Display* pDisplay)
{
	HP_ASSERT(pDisplay);
	delete pDisplay;
}

Display::Display()
{

}

Display::~Display()
{
	delete[] m_pTexturePixelsR8;
	m_pTexturePixelsR8 = nullptr;

	delete[] m_pDisplayBuffer;
	m_pDisplayBuffer = nullptr;

	if(m_texture > 0)
	{
		glDeleteTextures(1, &m_texture);
		m_texture = 0;
		m_textureWidth = 0;
		m_textureHeight = 0;
	}

	SDL_GL_DeleteContext(m_GLContext);

	SDL_DestroyWindow(m_pWindow);
	m_pWindow = nullptr;
}

bool Display::init(const unsigned int width, const unsigned int height, unsigned int zoom, bool rotate, bool bFullscreen)
{
	HP_ASSERT(width > 0 && height > 0);
	m_width = width;
	m_height = height;

	// Create SDL window
	unsigned int windowWidth = rotate ? height : width;
	unsigned int windowHeight = rotate ? width : height;

	HP_ASSERT(zoom > 0);
	windowWidth *= zoom;
	windowHeight *= zoom;

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	// create window
	HP_ASSERT(m_pWindow == nullptr);
	const char* title = "invaders-emulator";
	if(bFullscreen)
	{
		HP_FATAL_ERROR("Test this");
		m_pWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
	}
	else
	{
		Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
#ifndef __APPLE__
		// #TODO: Figure out High DPI on Mac
		window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
		m_pWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, windowWidth, windowHeight, window_flags);
	}

	if(m_pWindow == nullptr)
	{
		fprintf(stderr, "Failed to create SDL window: %s\n", SDL_GetError());
		return false;
	}

	m_GLContext = SDL_GL_CreateContext(m_pWindow);
	SDL_GL_MakeCurrent(m_pWindow, m_GLContext);

	bool err = gl3wInit() != 0; // // needs to be called for each GL context
	if(err)
	{
		fprintf(stderr, "Failed to initialize OpenGL loader!\n");
		return false;
	}

	SDL_GL_SetSwapInterval(1); // Enable vsync
	const int swapInterval = SDL_GL_GetSwapInterval();
	m_vsyncAvailable = swapInterval > 0;
	if(m_vsyncAvailable)
		printf("Display has VSYNC. SwapInterval=%i\n", swapInterval);
	else
		printf("Display does not have VSYNC\n");
	m_vsyncEnabled = m_vsyncAvailable;

	m_rotate = rotate;

	// display buffer
	HP_ASSERT((width % 8) == 0);
	const unsigned int bytesPerRow = m_width >> 3; // div 8
	m_displayBufferSizeBytes = bytesPerRow * height;
	m_pDisplayBuffer = new uint8_t[m_displayBufferSizeBytes];
	// #TODO: Zero buffer?

	// display texture
	glGenTextures(1, &m_texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_texture);
	m_textureWidth = rotate ? m_height : m_width;
	m_textureHeight = rotate ? m_width : m_height;
	glTexStorage2D(GL_TEXTURE_2D, 1/*mipLevels*/, GL_R8, m_textureWidth, m_textureHeight);

	// texture upload buffer. 1 byte per pixel
	unsigned int textureSizeBytes = width * height;
	m_pTexturePixelsR8 = new uint8_t[textureSizeBytes];

	return true;
}

void Display::Clear()
{
	HP_ASSERT(m_pWindow);
	SDL_GL_MakeCurrent(m_pWindow, m_GLContext);

	unsigned int width, height;
	GetSize(width, height);
	glViewport(0, 0, (GLsizei)width, (GLsizei)height);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Display::Render()
{
	updateTexture();
	Renderer::DrawFullScreenTexture(m_texture, m_bilinearSampling);
}

void Display::Present()
{
	HP_ASSERT(m_pWindow);
	SDL_GL_SwapWindow(m_pWindow);
}

void Display::GetSize(unsigned int &width, unsigned int &height)
{
	int displayWidth, displayHeight;
	SDL_GetWindowSize(m_pWindow, &displayWidth, &displayHeight);
	width = (unsigned int)displayWidth;
	height = (unsigned int)displayHeight;
}

bool Display::IsFullscreen()
{
	Uint32 windowFlags = SDL_GetWindowFlags(m_pWindow);
	bool fullScreen = (windowFlags & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN;
	return fullScreen;
}

void Display::SetFullscreen(bool fullscreen)
{
	if(fullscreen)
	{
		SDL_SetWindowFullscreen(m_pWindow, SDL_WINDOW_FULLSCREEN);
		SDL_ShowCursor(SDL_DISABLE);
	}
	else
	{
		SDL_SetWindowFullscreen(m_pWindow, 0);
		SDL_ShowCursor(SDL_ENABLE);
	}
}

void Display::ToggleFullscreen()
{
	Uint32 windowFlags = SDL_GetWindowFlags(m_pWindow);
	if(windowFlags & SDL_WINDOW_FULLSCREEN)
	{
		SDL_SetWindowFullscreen(m_pWindow, 0);
		SDL_ShowCursor(SDL_DISABLE);
	}
	else
	{
		SDL_SetWindowFullscreen(m_pWindow, SDL_WINDOW_FULLSCREEN);
		SDL_ShowCursor(SDL_ENABLE);
	}
}

unsigned int Display::GetRefreshRate()
{
	SDL_DisplayMode displayMode;
	int displayIndex = SDL_GetWindowDisplayIndex(m_pWindow);

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

bool Display::SetVsyncEnabled(bool enabled)
{
	HP_ASSERT(m_vsyncAvailable);
	bool success = false;
	if(enabled)
	{
		success = (SDL_GL_SetSwapInterval(1) == 0); // Enable vsync
	}
	else
	{
		success = (SDL_GL_SetSwapInterval(0) == 0); // Disable vsync
	}

	m_vsyncEnabled = SDL_GL_GetSwapInterval() > 0;
	return success;
}

void Display::SetByte(unsigned int address, uint8_t value)
{
	HP_ASSERT(address < m_displayBufferSizeBytes);
	HP_ASSERT(m_pDisplayBuffer != nullptr);
	m_pDisplayBuffer[address] = value;
}

void Display::updateTexture()
{
	if(m_rotate)
	{
		HP_ASSERT(m_textureHeight == m_width && m_textureWidth == m_height);
	}
	else
	{
		HP_ASSERT(m_textureWidth == m_width && m_textureHeight == m_height);
	}

	// src - 1 bit per pixel
	HP_ASSERT(m_pDisplayBuffer);

	// dst - 1 byte per pixel
	HP_ASSERT(m_pTexturePixelsR8);
	
	const unsigned int srcBytesPerRow = m_width >> 3; // div 8
	for(unsigned int srcY = 0; srcY < m_height; srcY++)
	{
		for(unsigned int srcX = 0; srcX < m_width; srcX++)
		{
			unsigned int srcRowByteIndex = srcX >> 3; // div 8
			uint8_t byteVal = m_pDisplayBuffer[(srcY * srcBytesPerRow) + srcRowByteIndex];
			unsigned int bitIndex = srcX & 7;
			uint8_t mask = 1 << bitIndex;
			uint8_t val = (byteVal & mask) ? 255 : 0;

			if(m_rotate)
			{
				// n.b. display is rotated 90 degrees so width and height are deliberately switched
				unsigned int dstX = srcY;
				unsigned int dstY = m_width - 1 - srcX;
				m_pTexturePixelsR8[dstY * m_textureWidth + dstX] = val;
			}
			else
			{
				// unrotated
				unsigned int dstX = srcX;
				unsigned int dstY = srcY;
				m_pTexturePixelsR8[dstY * m_textureWidth + dstX] = val;
			}
		}
	}

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_textureWidth, m_textureHeight, GL_RED, GL_UNSIGNED_BYTE, m_pTexturePixelsR8); // w and h deliberately swapped
}

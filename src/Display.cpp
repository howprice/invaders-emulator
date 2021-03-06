#include "Display.h"

#include "Renderer.h"
#include "hp_assert.h"
#include "Helpers.h"

// static
Display* Display::Create(const unsigned int width, const unsigned int height, unsigned int zoom, bool rotate, bool bFullscreen)
{
	Display* pDisplay = new Display(width, height);
	if(!pDisplay->init(zoom, rotate, bFullscreen))
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

Display::Display(unsigned int width, unsigned int height)
: m_width(width)
, m_height(height)
{
	HP_ASSERT(m_width > 0 && m_height > 0);
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
	}

	SDL_GL_DeleteContext(m_GLContext);

	SDL_DestroyWindow(m_pWindow);
	m_pWindow = nullptr;
}

bool Display::init(unsigned int zoom, bool rotate, bool bFullscreen)
{
	HP_ASSERT(zoom > 0);

	// Create SDL window
	m_rotate = rotate;
	unsigned int windowWidth = rotate ? m_height : m_width;
	unsigned int windowHeight = rotate ? m_width : m_height;
	windowWidth *= zoom;
	windowHeight *= zoom;

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	// create window
	HP_ASSERT(m_pWindow == nullptr);
	const char* title = "invaders-emulator";
	Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
#ifndef __APPLE__
	// #TODO: Figure out High DPI on Mac
	window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
	if(bFullscreen)
		window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

	m_pWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, windowWidth, windowHeight, window_flags);

	if(m_pWindow == nullptr)
	{
		fprintf(stderr, "Failed to create SDL window: %s\n", SDL_GetError());
		return false;
	}

	if(rotate)
		SDL_SetWindowMinimumSize(m_pWindow, (int)m_height, (int)m_width);
	else
		SDL_SetWindowMinimumSize(m_pWindow, (int)m_width, (int)m_height);

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

	// display buffer
	HP_ASSERT((m_width % 8) == 0);
	const unsigned int bytesPerRow = m_width >> 3; // div 8
	m_displayBufferSizeBytes = bytesPerRow * m_height;
	m_pDisplayBuffer = new uint8_t[m_displayBufferSizeBytes];
	// #TODO: Zero buffer?

	// display texture
	glGenTextures(1, &m_texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_texture);
	glTexStorage2D(GL_TEXTURE_2D, 1/*mipLevels*/, GL_R8, m_width, m_height);

	// texture upload buffer. 1 byte per pixel
	unsigned int textureSizeBytes = m_width * m_height;
	m_pTexturePixelsR8 = new uint8_t[textureSizeBytes];

	return true;
}

void Display::Clear()
{
	HP_ASSERT(m_pWindow);
	SDL_GL_MakeCurrent(m_pWindow, m_GLContext);

	unsigned int width, height;
	GetWindowClientSize(width, height);
	glViewport(0, 0, (GLsizei)width, (GLsizei)height);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Display::Render()
{
	updateTexture();

	unsigned int windowWidth, windowHeight;
	GetWindowClientSize(windowWidth, windowHeight);

	// size
	const unsigned int imageWidth = m_rotate ? m_height : m_width;
	const unsigned int imageHeight = m_rotate ? m_width : m_height;

	unsigned int viewportW, viewportH;
	if(m_fitWindow)
	{
		if(m_maintainAspectRatio)
		{
			HP_ASSERT(imageHeight > 0);
			const float imageAspectRatio = (float)imageWidth / (float)imageHeight;

			HP_ASSERT(windowHeight > 0);
			const float windowAspectRatio = (float)windowWidth / (float)windowHeight;

			if(imageAspectRatio == windowAspectRatio)
			{
				viewportW = windowWidth;
				viewportH = windowHeight;
			}
			else if(imageAspectRatio > windowAspectRatio)
			{
				// letterbox (bars above and below image)
				viewportW = windowWidth;
				viewportH = (unsigned int)((float)windowWidth * imageAspectRatio);
			}
			else
			{
				// pillarbox (bars left and right of image)
				viewportW = (unsigned int)((float)windowHeight * imageAspectRatio);
				viewportH = windowHeight;
			}
		}
		else
		{
			viewportW = windowWidth;
			viewportH = windowHeight;
		}
	}
	else
	{
		viewportW = imageWidth * m_imageScale;
		viewportH = imageHeight * m_imageScale;
	}

	// x
	int viewportX = 0;
	switch(m_horizontalAlignment)
	{
	case HorizontalAlignment::Left:
		viewportX = 0;
		break;
	case HorizontalAlignment::Centre:
		viewportX = ((int)windowWidth - (int)viewportW) / 2;
		break;
	case HorizontalAlignment::Right:
		viewportX = (int)windowWidth - (int)viewportW;
		break;
	}

	// y
	int viewportY = 0;
	switch(m_verticalAlignment)
	{
	case VerticalAlignment::Bottom:
		viewportY = 0;
		break;
	case VerticalAlignment::Centre:
		viewportY = ((int)windowHeight - (int)viewportH) / 2;
		break;
	case VerticalAlignment::Top:
		viewportY = (int)windowHeight - (int)viewportH;
		break;
	}

	glViewport((GLint)viewportX, (GLint)viewportY, (GLsizei)viewportW, (GLsizei)viewportH);

	Renderer::DrawFullScreenTexture(m_texture, m_bilinearSampling, m_rotate);
}

void Display::Present()
{
	HP_ASSERT(m_pWindow);
	SDL_GL_SwapWindow(m_pWindow);
}

void Display::GetWindowClientSize(unsigned int &width, unsigned int &height) const
{
	int displayWidth, displayHeight;
	SDL_GetWindowSize(m_pWindow, &displayWidth, &displayHeight);
	width = (unsigned int)displayWidth;
	height = (unsigned int)displayHeight;
}

void Display::SetWindowScale(unsigned int scale)
{
	HP_ASSERT(scale > 0);
	unsigned int windowWidth = m_rotate ? m_height : m_width;
	unsigned int windowHeight = m_rotate ? m_width : m_height;
	windowWidth *= scale;
	windowHeight *= scale;
	SDL_SetWindowSize(m_pWindow, (int)windowWidth, (int)windowHeight);
}

void Display::SetRotate(bool rotate)
{
	if(rotate == m_rotate)
		return;

	m_rotate = rotate;

	// swap width and height
	int windowWidth, windowHeight;
	SDL_GetWindowSize(m_pWindow, &windowWidth, &windowHeight);
	SDL_SetWindowSize(m_pWindow, windowHeight, windowWidth);

	if(rotate)
		SDL_SetWindowMinimumSize(m_pWindow, (int)m_height, (int)m_width);
	else
		SDL_SetWindowMinimumSize(m_pWindow, (int)m_width, (int)m_height);
}

bool Display::IsFullscreen() const
{
	Uint32 windowFlags = SDL_GetWindowFlags(m_pWindow);
	bool fullScreen = (windowFlags & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN;
	return fullScreen;
}

void Display::SetFullscreen(bool fullscreen)
{
	if(fullscreen)
	{
		SDL_SetWindowFullscreen(m_pWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
	}
	else
	{
		SDL_SetWindowFullscreen(m_pWindow, 0);
	}
}

void Display::ToggleFullscreen()
{
	Uint32 windowFlags = SDL_GetWindowFlags(m_pWindow);
	if(windowFlags & SDL_WINDOW_FULLSCREEN)
	{
		SDL_SetWindowFullscreen(m_pWindow, 0);
	}
	else
	{
		SDL_SetWindowFullscreen(m_pWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
	}
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

void Display::SetImageScale(unsigned int scale)
{
	HP_ASSERT(scale > 0);
	m_imageScale = scale;
}

void Display::SetByte(unsigned int address, uint8_t value)
{
	HP_ASSERT(address < m_displayBufferSizeBytes);
	HP_ASSERT(m_pDisplayBuffer != nullptr);
	m_pDisplayBuffer[address] = value;
}

void Display::updateTexture()
{
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

			unsigned int dstX = srcX;
			unsigned int dstY = srcY;
			m_pTexturePixelsR8[dstY * m_width + dstX] = val;
		}
	}

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RED, GL_UNSIGNED_BYTE, m_pTexturePixelsR8); // w and h deliberately swapped
}

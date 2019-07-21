#pragma once
#ifndef DISPLAY_H
#define DISPLAY_H

#include <SDL.h>

#include <GL/gl3w.h>

// #TODO: If there's no need to be a static class, then don't!
class Display
{
public:

	static bool Create(const unsigned int width, const unsigned int height, unsigned int zoom, bool rotate, bool bFullscreen);
	static void Destroy();

	static void Clear();
	static void Render();
	static void Present();

	static void GetSize(unsigned int &width, unsigned int &height);
	static bool IsFullscreen();
	static void SetFullscreen(bool fullscreen);
	static void ToggleFullscreen();
	static unsigned int GetRefreshRate();
	static bool GetBilinearSampling() { return s_bilinearSampling;  }
	static void SetBilinearSampling(bool bilinearSampling) { s_bilinearSampling = bilinearSampling; }

	static void SetByte(unsigned int address, uint8_t value);

	static SDL_Window* s_pWindow;
	static SDL_GLContext s_sdl_gl_context;

private:

	static void updateTexture();

	static unsigned int s_width;
	static unsigned int s_height;
	static uint8_t* s_pDisplayBuffer; // w * h * 1 bit per pixel
	static unsigned int s_displayBufferSizeBytes;
	static bool s_hasVsync;
	static bool s_rotate; // space invaders 90 degree rotated display

	static uint8_t* s_pTexturePixelsR8; // 1 byte per pixel
	static GLuint s_texture;
	static unsigned int s_textureWidth;
	static unsigned int s_textureHeight;

	static bool s_bilinearSampling;
};

#endif

#pragma once
#ifndef DISPLAY_H
#define DISPLAY_H

#include <SDL.h>

#include <GL/gl3w.h>

class Display
{
public:

	static Display* Create(const unsigned int width, const unsigned int height, unsigned int zoom, bool rotate, bool bFullscreen);
	static void Destroy(Display* pDisplay);

	void Clear();
	void Render();
	void Present();

	void GetSize(unsigned int &width, unsigned int &height) const;
	void SetZoom(unsigned int zoom);
	void SetRotate(bool rotate);
	bool GetRotate() const { return m_rotate; }
	bool IsFullscreen() const;
	void SetFullscreen(bool fullscreen);
	void ToggleFullscreen();
	bool GetBilinearSampling() const { return m_bilinearSampling; }
	void SetBilinearSampling(bool bilinearSampling) { m_bilinearSampling = bilinearSampling; }

	bool IsVsyncAvailable() const { return m_vsyncAvailable; }
	bool IsVsyncEnabled() const { return m_vsyncEnabled; }
	bool SetVsyncEnabled(bool enabled);

	void SetByte(unsigned int address, uint8_t value);

	SDL_Window* GetWindow() { return m_pWindow; }
	SDL_GLContext GetGLContext() { return m_GLContext; }

private:

	Display();
	~Display();

	bool init(const unsigned int width, const unsigned int height, unsigned int zoom, bool rotate, bool bFullscreen);

	void updateTexture();

	SDL_Window* m_pWindow = nullptr;
	SDL_GLContext m_GLContext = nullptr;

	unsigned int m_width = 0;
	unsigned int m_height = 0;
	uint8_t* m_pDisplayBuffer = nullptr; // w * h * 1 bit per pixel
	unsigned int m_displayBufferSizeBytes = 0;
	bool m_vsyncAvailable = false;
	bool m_vsyncEnabled = false;
	bool m_rotate = false; // space invaders 90 degree rotated display

	uint8_t* m_pTexturePixelsR8 = nullptr; // 1 byte per pixel
	GLuint m_texture = 0; // same dimensions as display
	bool m_bilinearSampling = true;
};

#endif

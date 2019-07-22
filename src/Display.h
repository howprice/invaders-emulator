#pragma once
#ifndef DISPLAY_H
#define DISPLAY_H

#include <SDL.h>

#include <GL/gl3w.h>

enum class HorizontalAlignment
{
	Left,
	Centre,
	Right
};

enum class VerticalAlignment
{
	Bottom,
	Centre,
	Top,
};

class Display
{
public:

	static Display* Create(const unsigned int width, const unsigned int height, unsigned int zoom, bool rotate, bool bFullscreen);
	static void Destroy(Display* pDisplay);

	void Clear();
	void Render();
	void Present();

	void GetWindowClientSize(unsigned int &width, unsigned int &height) const;
	void SetWindowScale(unsigned int scale);

	void SetRotate(bool rotate);
	bool GetRotate() const { return m_rotate; }

	bool IsFullscreen() const;
	void SetFullscreen(bool fullscreen);
	void ToggleFullscreen();

	bool IsVsyncAvailable() const { return m_vsyncAvailable; }
	bool IsVsyncEnabled() const { return m_vsyncEnabled; }
	bool SetVsyncEnabled(bool enabled);

	void SetImageScale(unsigned int scale);

	bool GetFitWindow() const { return m_fitWindow; }
	void SetFitWindow(bool val) { m_fitWindow = val; }

	bool GetMaintainAspectRatio() const { return m_maintainAspectRatio; }
	void SetMaintainAspectRatio(bool val) { m_maintainAspectRatio = val; }

	HorizontalAlignment GetHorizontalAlignment() const { return m_horizontalAlignment; }
	void SetHorizontalAlignment(HorizontalAlignment val) { m_horizontalAlignment = val; }

	VerticalAlignment GetVerticalAlignment() const { return m_verticalAlignment; }
	void SetVerticalAlignment(VerticalAlignment val) { m_verticalAlignment = val; }

	bool GetBilinearSampling() const { return m_bilinearSampling; }
	void SetBilinearSampling(bool bilinearSampling) { m_bilinearSampling = bilinearSampling; }

	void SetByte(unsigned int address, uint8_t value);

	SDL_Window* GetWindow() { return m_pWindow; }
	SDL_GLContext GetGLContext() { return m_GLContext; }

private:

	Display(unsigned int width, unsigned int height);
	~Display();

	bool init(unsigned int zoom, bool rotate, bool bFullscreen);

	void updateTexture();

	SDL_Window* m_pWindow = nullptr;
	SDL_GLContext m_GLContext = nullptr;

	const unsigned int m_width = 0;  // Machine display width in pixels. Not window width. 
	const unsigned int m_height = 0; // Machine display height in pixels. Not window height

	uint8_t* m_pDisplayBuffer = nullptr; // w * h * 1 bit per pixel
	unsigned int m_displayBufferSizeBytes = 0;
	bool m_vsyncAvailable = false;
	bool m_vsyncEnabled = false;
	bool m_rotate = false; // space invaders 90 degree rotated display

	uint8_t* m_pTexturePixelsR8 = nullptr; // 1 byte per pixel
	GLuint m_texture = 0; // same dimensions as display

	// render params
	unsigned int m_imageScale = 2;
	bool m_fitWindow = true;
	bool m_maintainAspectRatio = true;
	HorizontalAlignment m_horizontalAlignment = HorizontalAlignment::Centre;
	VerticalAlignment m_verticalAlignment = VerticalAlignment::Centre;
	bool m_bilinearSampling = false;
};

#endif

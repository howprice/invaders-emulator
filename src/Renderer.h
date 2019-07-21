#pragma once

#include <GL/gl3w.h>

class Renderer
{
public:

	static bool Init();
	static void Shutdown();

	static void DrawFullScreenTexture(GLuint texture, bool bilinearSampling);
};

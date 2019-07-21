#include "Renderer.h"

#include "Helpers.h"
#include "hp_assert.h"

#include <SDL.h>

#include <stdio.h>

// file static data
// #TODO: Get rid of all of this. Make members
static const char* s_GlslVersionString = nullptr;
static GLuint s_fullScreenQuadVertexShader = 0;
static GLuint s_fullScreenQuadRotatedVertexShader = 0;
static GLuint s_fragmentShader = 0;
static GLuint s_fullScreenQuadProgram = 0;
static GLuint s_fullScreenQuadRotatedProgram = 0;
static GLuint s_pointSampler = 0;
static GLuint s_bilinearSampler = 0;
static GLuint s_vao = 0;

static const char s_fullScreenQuadVertexShaderSource[] =
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
"        position = vec2( -1.0f, -1.0f );\n"
"        texCoord = vec2(  0.0f, 1.0f );\n"
"    }\n"
"    else if(gl_VertexID == 2)\n"
"    {\n"
"        position = vec2( 1.0f, 1.0f );\n"
"        texCoord = vec2( 1.0f, 0.0f );\n"
"    }\n"
"    else\n"
"    {\n"
"        position = vec2( 1.0f, -1.0f );\n"
"        texCoord = vec2( 1.0f, 1.0f );\n"
"    }\n"
"	 vec4 posPS = vec4(position, 0.0f, 1.0f);\n"
"	 gl_Position = posPS;\n"
"}\n";

static const char s_fullScreenQuadRotatedVertexShaderSource[] =
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
"        texCoord = vec2( 1.0f, 0.0f );\n"
"    }\n"
"    else if(gl_VertexID == 1)\n"
"    {\n"
"        position = vec2( -1.0f, -1.0f );\n"
"        texCoord = vec2(  0.0f, 0.0f );\n"
"    }\n"
"    else if(gl_VertexID == 2)\n"
"    {\n"
"        position = vec2( 1.0f, 1.0f );\n"
"        texCoord = vec2( 1.0f, 1.0f );\n"
"    }\n"
"    else\n"
"    {\n"
"        position = vec2( 1.0f, -1.0f );\n"
"        texCoord = vec2( 0.0f, 1.0f );\n"
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

bool Renderer::Init()
{
	// Decide GL+GLSL versions
#if __APPLE__
	// GL 3.2 Core + GLSL 150
	s_GlslVersionString = "#version 150\n";
#else
	// GL 3.0 + GLSL 130
	s_GlslVersionString = "#version 130\n";
#endif

	s_fullScreenQuadVertexShader = glCreateShader(GL_VERTEX_SHADER);
	const GLchar* fullScreenQuadVertexShaderSources[] = { s_GlslVersionString, s_fullScreenQuadVertexShaderSource };
	glShaderSource(s_fullScreenQuadVertexShader, COUNTOF_ARRAY(fullScreenQuadVertexShaderSources), fullScreenQuadVertexShaderSources, NULL);
	glCompileShader(s_fullScreenQuadVertexShader); // no return value
	CheckShader(s_fullScreenQuadVertexShader);

	s_fullScreenQuadRotatedVertexShader = glCreateShader(GL_VERTEX_SHADER);
	const GLchar* fullScreenQuadRotatedVertexShaderSources[] = { s_GlslVersionString, s_fullScreenQuadRotatedVertexShaderSource };
	glShaderSource(s_fullScreenQuadRotatedVertexShader, COUNTOF_ARRAY(fullScreenQuadRotatedVertexShaderSources), fullScreenQuadRotatedVertexShaderSources, NULL);
	glCompileShader(s_fullScreenQuadRotatedVertexShader); // no return value
	CheckShader(s_fullScreenQuadRotatedVertexShader);

	s_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	const GLchar* fragmentShaderSources[] = { s_GlslVersionString, s_fragmentShaderSource };
	glShaderSource(s_fragmentShader, COUNTOF_ARRAY(fragmentShaderSources), fragmentShaderSources, NULL);
	glCompileShader(s_fragmentShader); // no return value
	CheckShader(s_fragmentShader);

	s_fullScreenQuadProgram = glCreateProgram();
	glAttachShader(s_fullScreenQuadProgram, s_fullScreenQuadVertexShader);
	glAttachShader(s_fullScreenQuadProgram, s_fragmentShader);
	glLinkProgram(s_fullScreenQuadProgram);
	CheckProgram(s_fullScreenQuadProgram);

	// No need to keep the shaders attached now that the program is linked
	glDetachShader(s_fullScreenQuadProgram, s_fullScreenQuadVertexShader);
	glDetachShader(s_fullScreenQuadProgram, s_fragmentShader);

	s_fullScreenQuadRotatedProgram = glCreateProgram();
	glAttachShader(s_fullScreenQuadRotatedProgram, s_fullScreenQuadRotatedVertexShader);
	glAttachShader(s_fullScreenQuadRotatedProgram, s_fragmentShader);
	glLinkProgram(s_fullScreenQuadRotatedProgram);
	CheckProgram(s_fullScreenQuadRotatedProgram);

	// No need to keep the shaders attached now that the program is linked
	glDetachShader(s_fullScreenQuadRotatedProgram, s_fullScreenQuadVertexShader);
	glDetachShader(s_fullScreenQuadRotatedProgram, s_fragmentShader);

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

	return true;
}

void Renderer::Shutdown()
{
	glDeleteProgram(s_fullScreenQuadProgram);
	s_fullScreenQuadProgram = 0;
	glDeleteProgram(s_fullScreenQuadRotatedProgram);
	s_fullScreenQuadRotatedProgram = 0;

	glDeleteShader(s_fullScreenQuadVertexShader);
	s_fullScreenQuadVertexShader = 0;
	glDeleteShader(s_fullScreenQuadRotatedVertexShader);
	s_fullScreenQuadRotatedVertexShader = 0;

	glDeleteShader(s_fragmentShader);
	s_fragmentShader = 0;

	glDeleteVertexArrays(1, &s_vao);
	s_vao = 0;

	glDeleteSamplers(1, &s_pointSampler);
	s_pointSampler = 0;

	glDeleteSamplers(1, &s_bilinearSampler);
	s_bilinearSampler = 0;
}

void Renderer::DrawFullScreenTexture(GLuint texture, bool bilinearSampling, bool rotate)
{
	drawFullScreenQuad(texture, bilinearSampling, rotate);
}

void Renderer::drawFullScreenQuad(GLuint texture, bool bilinearSampling, bool rotate)
{
	glBindVertexArray(s_vao);
	glUseProgram(rotate ? s_fullScreenQuadRotatedProgram : s_fullScreenQuadProgram);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glBindTexture(GL_TEXTURE_2D, texture); // already bound
	glBindSampler(0, bilinearSampling ? s_bilinearSampler : s_pointSampler);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

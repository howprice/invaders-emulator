#include "Audio.h"

#include "Assert.h"

#include "SDL_mixer.h"

static const unsigned int kSampleCount = 19;
static Mix_Chunk* s_pChunk[kSampleCount] = {};

bool InitAudio()
{
	for(unsigned int sampleIndex = 0; sampleIndex < kSampleCount; sampleIndex++)
	{
		char filename[64];
		SDL_snprintf(filename, sizeof(filename), "%u.wav", sampleIndex);
		s_pChunk[sampleIndex] = Mix_LoadWAV(filename);
		if(s_pChunk[sampleIndex] == NULL)
		{
			fprintf(stderr, "Mix_LoadWAV failed: %s\n", Mix_GetError());
			return false;
		}
	}

	static bool testAudio = false;
	if(testAudio)
	{
		// TEST - play all samples in order
		for(unsigned int sampleIndex = 0; sampleIndex < kSampleCount; sampleIndex++)
		{
			int channel = Mix_PlayChannel(-1, s_pChunk[sampleIndex], 0);
			if(channel == -1)
			{
				fprintf(stderr, "Mix_PlayChannel failed: %s\n", Mix_GetError());
			}

			while(Mix_Playing(channel) != 0);
		}
	}

	return true;
}

void ShutdownAudio()
{
	for(unsigned int sampleIndex = 0; sampleIndex < kSampleCount; sampleIndex++)
	{
		Mix_FreeChunk(s_pChunk[sampleIndex]);
		s_pChunk[sampleIndex] = nullptr;
	}
}

void PlaySample(unsigned int index)
{
	HP_ASSERT(index < kSampleCount);
	int channel = Mix_PlayChannel(/*index*/-1, s_pChunk[index], 0);
	if(channel == -1)
	{
		fprintf(stderr, "Mix_PlayChannel failed: %s\n", Mix_GetError());
	}
}

#include "Audio.h"

#include "Assert.h"
#include "Helpers.h"

#include "SDL_mixer.h"

static const unsigned int kSampleCount = 9; // there are more in the invaders.zip sample file, but they don't seem to be used

struct Sample
{
	const char* filename;
	bool loop;
};

static const Sample s_samples[] =
{
	{"0.wav", true},  // UFO
	{"1.wav", false}, // Shot
	{"2.wav", false}, // Flash (player die)
	{"3.wav", false}, // Invader die
	{"4.wav", false}, // Fleet movement 1
	{"5.wav", false}, // Fleet movement 2
	{"6.wav", false}, // Fleet movement 3
	{"7.wav", false}, // Fleet movement 4
	{"8.wav", false}, // UFO Hit
};
static_assert(COUNTOF_ARRAY(s_samples) == kSampleCount, "Array size incorrect");

static Mix_Chunk* s_pChunk[kSampleCount] = {};

void InitGameAudio()
{
	for(unsigned int sampleIndex = 0; sampleIndex < kSampleCount; sampleIndex++)
	{
		const Sample& sample = s_samples[sampleIndex];
		s_pChunk[sampleIndex] = Mix_LoadWAV(sample.filename);
		if(s_pChunk[sampleIndex] == NULL)
		{
			fprintf(stderr, "Failed to load audio file: \"%s\" %s\n", sample.filename, Mix_GetError());
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
}

void ShutdownGameAudio()
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
	if(s_pChunk[index] == nullptr)
		return; // sample not loaded

	int loops = s_samples[index].loop ? -1 : 0;
	int channel = Mix_PlayChannel(/*index*/-1, s_pChunk[index], loops);
	if(channel == -1)
	{
		fprintf(stderr, "Mix_PlayChannel failed: %s\n", Mix_GetError());
	}
}

void StopSample(unsigned int index)
{
	HP_ASSERT(index < kSampleCount);
	Mix_HaltChannel((int)index);
}

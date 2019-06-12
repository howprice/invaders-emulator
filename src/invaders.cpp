// #TODO: Accessors to read and write memory,
//        - Catch illegal addresses
//        - Catch writes to ROM
// #TODO: Is RAM mirror at $4000 required?

#include "Helpers.h"
#include "8080.h"

#include <stdio.h>
#include <stdlib.h>

int main()
{
	printf("invaders\n");

	State8080 state = {};

	// Memory map :
	//   ROM
	//   $0000 - $07ff : invaders.h
	//   $0800 - $0fff : invaders.g
	//   $1000 - $17ff : invaders.f
	//   $1800 - $1fff : invaders.e
	//
	//   RAM
	//   $2000 - $23ff : work RAM
	//   $2400 - $3fff : video RAM
	//   
	//   $4000 - : RAM mirror
	//
	// http://www.emutalk.net/threads/38177-Space-Invaders?s=e58df01e41111c4efc6f3207b2890054&p=359411&viewfull=1#post359411

	struct Rom
	{
		const char* fileName;
		size_t fileSizeBytes;
	};

	static const Rom s_roms[] =
	{
		{"invaders.h", 0x800},
		{"invaders.g", 0x800},
		{"invaders.f", 0x800},
		{"invaders.e", 0x800},
	};

	size_t romSizeBytes = 0;
	for(unsigned int romIndex = 0; romIndex < COUNTOF_ARRAY(s_roms); romIndex++)
	{
		const Rom& rom = s_roms[romIndex];
		romSizeBytes += rom.fileSizeBytes;
	}
	
	size_t ramSizeBytes = 0x2000;
	size_t memorySizeBytes = romSizeBytes + ramSizeBytes;
	state.pMemory = new uint8_t[memorySizeBytes];

	size_t memoryOffset = 0;
	for(unsigned int romIndex = 0; romIndex < COUNTOF_ARRAY(s_roms); romIndex++)
	{
		const Rom& rom = s_roms[romIndex];
		
		const char* fileName = rom.fileName;
		FILE* pFile = fopen(fileName, "rb");
		if(!pFile)
		{
			fprintf(stderr, "Failed to open file: %s\n", fileName);
			return EXIT_FAILURE;
		}

		fseek(pFile, 0, SEEK_END);
		size_t fileSizeBytes = ftell(pFile);
		printf("Opened file \"%s\" size %u (0x%X)\n", fileName, (unsigned int)fileSizeBytes, (unsigned int)fileSizeBytes);

		if(fileSizeBytes != rom.fileSizeBytes)
		{
			fprintf(stderr, "ROM file \"%s\" is incorrect size. Expected %u, got %u\n", fileName, rom.fileSizeBytes, fileSizeBytes);
			return EXIT_FAILURE;
		}
		fseek(pFile, 0, SEEK_SET);
		fread(state.pMemory + memoryOffset, 1, fileSizeBytes, pFile);
		fclose(pFile);
		pFile = nullptr;

		memoryOffset += fileSizeBytes;
	}

	// Zero RAM
	// #TODO: Is this expected or required? Maybe just helpful for debugging. Write 0xCC maybe? 
	for(size_t address = romSizeBytes; address < memorySizeBytes; address++)
	{
		state.pMemory[address] = 0x00;
	}

	// #TODO: Emulation loop here


	delete[] state.pMemory;
	state.pMemory = nullptr;

	return 0;
}

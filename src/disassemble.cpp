// Run with command line: invaders.h invaders.g invaders.f invaders.e

#include "8080.h"
#include "hp_assert.h"
#include "Helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char** argv)
{
	unsigned int fileCount = (unsigned int)argc - 1;
	if(fileCount < 1)
	{
		puts("Usage: disassemble <file> [<file2> [<file3> ...]]");
		return EXIT_FAILURE;
	}

	size_t bufferSizeBytes = 0;
	for(unsigned int fileIndex = 0; fileIndex < fileCount; fileIndex++)
	{
		const char* fileName = argv[fileIndex + 1];
		FILE* pFile = fopen(fileName, "rb");
		if(!pFile)
		{
			fprintf(stderr, "Failed to open file: %s\n", fileName);
			return EXIT_FAILURE;
		}

		fseek(pFile, 0, SEEK_END);
		size_t fileSizeBytes = ftell(pFile);
		fseek(pFile, 0, SEEK_SET);
		printf("Opened file \"%s\" size %u (0x%X)\n", fileName, (unsigned int)fileSizeBytes, (unsigned int)fileSizeBytes);
		fclose(pFile);
		pFile = nullptr;

		bufferSizeBytes += fileSizeBytes;
	}
	printf("Total size: %u bytes (0x%X)\n", (unsigned int)bufferSizeBytes, (unsigned int)bufferSizeBytes);

	uint8_t* buffer = new uint8_t[bufferSizeBytes];
	size_t bufferOffset = 0;
	for(unsigned int fileIndex = 0; fileIndex < fileCount; fileIndex++)
	{
		const char* fileName = argv[fileIndex + 1];
		FILE* pFile = fopen(fileName, "rb");
		if(!pFile)
		{
			fprintf(stderr, "Failed to open file: %s\n", fileName);
			return EXIT_FAILURE;
		}

		fseek(pFile, 0, SEEK_END);
		size_t fileSizeBytes = ftell(pFile);
		fseek(pFile, 0, SEEK_SET);
		if(fread(buffer + bufferOffset, 1, fileSizeBytes, pFile) != fileSizeBytes)
		{
			fprintf(stderr, "Failed to read file: %s\n", fileName);
			return EXIT_FAILURE;
		}
		fclose(pFile);
		pFile = nullptr;

		bufferOffset += fileSizeBytes;
	}

	unsigned int pc = 0;
	while(pc < bufferSizeBytes)
	{
		pc += Disassemble8080(buffer, bufferSizeBytes, pc);
		printf("\n");
	}

	delete[] buffer;
	buffer = nullptr;
	return EXIT_SUCCESS;
}

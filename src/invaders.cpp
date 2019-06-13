// #TODO: Is RAM mirror at $4000 required by invaders roms??

#include "Assert.h"
#include "Helpers.h"
#include "8080.h"

#include <stdio.h>
#include <stdlib.h>

enum class MemoryType
{
	ROM,
	RAM
};

struct Chunk
{
	size_t addressStart; // in machine address space
	size_t physicalMemoryStart; 
	size_t sizeInBytes;
	MemoryType memoryType;
};

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

static const Chunk kChunks[] =
{
	{0x0000, 0x0000, 0x2000, MemoryType::ROM},
	{0x2000, 0x2000, 0x2000, MemoryType::RAM},
	{0x4000, 0x2000, 0x2000, MemoryType::RAM}, // RAM mirror
};

// In a more complicated system would calculate these from Chunk array
static const size_t kRomPhysicalSizeBytes = 0x2000;
static const size_t kRamPhysicalSizeBytes = 0x2000;
static const size_t kPhysicalMemorySizeBytes = kRomPhysicalSizeBytes + kRamPhysicalSizeBytes;

static bool WriteByteToMemory(uint8_t* pMemory, size_t address, uint8_t val, bool fatalOnFail = false)
{
	HP_ASSERT(pMemory);

	for(unsigned int chunkIndex = 0; chunkIndex < COUNTOF_ARRAY(kChunks); chunkIndex++)
	{
		const Chunk& chunk = kChunks[chunkIndex];
		if(chunk.memoryType == MemoryType::ROM)
			continue;

		if(address >= chunk.addressStart && address < chunk.addressStart + chunk.sizeInBytes)
		{
			size_t offset = address - chunk.addressStart;
			size_t physicalAddress = chunk.physicalMemoryStart + offset;
			HP_ASSERT(physicalAddress < kPhysicalMemorySizeBytes);
			pMemory[physicalAddress] = val;
			return true;
		}
	}

	if(fatalOnFail)
	{
		// #TODO: Maybe fail silently - think this is how MSX cartridge copy protection may work.
		HP_FATAL_ERROR("Failed to find RAM memory chunk containing address %u", address);
	}

	return false;
}

static uint8_t ReadByteFromMemory(uint8_t* pMemory, size_t address, bool fatalOnFail = false)
{
	HP_ASSERT(pMemory);

	for(unsigned int chunkIndex = 0; chunkIndex < COUNTOF_ARRAY(kChunks); chunkIndex++)
	{
		const Chunk& chunk = kChunks[chunkIndex];
		if(address >= chunk.addressStart && address < chunk.addressStart + chunk.sizeInBytes)
		{
			size_t offset = address - chunk.addressStart;
			size_t physicalAddress = chunk.physicalMemoryStart + offset;
			HP_ASSERT(physicalAddress < kPhysicalMemorySizeBytes);
			uint8_t val = pMemory[physicalAddress];
			return val;
		}
	}

	if(fatalOnFail)
	{
		// #TODO: Maybe fail silently - think this is how MSX cartridge copy protection may work.
		HP_FATAL_ERROR("Failed to find RAM memory chunk containing address %u", address);
	}

	return 0;
}

static void TestMemory(uint8_t* pMemory)
{
	HP_ASSERT(pMemory);

	// ROM 0 - $1fff
	for(size_t address = 0; address < 0x2000; address++)
	{
		HP_ASSERT(WriteByteToMemory(pMemory, address, 0xcc) == false);
	}

	// RAM $2000 - $3fff
	for(size_t address = 0x2000; address < 0x4000; address++)
	{
		uint8_t val = (uint8_t)address;
		HP_ASSERT(WriteByteToMemory(pMemory, address, val) == true);
		HP_ASSERT(ReadByteFromMemory(pMemory, address) == val);
	}

	// RAM mirror $4000 - $5fff
	for(size_t address = 0x4000; address < 0x6000; address++)
	{
		uint8_t val = (uint8_t)address + 1;
		HP_ASSERT(WriteByteToMemory(pMemory, address, val) == true);
		HP_ASSERT(ReadByteFromMemory(pMemory, address) == val);
	}
}

struct Rom
{
	const char* fileName;
	size_t fileSizeBytes;
};

static const Rom kRoms[] =
{
	{"invaders.h", 0x800},
	{"invaders.g", 0x800},
	{"invaders.f", 0x800},
	{"invaders.e", 0x800},
};

int main()
{
	printf("invaders\n");

	State8080 state = {};

	// Init memory
	// #TODO: Should really assert that address space regions don't overlap, or if they do that
	//        they are of the same type? 
	state.pMemory = new uint8_t[kPhysicalMemorySizeBytes];

	TestMemory(state.pMemory);

#if 0
	// Zero RAM
	// #TODO: Is this expected or required? Maybe just helpful for debugging. Write 0xCC maybe? 
	for(size_t address = kRomPhysicalSizeBytes; address < kPhysicalMemorySizeBytes; address++)
	{
		state.pMemory[address] = 0x00;
	}
#endif

	// Load ROMs into ROM memory
	size_t address = 0;
	for(unsigned int romIndex = 0; romIndex < COUNTOF_ARRAY(kRoms); romIndex++)
	{
		const Rom& rom = kRoms[romIndex];
		
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
		fread(state.pMemory + address, 1, fileSizeBytes, pFile);
		fclose(pFile);
		pFile = nullptr;

		address += fileSizeBytes;
	}

	// #TODO: Emulation loop here

	delete[] state.pMemory;
	state.pMemory = nullptr;

	return 0;
}

#include "8086-cpu-simulation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ADDRESS_LINES 20

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

const char* NON_WIDE_REGISTERS[8] = {
    "al\0",
    "cl\0",
    "dl\0",
    "bl\0",
    "ah\0",
    "ch\0",
    "dh\0",
    "bh\0"
};

const char* WIDE_REGISTERS[8] = {
    "ax\0",
    "cx\0",
    "dx\0",
    "bx\0",
    "sp\0",
    "bp\0",
    "si\0",
    "di\0"
};

static u32 LoadMemoryFromFile(char* filename, u8* memory)
{
    u32 result = 0;
    u32 maxBytes2Read = 1 << ADDRESS_LINES;

    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        fprintf(stderr, "ERROR: Unable to open %s\n", filename);
        return 0;
    }

    fseek(file, 0, SEEK_END);
    u64 fileSize = ftell(file);
    rewind(file);

    if (fileSize > maxBytes2Read)
        fprintf(stderr, "Warning: File bigger than available memory\n");

    result = fread(memory, 1, maxBytes2Read, file);

    return result;
}

static u8* AllocateMemoryPow2(u32 sizePow2)
{
    static u8 failedAllocationByte;

    u8* memory = (u8*) malloc(1<<sizePow2);
    if (!memory)
    {
        sizePow2 = 0;
        memory = &failedAllocationByte;
    }

    return memory;
}

static void Disassembly(u32 bytesCount, u8* mainMemory)
{
    for (u32 instructions = 0; instructions < bytesCount; instructions += 2)
    {
        u8 instructionByte1 = mainMemory[instructions];
        u8 instructionByte2 = mainMemory[instructions + 1];

        bool d = instructionByte1 & 2;
        bool w = instructionByte1 & 1;

        u8 reg = instructionByte2 & 7;
        u8 rm = (instructionByte2 >> 3) & 7;

        if (d)
        {
            u8 temp = reg;
            reg = rm;
            rm = temp;
        }

        if (w)
            printf("mov %s, %s\n", WIDE_REGISTERS[reg], WIDE_REGISTERS[rm]);
        else
            printf("mov %s, %s\n", NON_WIDE_REGISTERS[reg], NON_WIDE_REGISTERS[rm]);
    }
}

int main(int argumentsCount, char** arguments)
{
    // The number of address lines in 8086 is 20
    u8* mainMemory = AllocateMemoryPow2(ADDRESS_LINES);
    if (!mainMemory)
    {
        fprintf(stderr,"Failed to allocate memory for 8086\n");
        return 1;
    }

    if (argumentsCount <= 1)
    {
        // PrintHelp();
        fprintf(stderr,"USAGE: %s [8086 machine code file]\n", arguments[0]);
        return 2;
    }

    for (int argumentIndex = 1; argumentIndex < argumentsCount; ++argumentIndex)
    {
        char* filename = arguments[argumentIndex];

        u32 bytesRead = LoadMemoryFromFile(filename, mainMemory);
        if (bytesRead <= 0)
        {
            fprintf(stderr, "Failed to read %s", filename);
            continue;
        }

        Disassembly(bytesRead, mainMemory);
    }

    return 0;
}

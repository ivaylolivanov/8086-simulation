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

const char* RM_EFFECTIVE_ADDRESS_CALCULATION[8] ={
    "bx + si\0",
    "bx + di\0",
    "bp + si\0",
    "bp + di\0",
    "si\0",
    "di\0",
    "bp\0",
    "bx\0"
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

enum InstructionOpcodes : u8
{
    // sizeof(d) = 1 bit, sizeof(w) = 1 bit, sizeof(reg) = 3 bit

    MOV_REG_MEMTO_OR_FROMREG = 0x22, // 1 0 0 0 1 0 d w
    MOV_IMM_TO_RM            = 0x63, // 1 1 0 0 0 1 1 w
    MOV_IMM_TO_REG           = 0xB,  // 1 0 1 1 w  reg
    MOV_MEM_TO_ACC           = 0x50, // 1 0 1 0 0 0 0 w
    MOV_ACC_TO_MEM           = 0x51, // 1 0 1 0 0 0 1 w
    MOV_RM_TO_SEG_REG        = 0x8E, // 1 0 0 0 1 1 1 0
    MOV_SEG_REG_TO_RM        = 0x8E, // 1 0 0 0 1 1 0 0
};

inline static bool ContainsOpCode(u8 byte, InstructionOpcodes opcode)
{
    bool result = false;

    u8 mask = (1 << sizeof(opcode) * 8) - 1;
    while (byte != 0)
    {
        if ((mask & byte) == opcode)
        {
            result = true;
            break;
        }

        byte >>= 1;
    }

    return result;
}

static s8 LeftmostSetBitPosition(u8 data)
{
    s8 position = INVALID_BIT_POSITION;

    u8 currentPosition = 0;
    while (currentPosition <= MAX_BITS_IN_BYTE)
    {
        if ((data >> currentPosition) & 1)
            position = currentPosition;

        ++currentPosition;
    }

    return position;
}

static void Disassembly(u32 bytesCount, u8* mainMemory)
{
    u32 byteIndex = 0;
    while (byteIndex < bytesCount)
    {
        u8 opcodeInstruction = mainMemory[byteIndex];
        if (ContainsOpCode(opcodeInstruction, MOV_REG_MEMTO_OR_FROMREG))
        {
            bool d = opcodeInstruction & 2;
            bool w = opcodeInstruction & 1;

            ++byteIndex;
            u8 modRegRm = mainMemory[byteIndex];

            u8 mod = (modRegRm >> 6) & 3;
            u8 reg = (modRegRm >> 3) & 7;
            u8 rm  =  modRegRm       & 7;

            if (!d)
            {
                u8 temp = reg;
                reg = rm;
                rm = temp;
            }

            switch (mod)
            {
                // Memory mode, no displacement. EXCEPT when R/M is
                // 110, then 16-bit displacement
                case 0:
                {
                    if (rm != 6)
                    {
                        if (w)
                            printf("mov [%s], %s\n", RM_EFFECTIVE_ADDRESS_CALCULATION[reg], WIDE_REGISTERS[rm]);
                        else
                            printf("mov [%s], %s\n", RM_EFFECTIVE_ADDRESS_CALCULATION[reg], NON_WIDE_REGISTERS[rm]);
                    }
                    else
                    {
                        s16 displacement = (mainMemory[byteIndex + 2] << 8) | mainMemory[byteIndex + 1];
                        byteIndex += 2;

                        if (w)
                            printf("mov %s, %d\n",
                                   WIDE_REGISTERS[reg],
                                   displacement);
                        else
                            printf("mov %s, %d\n",
                                   NON_WIDE_REGISTERS[reg],
                                   displacement);
                    }
                } break;

                // Memory mode, 8-bit displacement.
                case 1:
                {
                    ++byteIndex;
                    s8 displacement = mainMemory[byteIndex];
                    if (w)
                    {
                        if (displacement)
                            printf("mov %s, [%s + %d]\n",
                                   WIDE_REGISTERS[reg],
                                   RM_EFFECTIVE_ADDRESS_CALCULATION[rm],
                                   displacement);
                        else
                            printf("mov %s, [%s]\n",
                                   WIDE_REGISTERS[reg],
                                   RM_EFFECTIVE_ADDRESS_CALCULATION[rm]);
                    }
                    else
                    {
                        if (d)
                        {
                            if (displacement)
                                printf("mov %s, [%s + %d]\n",
                                       NON_WIDE_REGISTERS[reg],
                                       RM_EFFECTIVE_ADDRESS_CALCULATION[rm],
                                       displacement);
                            else
                                printf("mov %s, [%s]\n",
                                       WIDE_REGISTERS[reg],
                                       RM_EFFECTIVE_ADDRESS_CALCULATION[rm]);
                        }
                        else
                        {
                            if (displacement)
                                printf("mov [%s + %d], %s\n",
                                       RM_EFFECTIVE_ADDRESS_CALCULATION[reg],
                                       displacement,
                                       NON_WIDE_REGISTERS[rm]);
                            else
                                printf("mov [%s], %s\n",
                                       RM_EFFECTIVE_ADDRESS_CALCULATION[reg],
                                       NON_WIDE_REGISTERS[rm]);
                        }
                    }
                } break;

                // Memory mode, 16-bit displacement.
                case 2:
                {
                    s16 displacement = (mainMemory[byteIndex + 2] << 8) | mainMemory[byteIndex + 1];

                    if (w)
                    {
                        if (displacement)
                            printf("mov %s, [%s + %d]\n",
                                   WIDE_REGISTERS[reg],
                                   RM_EFFECTIVE_ADDRESS_CALCULATION[rm],
                                   displacement);
                        else
                            printf("mov %s, [%s]\n",
                                   WIDE_REGISTERS[reg],
                                   RM_EFFECTIVE_ADDRESS_CALCULATION[rm]);
                    }
                    else
                    {
                        if (displacement)
                            printf("mov %s, [%s + %d]\n",
                                   NON_WIDE_REGISTERS[reg],
                                   RM_EFFECTIVE_ADDRESS_CALCULATION[rm],
                                   displacement);
                        else
                            printf("mov %s, [%s]\n",
                                   NON_WIDE_REGISTERS[reg],
                                   RM_EFFECTIVE_ADDRESS_CALCULATION[rm]);
                    }

                } break;

                // Register mode, no displacement
                case 3:
                {
                    if (w)
                        printf("mov %s, %s\n", WIDE_REGISTERS[reg], WIDE_REGISTERS[rm]);
                    else
                        printf("mov %s, %s\n", NON_WIDE_REGISTERS[reg], NON_WIDE_REGISTERS[rm]);
                } break;
            }
        }
        else if (ContainsOpCode(opcodeInstruction, MOV_IMM_TO_REG))
        {
            bool w = opcodeInstruction >>3 & 1;
            u8 reg = opcodeInstruction & 7;
            if (w)
            {
                s16 data = (mainMemory[byteIndex + 2] << 8) | mainMemory[byteIndex + 1];
                printf("mov %s, %d\n", WIDE_REGISTERS[reg], data);
                ++byteIndex;
                // byteIndex += 2;
            }
            else
            {
                s8 data = mainMemory[byteIndex + 1];
                printf("mov %s, %d\n", NON_WIDE_REGISTERS[reg], data);
                // ++byteIndex;
            }
        }

        ++byteIndex;
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

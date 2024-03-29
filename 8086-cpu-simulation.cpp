#include "8086-cpu-simulation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ADDRESS_LINES 20

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

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

inline static u32 StringTerminationPosition(const s8* string)
{
    u32 position = 0;
    while(string[position] != '\0')
        ++position;

    return position;
}

inline static void Append(s8* destination, const s8* content, u32 max)
{
    u32 currentPosition = StringTerminationPosition(destination);
    if (currentPosition >= max) return;

    u32 contentSymbols = StringTerminationPosition(content);
    if ((currentPosition + contentSymbols) >= max) return;

    u32 currentContentSymbol = 0;
    while (content[currentContentSymbol] != '\0')
        destination[currentPosition++] = content[currentContentSymbol++];

    destination[currentPosition] = '\0';
}

inline static bool ContainsOpCode(u8 byte, InstructionOpcodes opcode)
{
    bool result = false;

    while (byte != 0)
    {
        if (byte == opcode)
        {
            result = true;
            break;
        }

        byte >>= 1;
    }

    return result;
}

static void MovRegToRegOrMemOrFromReg(u8* memory, u32& memoryIndex)
{
    bool direction       = memory[memoryIndex] & 2;
    bool isWordOperation = memory[memoryIndex] & 1;

    ++memoryIndex;
    u8 modRegRm = memory[memoryIndex];

    u8 mod = (modRegRm >> 6) & 3;
    u8 reg = (modRegRm >> 3) & 7;
    u8 rm  =  modRegRm       & 7;

    s16 displacement = 0;
    bool hasDisplacement = (mod == 0 && rm == 6) || (mod == 2) || (mod == 1);
    if (hasDisplacement)
    {
        ++memoryIndex;
        displacement = (s8)memory[memoryIndex];

        if ((mod == 0 && rm == 6) || mod == 2)
        {
            ++memoryIndex;
            displacement = (memory[memoryIndex] << 8) | memory[memoryIndex - 1];
        }
    }

    s8 destination[MAX_INSTRUCTION_STRING_LENGTH];
    strcpy(destination, isWordOperation ?
        WORD_REGISTERS[direction ? reg : rm]
           : BYTE_REGISTERS[direction ? reg : rm]);

    s8 source[MAX_INSTRUCTION_STRING_LENGTH];
    strcpy(source, isWordOperation ?
           WORD_REGISTERS[direction ? rm : reg]
           : BYTE_REGISTERS[direction ? rm : reg]);

    // When Mod is 0, then Memory mode, no displacement. EXCEPT when
    // R/M is 0110 (6), then 16-bit displacement.
    //
    // If (direction)  -> destination is REG
    // If (!direction) -> destination is RM
    if (!mod && (rm == 6))
        snprintf(
            direction ? source : destination,
            MAX_INSTRUCTION_STRING_LENGTH,
            "[%d]",
            displacement);
    else if ((mod == 1) || (mod == 2))
        snprintf(
            direction ? source : destination,
            MAX_INSTRUCTION_STRING_LENGTH,
            displacement ? displacement > 0 ? "[%s + %d]" : "[%s %d]" : "[%s]",
            RM_EFFECTIVE_ADDRESS_CALCULATION[rm], displacement);

    printf("mov %s, %s\n", destination, source);
}

static void MovImmediatelyToRegOrMem(u8* memory, u32& memoryIndex)
{
    bool isWordOperation = memory[memoryIndex] & 1;

    ++memoryIndex;
    u8 modRm = memory[memoryIndex];

    u8 mod = (modRm >> 6) & 3;
    u8 rm  = modRm        & 7;

    s16 displacement = 0;
    s16 data = 0;
    bool hasDisplacement = (mod == 0 && rm == 6) || (mod == 2) || (mod == 1);
    if (hasDisplacement)
    {
        ++memoryIndex;
        displacement = (s8)memory[memoryIndex];

        if ((mod == 0 && rm == 6) || mod == 2)
        {
            ++memoryIndex;
            displacement = (memory[memoryIndex] << 8) | memory[memoryIndex - 1];
        }
    }

    ++memoryIndex;
    if (isWordOperation) ++memoryIndex;

    data = isWordOperation ?
        (memory[memoryIndex] << 8) | memory[memoryIndex - 1]
        : (s8)memory[memoryIndex];

    s8 destination[MAX_INSTRUCTION_STRING_LENGTH];
    strcpy(destination, isWordOperation
           ? WORD_REGISTERS[rm] : BYTE_REGISTERS[rm]);
    if (!mod || (mod == 1) || (mod == 2))
    {
        snprintf(
            destination,
            MAX_INSTRUCTION_STRING_LENGTH,
            displacement ? displacement > 0 ? "[%s + %d]" : "[%s %d]" : "[%s]",
            RM_EFFECTIVE_ADDRESS_CALCULATION[rm], displacement);
    }

    s8 source[MAX_INSTRUCTION_STRING_LENGTH];
    snprintf(
        source,
        MAX_INSTRUCTION_STRING_LENGTH,
        "%s %d",
        isWordOperation ? "word" : "byte",
        data);

    printf ("mov %s, %s\n", destination, source);
}

static void MovImmediatelyToRegister(u8* memory, u32& memoryIndex)
{
    bool isWordOperation = (memory[memoryIndex] >> 3) & 1;
    u8 reg = memory[memoryIndex] & 7;

    ++memoryIndex;

    const s8* destination = BYTE_REGISTERS[reg];
    s8 value[MAX_INSTRUCTION_STRING_LENGTH];
    snprintf(value, MAX_INSTRUCTION_STRING_LENGTH, "%d", (s8)memory[memoryIndex]);

    if (isWordOperation)
    {
        destination = WORD_REGISTERS[reg];
        ++memoryIndex;
        s16 data = (memory[memoryIndex] << 8) | memory[memoryIndex - 1];
        snprintf(value, MAX_INSTRUCTION_STRING_LENGTH, "%d", data);
    }

    printf("mov %s, %s\n", destination, value);
}

static void MovMemToAcc(u8* memory, u32& memoryIndex)
{
    bool isWordOperation = (memory[memoryIndex] >> 3) & 1;

    memoryIndex += 2;
    s16 address = (memory[memoryIndex] << 8) | memory[memoryIndex - 1];

    printf("mov ax, [%d]\n", address);
}

static void MovAccToMem(u8* memory, u32& memoryIndex)
{
    bool isWordOperation = (memory[memoryIndex] >> 3) & 1;

    memoryIndex += 2;
    s16 address = (memory[memoryIndex] << 8) | memory[memoryIndex - 1];

    printf("mov [%d], ax\n", address);
}

static void MovRmToSr(u8* memory, u32& memoryIndex)
{
    printf ("WARNING: NOT TESTED - Register / memory to segmented register\n");

    ++memoryIndex;
    u8 modSrRm = memory[memoryIndex];

    u8 mod = (modSrRm >> 6) & 3;
    u8 sr  = (modSrRm >> 3) & 3;
    u8 rm  =  modSrRm       & 7;

    s16 displacement = 0;
    bool hasDisplacement = (mod == 0 && rm == 6) || (mod == 2) || (mod == 1);
    if (hasDisplacement)
    {
        ++memoryIndex;
        displacement = (s8)memory[memoryIndex];

        if ((mod == 0 && rm == 6) || mod == 2)
        {
            ++memoryIndex;
            displacement = (memory[memoryIndex] << 8) | memory[memoryIndex - 1];
        }
    }

    s8 destination[MAX_INSTRUCTION_STRING_LENGTH];
    strcpy(destination, SEGMENTED_REGISTERS[sr]);

    s8 source[MAX_INSTRUCTION_STRING_LENGTH];
    strcpy(source, BYTE_REGISTERS[rm]);

    if (!mod && (rm == 6))
        snprintf(
            source,
            MAX_INSTRUCTION_STRING_LENGTH,
            "[%d]",
            displacement);
    else if ((mod == 1) || (mod == 2))
        snprintf(
            source,
            MAX_INSTRUCTION_STRING_LENGTH,
            displacement ? displacement > 0 ? "[%s + %d]" : "[%s %d]" : "[%s]",
            RM_EFFECTIVE_ADDRESS_CALCULATION[rm], displacement);

    printf ("mov %s, %s\n", destination, source);
}

static void MovSrToRm(u8* memory, u32& memoryIndex)
{
    printf ("WARNING: NOT TESTED - Segmented register to Register / memory\n");

    ++memoryIndex;
    u8 modSrRm = memory[memoryIndex];

    u8 mod = (modSrRm >> 6) & 3;
    u8 sr  = (modSrRm >> 3) & 3;
    u8 rm  =  modSrRm       & 7;

    s16 displacement = 0;
    bool hasDisplacement = (mod == 0 && rm == 6) || (mod == 2) || (mod == 1);
    if (hasDisplacement)
    {
        ++memoryIndex;
        displacement = (s8)memory[memoryIndex];

        if ((mod == 0 && rm == 6) || mod == 2)
        {
            ++memoryIndex;
            displacement = (memory[memoryIndex] << 8) | memory[memoryIndex - 1];
        }
    }

    s8 destination[MAX_INSTRUCTION_STRING_LENGTH];
    strcpy(destination, BYTE_REGISTERS[rm]);

    s8 source[MAX_INSTRUCTION_STRING_LENGTH];
    strcpy(source, SEGMENTED_REGISTERS[sr]);


    if (!mod && (rm == 6))
        snprintf(
            destination,
            MAX_INSTRUCTION_STRING_LENGTH,
            "[%d]",
            displacement);
    else if ((mod == 1) || (mod == 2))
        snprintf(
            destination,
            MAX_INSTRUCTION_STRING_LENGTH,
            displacement ? displacement > 0 ? "[%s + %d]" : "[%s %d]" : "[%s]",
            RM_EFFECTIVE_ADDRESS_CALCULATION[rm], displacement);

    printf ("mov %s, %s\n", destination, source);
}

static void AddRmWRegToEither(u8* memory, u32& memoryIndex)
{
    bool direction       = memory[memoryIndex] & 2;
    bool isWordOperation = memory[memoryIndex] & 1;

    ++memoryIndex;
    u8 modRegRm = memory[memoryIndex];

    u8 mod = (modRegRm >> 6) & 3;
    u8 reg = (modRegRm >> 3) & 7;
    u8 rm  = modRegRm        & 7;

    s16 displacement = 0;
    bool hasDisplacement = (mod == 0 && rm == 6) || (mod == 2) || (mod == 1);
    if (hasDisplacement)
    {
        ++memoryIndex;
        displacement = (s8)memory[memoryIndex];

        if ((mod == 0 && rm == 6) || mod == 2)
        {
            ++memoryIndex;
            displacement = (memory[memoryIndex] << 8) | memory[memoryIndex - 1];
        }
    }

    s8 destination[MAX_INSTRUCTION_STRING_LENGTH];
    strcpy(destination, isWordOperation ?
           WORD_REGISTERS[direction ? reg : rm]
           : BYTE_REGISTERS[direction ? reg : rm]);

    s8 source[MAX_INSTRUCTION_STRING_LENGTH];
    strcpy(source, isWordOperation ?
           WORD_REGISTERS[direction ? rm : reg]
           : BYTE_REGISTERS[direction ? rm : reg]);

    if (!mod && (rm == 6))
        snprintf(
            direction ? source : destination,
            MAX_INSTRUCTION_STRING_LENGTH,
            "[%d]",
            displacement);
    else if ((mod == 1) || (mod == 2))
        snprintf(
            direction ? source : destination,
            MAX_INSTRUCTION_STRING_LENGTH,
            displacement ? displacement > 0 ? "[%s + %d]" : "[%s %d]" : "[%s]",
            RM_EFFECTIVE_ADDRESS_CALCULATION[rm],
            displacement);

    printf("add %s, %s\n", destination, source);
}

static void Disassembly(u32 bytesCount, u8* mainMemory)
{
    u32 byteIndex = 0;
    while (byteIndex < bytesCount)
    {
        u8 opcodeInstruction = mainMemory[byteIndex];
        if (ContainsOpCode(opcodeInstruction, MOV_REG_MEMTO_OR_FROMREG))
            MovRegToRegOrMemOrFromReg(mainMemory, byteIndex);
        else if (ContainsOpCode(opcodeInstruction, MOV_IMM_TO_RM))
            MovImmediatelyToRegOrMem(mainMemory, byteIndex);
        else if (ContainsOpCode(opcodeInstruction, MOV_IMM_TO_REG))
            MovImmediatelyToRegister(mainMemory, byteIndex);
        else if (ContainsOpCode(opcodeInstruction, MOV_MEM_TO_ACC))
            MovMemToAcc(mainMemory, byteIndex);
        else if (ContainsOpCode(opcodeInstruction, MOV_ACC_TO_MEM))
            MovAccToMem(mainMemory, byteIndex);
        else if (ContainsOpCode(opcodeInstruction, MOV_RM_TO_SEG_REG))
            MovRmToSr(mainMemory, byteIndex);
        else if (ContainsOpCode(opcodeInstruction, MOV_SEG_REG_TO_RM))
            MovSrToRm(mainMemory, byteIndex);
        else if (ContainsOpCode(opcodeInstruction, ADD_RM_W_REG_TO_EITHER))
            AddRmWRegToEither(mainMemory, byteIndex);
        // else
        //     printf("WARNING: Unidentified instruction: %d\n", opcodeInstruction);

        // Move to next instruction
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

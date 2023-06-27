typedef char unsigned u8;
typedef short unsigned u16;
typedef int unsigned u32;
typedef long long unsigned u64;

typedef char s8;
typedef short s16;
typedef int s32;
typedef long long s64;

typedef s32 b32;

#define INVALID_BIT_POSITION -1
#define MAX_BITS_IN_BYTE 7

#define MAX_INSTRUCTION_STRING_LENGTH 100

const s8* BYTE_REGISTERS[8] = {
    "al\0",
    "cl\0",
    "dl\0",
    "bl\0",
    "ah\0",
    "ch\0",
    "dh\0",
    "bh\0"
};

const s8* WORD_REGISTERS[8] = {
    "ax\0",
    "cx\0",
    "dx\0",
    "bx\0",
    "sp\0",
    "bp\0",
    "si\0",
    "di\0"
};

const s8* RM_EFFECTIVE_ADDRESS_CALCULATION[8] ={
    "bx + si\0",
    "bx + di\0",
    "bp + si\0",
    "bp + di\0",
    "si\0",
    "di\0",
    "bp\0",
    "bx\0"
};

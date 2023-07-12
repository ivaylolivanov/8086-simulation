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

const s8* SEGMENTED_REGISTERS[4] = {
    "es\0",
    "cs\0",
    "ss\0",
    "ds\0",
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

enum InstructionOpcodes : u8
{
    // sizeof(d)   = 1 bit
    // sizeof(w)   = 1 bit
    // sizeof(reg) = 3 bit
    // sizeof(s)   = 1 bit

    // Bit index                        7 6 5 4 3 2 1 0
    //
    // MOV opcodes
    MOV_REG_MEMTO_OR_FROMREG = 0x22, // 1 0 0 0 1 0 d w
    MOV_IMM_TO_RM            = 0x63, // 1 1 0 0 0 1 1 w
    MOV_IMM_TO_REG           = 0xB,  // 1 0 1 1 w  reg
    MOV_MEM_TO_ACC           = 0x50, // 1 0 1 0 0 0 0 w
    MOV_ACC_TO_MEM           = 0x51, // 1 0 1 0 0 0 1 w
    MOV_RM_TO_SEG_REG        = 0x8E, // 1 0 0 0 1 1 1 0
    MOV_SEG_REG_TO_RM        = 0x8E, // 1 0 0 0 1 1 0 0

    // Bit index                        7 6 5 4 3 2 1 0
    //
    // ADD opcodes
    ADD_RM_W_REG_TO_EITHER   = 0x0,  // 0 0 0 0 0 0 d w
    ADD_IMM_TO_REG           = 0x20, // 1 0 0 0 0 0 s w
    ADD_IMM_TO_ACC           = 0x2,  // 0 0 0 0 0 1 0 w
};

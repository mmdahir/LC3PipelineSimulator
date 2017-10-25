/***********************************************************************
* TCSS 372 - Project                                                   *
* By: John Chang, Mustaf Dahir, Mohamed Mohamed, Harmandeep Singh      *
***********************************************************************/
#ifndef PLC3_H
#define PLC3_H
#include <curses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DEBUG 0
#define NO_OF_REGISTERS 8
#define SIZE_OF_MEM 8192
#define START_STACK 4096
#define DISPLAY_SIZE 16
#define DEFAULT_ADDRESS 0x3000

#define FETCH 0
#define DECODE 1
#define EVAL_ADDR 2
#define FETCH_OP 3
#define EXECUTE 4
#define STORE 5
#define DONE 6

#define OPCODE_FIELD 0xF000 // 1111 0000 0000 0000 - gets first three bits
#define OPCODE_FIELD_SHIFT 12
#define RD_FIELD 0x0E00 // 0000 1110 0000 0000 - gets Rd field bits
#define RD_FIELD_SHIFT 9
#define RS_FIELD 0x01C0 // 0000 0001 1100 0000 - gets Rs field bits
#define RS_FIELD_SHIFT 6
#define RS2_FIELD 0x0007 //0000 0000 0000 0111  
#define OFFSET9_SIGNED_BIT 0x0200

#define IMM5_MASK 0x001F 
#define OFFSET6_MASK 0x003F 
#define OFFSET9_MASK 0x01FF 
#define OFFSET11_MASK 0x07FF 
#define TRAP_VECT8_MASK 0x00FF

#define IMMED_FIELD 0x001F // 0000 0000 0001 1111 - gets immed field bits
#define IMMED7_FIELD 0x00FF //0000 0000 1111 1111 - gets immed 

#define EXT5 0x0010
#define NEG5 0xFFF0
#define EXT6 0x0020
#define NEG6 0xFF70
#define EXT9 0x0100
#define NEG9 0xFF00
#define EXT11 0x400
#define NEG11 0xFC00

#define SECOND_SOURCE_REGISTER_BIT 0x0020 //0000 0000 0010 0000
#define HIGH_ORDER_BIT_VALUE 0x0080 // 0000 0000 1000 0000

#define ADD 1
#define AND 5
#define NOT 9
#define TRAP 15
#define LD 2
#define ST 3
#define JMP 12
#define BR 0
#define JSRR 4
#define LEA 14
#define RET 12
#define STR 7
#define LDR 6
#define PUSHPOP 13
#define LDI 10 //added
#define STI 11//added

#define N 4 //100
#define Z 2 //010
#define P 1 //001

#define LOAD 1
#define SAVE 2
#define STEP 3
#define RUN 4
#define SHOW 5
#define EDIT 6
#define SetBrkpt 7
#define EXIT 8

#define GETCH 32 //0x20
#define OUT 33 //0x21
#define PUTS 34 //0x22
#define HALT 37 //0x25

#define STEP_MODE 0
#define RUN_MODE 1

#define REG_FILE_LINES 11
#define REG_FILE_COLS 12
#define REG_FILE_START_Y 1
#define REG_FILE_START_X 1

#define MEMORY_LINES 19
#define MEMORY_COLS 17
#define MEMORY_START_Y 1
#define MEMORY_START_X 62

#define FBUFF_LINES 3
#define FBUFF_COLS 25
#define FBUFF_START_Y 1
#define FBUFF_START_X 25

#define DBUFF_LINES 4
#define DBUFF_COLS 24
#define DBUFF_START_Y 4
#define DBUFF_START_X 25

#define EBUFF_LINES 4
#define EBUFF_COLS 17
#define EBUFF_START_Y 8
#define EBUFF_START_X 25

#define MBUFF_LINES 4
#define MBUFF_COLS 17
#define MBUFF_START_Y 12
#define MBUFF_START_X 25

#define STORE_LINES 3
#define STORE_COLS 22
#define STORE_START_Y 16
#define STORE_START_X 22

#define MENU_Y_POS 19
#define MENU_X_POS 0

#define SCR_TITLE_Y_POS 0
#define SCR_TITLE_X_POS 20

#define REG_TITLE_Y_POS 1
#define REG_TITLE_X_POS 2

#define MEM_TITLE_Y_POS 1
#define MEM_TITLE_X_POS 5

#define FBUFF_TITLE_Y_POS 2
#define FBUFF_TITLE_X_POS 19
#define FBUFF_DATA_Y_POS 1
#define FBUFF_DATA_X_POS 1

#define DBUFF_TITLE_Y_POS 5
#define DBUFF_TITLE_X_POS 19
#define DBUFF_HEADERS_Y_POS 1
#define DBUFF_HEADERS_X_POS 1
#define DBUFF_DATA_Y_POS 2
#define DBUFF_DATA_X_POS 1

#define EBUFF_TITLE_Y_POS 9
#define EBUFF_TITLE_X_POS 19
#define EBUFF_HEADERS_Y_POS 1
#define EBUFF_HEADERS_X_POS 1
#define EBUFF_DATA_Y_POS 2
#define EBUFF_DATA_X_POS 1

#define MBUFF_TITLE_Y_POS 13
#define MBUFF_TITLE_X_POS 19
#define MBUFF_HEADERS_Y_POS 1
#define MBUFF_HEADERS_X_POS 1
#define MBUFF_DATA_Y_POS 2
#define MBUFF_DATA_X_POS 1

#define STORE_INFO_Y_POS 1
#define STORE_INFO_X_POS 1

#define CC_INFO_Y_POS 2
#define CC_INFO_X_POS 3

#define LC3_START_ADD 12288
#define MEM_DISPLAY_LENGTH 16

#define LENGTH_OF_IO_LINE 70
#define IO_LINES 5
#define IO_COLS 35
#define IO_START_Y 22
#define IO_START_X 15
#define INPUT_Y_POS 23
#define INPUT_X_POS 5
#define OUTPUT_Y_POS 24
#define OUTPUT_X_POS 5

#define MEMORY_CYCLE 10
#define NUMBER_OF_STATES 5


#define MENU_CURSOR_Y_POS 20
#define MENU_CURSOR_X_POS 0


typedef unsigned short Register;

typedef struct ALU_s {
    Register A;
    Register B;
    Register R;
}ALU_s;

typedef ALU_s * ALU_p;

typedef struct CPU_s {
    Register prefetch[8];
    Register regFile[8];
    int head;
    int NOPCount;
    int FutureNOP;
    Register *R6Stack;
    Register IR;
    Register PC;
    Register MAR;
    Register MDR;
    Register CC;
	int BEN;
	ALU_p alu;
}CPU_s;

typedef struct FBUFFER {
    Register IR;
    Register PC;
    Register NOSTALL;
}FBuffer_s;

typedef struct DBUFFER {
    Register OP;
    Register DR;
    Register NZP;
    Register EXT_DATA1;
    Register EXT_DATA2;
    Register EXT_DATA3;
    Register PC;
    Register NOSTALL;
}DBuffer_s;

typedef struct EMBUFFER {
    Register OP;
    Register DR;
    Register NZP;
    Register RESULT;
    int halt;
    Register PC;
    Register NOSTALL;
} EBuffer_s, MBuffer_s;

typedef CPU_s * CPU_p;
typedef FBuffer_s * FBuffer_p;
typedef DBuffer_s * DBuffer_p;
typedef EBuffer_s * EBuffer_p;
typedef MBuffer_s * MBuffer_p;

#endif

/***********************************************************************
* TCSS 372 - Project                                                   *
* By: John Chang, Mustaf Dahir, Mohamed Mohamed, Harmandeep Singh      *
***********************************************************************/

#include "plc3.h"

FBuffer_p fbuffer;
DBuffer_p dbuffer;
EBuffer_p ebuffer;
MBuffer_p mbuffer;
Register memory[SIZE_OF_MEM];
int startMem = 0;
int breakPts[4] = {99999, 99999,99999,99999};

void pipelineController(CPU_p, int);
FBuffer_p fetch(CPU_p);
DBuffer_p decode(CPU_p, FBuffer_p);
EBuffer_p execute(CPU_p, DBuffer_p);
MBuffer_p mem(CPU_p, EBuffer_p);
void store(CPU_p, MBuffer_p);
void displayScreen(CPU_p c);
int checkForHazard(Register firstIR, Register secondIR);
void predecode(CPU_p cpu);

int valInArray(int val, int *arr, int size){
    int i;
    for (i = 0; i < size; i++){
        if (arr[i] == val)
            return 1;
    }
    return 0;
}

void presetBuffers() {
    fbuffer->IR = 0; 
    fbuffer->PC = 0;
    fbuffer->NOSTALL = 1;
    dbuffer->OP = BR;
    dbuffer->DR = 0;
    dbuffer->NZP = 0;
    dbuffer->EXT_DATA1 = 0;
    dbuffer->EXT_DATA2 = 0;
    dbuffer->EXT_DATA3 = 0;
    dbuffer->PC = 0;
    dbuffer->NOSTALL = 1;
    ebuffer->OP = 0;
    ebuffer->DR = 0;
    ebuffer->NZP = 0;
    ebuffer->RESULT = 0;
    ebuffer->PC = 0;
    ebuffer->NOSTALL = 1;
    ebuffer->halt = 0;
    mbuffer->OP = BR;
    mbuffer->DR = 0;
    mbuffer->NZP = 0;
    mbuffer->RESULT = 0;
    mbuffer->PC = 0;
    mbuffer->NOSTALL = 1;
}

Register sext(Register immed, int extend) 
{
	if (immed & extend) 
	{ 
		switch (extend) 
		{ 
			case EXT5:
				immed |= NEG5; 
				break;
			case EXT6:
				immed |= NEG6; 
				break;
			case EXT9:
				immed |= NEG9; 
				break;
			case EXT11:
				immed |= NEG11; 
				break;
		}
	} 
	return immed;
}


void initALU(ALU_p a) 
{
    a->A = 0; 
    a->B = 0; 
    a->R = 0; 
}

void initCPU (CPU_p c, ALU_p a) 
{
    Register *stack;

    c->regFile[0] = 0x4100;
    c->regFile[1] = 0x304D;
    c->regFile[2] = 0xFC31;
    c->regFile[3] = 0x102A;
    c->regFile[4] = 0x94CD;
    c->regFile[5] = 0x0FA0;
    c->regFile[6] = 0x4000;
    c->regFile[7] = 0x3100;
    
    c->alu = a; 
    c->PC = 0; 
    c->MAR = 0; 
    c->MDR = 0; 
    c->IR = 0; 
    c->head = 8;
    c->NOPCount = 0;
    c->FutureNOP = 0;
    c->CC = 0;

}

void pipelineController(CPU_p cpu, int mode) {
    //printf("Here");
    //The set of buffers used in the next pipeline cycle
    FBuffer_p nextFBuffer;
    DBuffer_p nextDBuffer;
    EBuffer_p nextEBuffer;
    MBuffer_p nextMBuffer;
	
    do {
        //check for breakpt
        if (mode == RUN_MODE && valInArray(cpu->PC, breakPts, 4))
            mode = STEP_MODE;
        nextFBuffer = fetch(cpu);
        nextDBuffer = decode(cpu, fbuffer);
        nextEBuffer = execute(cpu, dbuffer);
        if (nextEBuffer->halt)
            break;
        nextMBuffer = mem(cpu, ebuffer);
        store(cpu, mbuffer);
        fbuffer = nextFBuffer;
        dbuffer = nextDBuffer;
        ebuffer = nextEBuffer;
        mbuffer = nextMBuffer;
        if (cpu->NOPCount) 
            cpu->NOPCount--;

    } while (mode);
}

void predecode(CPU_p cpu) {
    Register firstIR, secondIR, thirdIR, fourthIR;
        firstIR = cpu->prefetch[cpu->head];
        secondIR = cpu->prefetch[cpu->head + 1];
        thirdIR = cpu->prefetch[cpu->head + 2];
        fourthIR = cpu->prefetch[cpu->head + 3];

        if (cpu->FutureNOP == 1) {
            if(checkForHazard(firstIR, thirdIR)) 
                cpu->NOPCount = 4;
            else 
                cpu->NOPCount = 3;
            cpu->FutureNOP = 0;
        } else if (cpu->FutureNOP == 2) {
            cpu->NOPCount = 2;
            cpu->FutureNOP = 0;
        } else if (cpu->head + 4 <= 8) {
            if (checkForHazard(firstIR, secondIR)) {
                cpu->NOPCount = 4;
            } else if(checkForHazard(firstIR, thirdIR)) {
                cpu->FutureNOP = 1;
            } else if (checkForHazard(firstIR, fourthIR)) {
                cpu->FutureNOP = 2;
            }
        }
}

int checkForHazard(Register firstIR, Register secondIR) {
    Register rd1, rd2, rs1, rs2, rs3, rs4, op1, op2, 
        firstSecondSourceCond, secondSecondSourceCond, trap, hazard = 0;
    rd1 = (firstIR & RD_FIELD) >> RD_FIELD_SHIFT;
    rd2 = (secondIR & RD_FIELD) >> RD_FIELD_SHIFT;
    rs1 = (RS_FIELD & secondIR) >> RS_FIELD_SHIFT; 
    rs2 = (RS2_FIELD & secondIR);
    rs3 = (RS_FIELD & firstIR) >> RS_FIELD_SHIFT; 
    rs4 = (RS2_FIELD & firstIR);
    op1 = (OPCODE_FIELD & firstIR) >> OPCODE_FIELD_SHIFT;
    op2 = (OPCODE_FIELD & secondIR) >> OPCODE_FIELD_SHIFT;
    firstSecondSourceCond = (SECOND_SOURCE_REGISTER_BIT & firstIR);
    secondSecondSourceCond = (SECOND_SOURCE_REGISTER_BIT & secondIR);

        switch (op2) {
        case ADD:
            if (secondSecondSourceCond == 0 && (rs1 == rd1 || rs2 == rd1)) 
                hazard = 1;
            else if (secondSecondSourceCond && rs1 == rd1)
                hazard = 1;
            break;
        case JSRR:
            if (rd1 == rs1)
                hazard = 1;
            break;
        case LEA:
            if (rd1 == rs1 || rd1 == rs2)
                hazard = 1;
            break;
        case STR:
            if (rd2 == rd1 || rs3 == rd1)
                hazard = 1;
            break;
        case AND:
            if (secondSecondSourceCond == 0 && (rs1 == rd1 || rs2 == rd1)) 
                hazard = 1;
            else if (secondSecondSourceCond && rs1 == rd1)
                hazard = 1;
            break;
        case NOT:
            if (rd1 == rs1)
                hazard = 1;
            break;
        case TRAP:
            trap = TRAP_VECT8_MASK & secondIR;
            if (trap == HALT)
                hazard = 1;
            else if ((trap == OUT || trap == GETCH || trap == PUTS)
                 && rd1 == 0)
                hazard = 1;
            break;
        case LD:
            if (rd2 == rs3 || rd2 == rs4)
                hazard = 1;
            break;
        case LDI:
            if (rd2 == rs3 || rd2 == rs4)
                hazard = 1;
            break;
        case ST:
            if (rd2 == rd1)
                hazard = 1;
            break;
        case STI:
            if (rd2 == rd1)
                hazard = 1;
            break;
        case JMP:
            if (rd1 == rs1)
                hazard = 1;
            break;
        case BR:
            //pass
            break;
        case PUSHPOP:
            if (op1 == op2)
                hazard = 1;
            if (secondSecondSourceCond == 0 && rd1 == rd2)
                hazard = 1;
            break;
    }

    return hazard;
}


FBuffer_p fetch(CPU_p cpu) {
    int i;
    FBuffer_p f = malloc(sizeof(FBuffer_s));

    if (cpu->NOPCount) {
        f->IR = 0;
        f->PC = 0;
        f->NOSTALL = 0;
    }else if (cpu->head > 4) {
        cpu->MAR = cpu->PC;
        for (i = 0; i < 8; i++) {
            cpu->MDR = memory[cpu->MAR];
            cpu->prefetch[i] = cpu->MDR;
            cpu->MAR++;
        }
        cpu->head = 0;
        cpu->NOPCount = MEMORY_CYCLE;
        f->IR = 0;
        f->PC = 0;
        f->NOSTALL = 0;
    } else {
        predecode(cpu);

        cpu->IR = cpu->prefetch[cpu->head];
        cpu->head++;
        
        f->IR = cpu->IR;
        f->PC = cpu->PC++;
        f->NOSTALL = 1;
    }
    return f;
}

DBuffer_p decode(CPU_p c, FBuffer_p f) {
    DBuffer_p d = malloc(sizeof(DBuffer_s));
    Register sr1, sr2, secondSourceCond, base_reg, offset9;

    if (c->NOPCount && f->NOSTALL == 0) {
        d->OP = BR;
        d->DR = 0;
        d->NZP = 0;
        d->EXT_DATA1 = 0;
        d->EXT_DATA2 = 0;
        d->EXT_DATA3 = 0;
        d->PC = 0;
        d->NOSTALL = 0;
        return d;
    } 

    d->OP = (f->IR & OPCODE_FIELD) >> OPCODE_FIELD_SHIFT;
    d->DR = (RD_FIELD & f->IR) >> RD_FIELD_SHIFT;
    sr1 = (RS_FIELD & f->IR) >> RS_FIELD_SHIFT;
    sr2 = (RS2_FIELD & f->IR);
    secondSourceCond = (SECOND_SOURCE_REGISTER_BIT & f->IR);
    d->NOSTALL = f->NOSTALL;

    switch (d->OP) {
        case ADD: 
            d->EXT_DATA1 = c->regFile[sr1];
            if (secondSourceCond)
                d->EXT_DATA2 = sext(f->IR & IMM5_MASK , EXT5);
            else 
                d->EXT_DATA2 = c->regFile[sr2];
            break;
        case JSRR:
            base_reg = (RS_FIELD & f->IR) >> RS_FIELD_SHIFT;
            d->EXT_DATA1 = c->regFile[base_reg];
            break;
        case LEA:
            d->EXT_DATA1 = sext(f->IR & OFFSET9_MASK , EXT9);
            break;
        case STR:
            d->EXT_DATA1 = sext(f->IR & OFFSET6_MASK , EXT6);
            base_reg = (RS_FIELD & f->IR) >> RS_FIELD_SHIFT;
            d->EXT_DATA2 = c->regFile[base_reg];
            d->EXT_DATA3 = (RD_FIELD & f->IR) >> RD_FIELD_SHIFT;
            break;
        case AND:
            if (secondSourceCond)
                d->EXT_DATA2 = sext(f->IR & IMM5_MASK , EXT5);
            else
                d->EXT_DATA2 = c->regFile[sr2];
            break;
        case NOT:
            break;
        case TRAP:
            d->EXT_DATA1 = f->IR & TRAP_VECT8_MASK;
            break;
        case LD: 
            d->EXT_DATA1 = sext(f->IR & OFFSET9_MASK, EXT9); 
            //c->MAR = f->PC + offset9; !DO IN EXECUTE!
            break;
        case LDI:
            d->EXT_DATA1 = sext(f->IR & OFFSET9_MASK, EXT9);
            break;
        case ST:
            d->EXT_DATA1 = sext(f->IR & OFFSET9_MASK, EXT9); 
            //c->MAR = f->PC + offset9; !DO IN EXECUTE!
            break;
        case STI:
            break;
        case JMP:
            c->regFile[7] = f->PC;
            d->PC = sr1;
            break;
        case BR:
            offset9 = sext(f->IR & OFFSET9_MASK, EXT9);
            if (f->IR & OFFSET9_SIGNED_BIT)
                offset9 *= -1;
            d->NZP = (RD_FIELD & f->IR) >> RD_FIELD_SHIFT;
            if (d->NZP)
                c->BEN = 1;
            else 
                c->BEN = 0;
            d->EXT_DATA1 = offset9;
            break;
        case PUSHPOP:
            d->EXT_DATA2 = secondSourceCond;
            if (!secondSourceCond)
                d->EXT_DATA1 = c->regFile[d->DR];
            break;
    }
    return d;
}

EBuffer_p execute(CPU_p c, DBuffer_p d) {
    EBuffer_p e = malloc(sizeof(EBuffer_s));
	int i, j;

    if (c->NOPCount && d->NOSTALL == 0) {
        e->OP = BR;
        e->DR = 0;
        e->NZP = 0;
        e->RESULT = 0;
        e->PC = 0;
        e->NOSTALL = 0;

        return e;
    }

    e->OP = d->OP;
    e->DR = d->DR;
    e->NOSTALL = d->NOSTALL;
    switch (d->OP) {
        case ADD:
            //result message
            e->RESULT = d->EXT_DATA1 + d->EXT_DATA2;
            if (e->RESULT== 0)
                e->NZP = 0;
            else if (e->RESULT >= 1)
                e->NZP = 1;
            else
                e->NZP = -1;
            break;
        case JSRR:
            e->PC = d->EXT_DATA1;
            c->regFile[7] = d->PC;
            break;
        case LEA:
            e->RESULT = d->PC + d->EXT_DATA1;
            if (c->regFile[d->DR] == 0)
                e->NZP = 0;
            else if (c->regFile[d->DR] >= 1)
                e->NZP = 1;
            else
                e->NZP = -1;
            break;
        case STR:
            c->MAR = d->EXT_DATA1 + d->EXT_DATA2;
            c->MDR = d->EXT_DATA3;
            break;
        case AND:
            e->RESULT = d->EXT_DATA1 & d->EXT_DATA2;
            if (e->RESULT == 0)
                e->NZP = 0;
            else if (e->RESULT >= 1)
                e->NZP = 1;
            else
                e->NZP = -1;
            break;
        case NOT:
            //result message
            e->RESULT = !(d->EXT_DATA1);
            if (e->RESULT == 0)
                e->NZP = 0;
            else if (e->RESULT >= 1)
                e->NZP = 1;
            else
                e->NZP = -1;
            break;
        case TRAP:
            switch(d->EXT_DATA1) {
                case GETCH:
                    scanw("%c", &c->regFile[0]);
                    //refresh();
                    //display(c);
                    getch();
                    break;
                case OUT:
                    mvprintw(20, 0, "%c", c->regFile[0]);
                    
                    //clrtoeol();
                    //refresh();
                    //getch();
                    break;
                case PUTS:
					
                    i = c->regFile[d->DR];
                    j = 0;
                    while (memory[i] != 0)
                    {
                        mvprintw(20, j, "%c", memory[i]);
                        
                        refresh();
                        i++;
                        j++;
                    }
                    getch();
                    break;
                case HALT:
                    mvprintw(20, 0, "Program has been halted.");
                    getch();
                    clrtoeol();
                    refresh();
                    e->halt = 1;
            }
            break;
        case LD:
            c->MAR = d->PC + d->EXT_DATA1;
            break;
        case LDI:
            c->MAR = d->PC + d->EXT_DATA1;
            break;
        case ST:
            c->MAR = d->PC + d->EXT_DATA1;
            break;
        case STI:
            break;
        case JMP:
            break;
        case BR:
            //result message
            if (c->BEN)
                e->PC = d->PC + d->EXT_DATA1;
            break;
        case PUSHPOP:
            if (d->EXT_DATA2) {
                c->regFile[6]--;
            } 
            c->MAR = c->regFile[6] - LC3_START_ADD;
            e->RESULT = d->EXT_DATA2;
            break;
    }

    return e;
}

MBuffer_p mem(CPU_p c, EBuffer_p e) {
    MBuffer_p m = malloc(sizeof(MBuffer_s));

    if (c->NOPCount && e->NOSTALL == 0) {
        m->OP = BR;
        m->DR = 0;
        m->NZP = 0;
        m->RESULT = 0;
        m->PC = 0;
        m->NOSTALL = 0;
        return m;
    }

    m = e;

    switch (e->OP) {
        case ADD:
            //pass
            break;
        case JSRR:
            //pass
            break;
        case LEA:
            //pass
            break;
        case STR:
            memory[c->MAR] = c->regFile[c->MDR];
            break;
        case AND:
            //pass
            break;
        case NOT:
            //result message
            c->regFile[e->DR] = e->RESULT;
            break;
        case TRAP:
            //pass
            break;
        case LD:
            //result message
            c->MDR = memory[c->MAR];
            c->NOPCount += MEMORY_CYCLE;
            if (c->MDR == 0)
                m->NZP = 0;
            else if (c->MDR >= 1)
                m->NZP = 1;
            else
                m->NZP = -1;
            break;
        case LDI:
            c->MDR = memory[c->MAR];
            c->NOPCount += MEMORY_CYCLE;
            c->MAR = c->MDR;
            c->MDR = memory[c->MAR];
            c->NOPCount += MEMORY_CYCLE;
            if (c->MDR == 0)
                m->NZP = 0;
            else if (c->MDR >= 1)
                m->NZP = 1;
            else
                m->NZP = -1;
            break;
        case ST:
            //result message
            c->MDR = c->regFile[e->DR];
            memory[c->MAR] = c->MDR;
            c->NOPCount += MEMORY_CYCLE;
            if (memory[c->MAR] == 0)
                m->NZP = 0;
            else if (memory[c->MAR] >= 1)
                m->NZP = 1;
            else
                m->NZP = -1;
            break;
        case STI:
            memory[c->MAR] = c->MDR;
            c->NOPCount += MEMORY_CYCLE;
            break;
        case JMP:
            break;
        case BR:
            //result message
            break;
        case PUSHPOP:
            if (e->RESULT == 0) {
                memory[c->MAR] = c->regFile[e->DR];
                c->NOPCount = MEMORY_CYCLE;
                c->regFile[6]++;
            }
            break;
    }

    return m;
}

void store(CPU_p c, MBuffer_p m) {
    if (c->NOPCount && m->NOSTALL == 0) {
        return;
    }

    switch (m->OP) {
        case ADD:
            //result message
            c->regFile[m->DR] = m->RESULT;
            break;
        case JSRR:
            //pass
            break;
        case LEA:
            c->regFile[m->DR] = m->RESULT;
            break;
        case STR:
            memory[c->MAR] = c->MDR;
            c->NOPCount += MEMORY_CYCLE;
            break;
        case AND:
            //result message
            c->regFile[m->DR] = m->RESULT;
            break;
        case NOT:
            //result message
            c->regFile[m->DR] = m->RESULT;
            break;
        case TRAP:
            //pass
            break;
        case LD:
            //result message
            c->regFile[m->DR] = c->MDR;  
            break;
        case LDI:
            c->regFile[m->DR] = c->MDR;
            break;
        case ST:
            //result message
            memory[c->MAR] =  c->MDR;
            c->NOPCount += MEMORY_CYCLE;
            break;
        case JMP:
            //pass
            break;
        case STI:
            //pass
            break;
        case BR:
            //pass
            break;
        case PUSHPOP:
            if (m->RESULT) {
                c->regFile[m->DR] = memory[c->MAR];
                c->NOPCount += MEMORY_CYCLE;
            }
            break;
    }
}

//Creates and returns a window for displaying of memory
WINDOW* displayMem(){
	int i, j = 0;
	initscr();
	WINDOW *mem = newwin(MEMORY_LINES, MEMORY_COLS, 
									MEMORY_START_Y, MEMORY_START_X);
	mvwprintw(mem, MEM_TITLE_Y_POS, MEM_TITLE_X_POS, "Memory");
	
	for (i = startMem; i < startMem + MEM_DISPLAY_LENGTH; i++){ 
		mvwprintw(mem, j + 2, 1, "0x%04X: 0x%04X", i + LC3_START_ADD, memory[i]);
		j++;
	}
	endwin();
	return mem;
}

//Creates and returns a window for displaying of registers
WINDOW* displayRegisters(CPU_p c){
	int i;
	initscr();
	WINDOW *reg = newwin(REG_FILE_LINES, REG_FILE_COLS, 
									REG_FILE_START_Y, REG_FILE_START_X);
									
	mvwprintw(reg, REG_TITLE_Y_POS, REG_TITLE_X_POS, "Registers");
	
	for (i = 0; i < NO_OF_REGISTERS; i++)
		mvwprintw(reg, i + 2, 1, "R%d: 0x%04X", i, c->regFile[i]);
	
	endwin();
	return reg;
}

//Creates and returns a window for displaying of fbuffer
WINDOW* displayFBUFF(){
	initscr();
	WINDOW *FBUFF = newwin(FBUFF_LINES, FBUFF_COLS, 
									FBUFF_START_Y, FBUFF_START_X);
    if (fbuffer->IR == 0) 
        mvwprintw(FBUFF, FBUFF_DATA_Y_POS, FBUFF_DATA_X_POS, "NOP");
    else
	   mvwprintw(FBUFF, FBUFF_DATA_Y_POS, FBUFF_DATA_X_POS, "PC: 0x%04X  IR: 0x%04X", fbuffer->PC + LC3_START_ADD, fbuffer->IR);
	endwin();
	return FBUFF;
	
}

//Creates and returns a window for displaying of dbuffer
WINDOW* displayDBUFF(){
	initscr();
	WINDOW *DBUFF = newwin(DBUFF_LINES, DBUFF_COLS, 
									DBUFF_START_Y, DBUFF_START_X);
    if (dbuffer->OP + dbuffer->DR + dbuffer->EXT_DATA1 == 0) {
        mvwprintw(DBUFF, DBUFF_HEADERS_Y_POS, DBUFF_HEADERS_X_POS, "NOP");
    }
    else {
	   mvwprintw(DBUFF, DBUFF_HEADERS_Y_POS, DBUFF_HEADERS_X_POS, "OP: DR: OPN1: OPN2: ");
	   mvwprintw(DBUFF, DBUFF_DATA_Y_POS, DBUFF_DATA_Y_POS, "x%d x%d 0x%04X 0x%04X", 
					dbuffer->OP, dbuffer->DR, dbuffer->EXT_DATA1, dbuffer->EXT_DATA2);
    }
	endwin();
	return DBUFF;
}

//Creates and returns a window for displaying of ebuffer
WINDOW* displayEBUFF(){
	initscr();
    WINDOW *EBUFF = newwin(EBUFF_LINES, EBUFF_COLS, 
                                        EBUFF_START_Y, EBUFF_START_X);
    if (ebuffer->OP + ebuffer->DR + ebuffer->RESULT == 0) {
        mvwprintw(EBUFF, EBUFF_HEADERS_Y_POS, EBUFF_HEADERS_X_POS, "NOP");
    }
    else {
    	mvwprintw(EBUFF, EBUFF_HEADERS_Y_POS, EBUFF_HEADERS_X_POS, "OP: DR: RESULT: ");
    	mvwprintw(EBUFF, EBUFF_DATA_Y_POS, EBUFF_DATA_X_POS, "x%d x%d 0x%04X", 
    					ebuffer->OP, ebuffer->DR, ebuffer->RESULT);
    }
	endwin();
	return EBUFF;
}

//Creates and returns a window for displaying of mbuffer
WINDOW* displayMBUFF(){
	initscr();
	WINDOW *MBUFF = newwin(MBUFF_LINES, MBUFF_COLS, 
									MBUFF_START_Y, MBUFF_START_X);
    if (mbuffer->OP + mbuffer->DR + mbuffer->RESULT == 0) {
        mvwprintw(MBUFF, MBUFF_HEADERS_Y_POS, MBUFF_HEADERS_X_POS, "NOP");
    }
    else {
    	mvwprintw(MBUFF, MBUFF_HEADERS_Y_POS, MBUFF_HEADERS_X_POS, "OP: DR: RESULT: ");
    	mvwprintw(MBUFF, MBUFF_DATA_Y_POS, MBUFF_DATA_X_POS, "x%d x%d 0x%04X", 
    					mbuffer->OP, mbuffer->DR, mbuffer->RESULT);
    }
	endwin();
	return MBUFF;
}

WINDOW* displayStore(){
	initscr();
	WINDOW *store = newwin(STORE_LINES, STORE_COLS, 
									STORE_START_Y, STORE_START_X);
									
	mvwprintw(store, STORE_INFO_Y_POS, STORE_INFO_X_POS, "STORE: 0x%04X in R%d", 0, 0);

    if (mbuffer->NZP > 0)
	mvwprintw(store, CC_INFO_Y_POS, CC_INFO_X_POS, "CC: N:%d Z:%d P:%d", 0, 0, 1);
    
    else if (mbuffer->NZP < 0)
        mvwprintw(store, CC_INFO_Y_POS, CC_INFO_X_POS, "CC: N:%d Z:%d P:%d", 1, 0, 0);
    else
        mvwprintw(store, CC_INFO_Y_POS, CC_INFO_X_POS, "CC: N:%d Z:%d P:%d", 0, 1, 0);
	
	endwin();
	return store;
	
}

//Creates and returns subwindow for i/o
WINDOW* displayIO(){
	initscr();
	WINDOW *io = newwin(IO_LINES, IO_COLS, 
									IO_START_Y, IO_START_X);

	endwin();
	return io;
	
}

int storeInMem(CPU_p c)
{
	FILE *infile;
	int value, i = 0;
	size_t len = 0;
	ssize_t read;
	char filename[15];
	char *line = NULL;
	
	mvprintw(MENU_CURSOR_Y_POS, MENU_CURSOR_X_POS, "File name: ");
	
	refresh();
	scanw("%s", &filename);
	infile = fopen(filename, "r");

	if (infile == NULL)
	{
		mvprintw(MENU_CURSOR_Y_POS, MENU_CURSOR_X_POS, "Error: File not found. Press <ENTER> to continue.");
		getch();
		refresh();
		return 0;
	}

	else
	{
		while ((read = getline(&line, &len, infile)) != -1) 
		{
			sscanf(line, "%X", &value);
			memory[i] = value;
			i++;
		}
	} 
	fclose(infile);
	return 1;
}


void save(CPU_p c){
	FILE *outfile;
	int value, i = 0, startAdd, endAdd, choice;
	size_t len = 0;
	ssize_t read;
	char filename[15];
	char *line = NULL;
	
	mvprintw(MENU_CURSOR_Y_POS, MENU_CURSOR_X_POS, "Enter the starting address to write: ");
	
	scanw("%X", &startAdd);
	
	move(MENU_CURSOR_Y_POS, MENU_CURSOR_Y_POS);
	
	clrtoeol();
	refresh();
	
	mvprintw(MENU_CURSOR_Y_POS, MENU_CURSOR_X_POS, "Enter the ending address to write: ");
	
	scanw("%X", &endAdd);
	
	move(MENU_CURSOR_Y_POS, MENU_CURSOR_Y_POS);
	
	clrtoeol();
	refresh();
	
	
	mvprintw(MENU_CURSOR_Y_POS, MENU_CURSOR_X_POS, "Enter the file name to write to: ");
	scanw("%s", filename);
	
	move(MENU_CURSOR_Y_POS, MENU_CURSOR_Y_POS);
	
	clrtoeol();
	refresh();
	
	startAdd-= LC3_START_ADD;
	endAdd-= LC3_START_ADD;
	
	if( access( filename, F_OK ) != -1 ) {
		mvprintw(MENU_CURSOR_Y_POS, MENU_CURSOR_X_POS, "File already exists, do you wish to overwrite? Enter 1 for yes, any other key for no: ");
		refresh();
		scanw("%d", &choice);
		if (choice)
		{
			outfile = fopen(filename, "w");
			for (i = startAdd; i <= endAdd; i++){	
				fprintf(outfile, "0x%04X\n", memory[i]);
				
			}
		}
	}
	
	else{
		outfile = fopen(filename, "w");
			for (i = startAdd; i <= endAdd; i++)	
				fprintf(outfile, "0x%04X\n", memory[i]);
			
	}
	
	refresh();
	fclose(outfile);
}

void showMem(){
	mvprintw(MENU_CURSOR_Y_POS, MENU_CURSOR_X_POS, "Starting Address: ");
	
	scanw("%X", &startMem);
	startMem-=LC3_START_ADD;
}

void edit(CPU_p c){

	int choiceAdd, choiceVal;
	mvprintw(MENU_CURSOR_Y_POS, MENU_CURSOR_X_POS, "Enter the address you want to edit: ");
	
	scanw("%X", &choiceAdd);
	
	clrtoeol();
	refresh();
	
	choiceAdd-=LC3_START_ADD;
	mvprintw(MENU_CURSOR_Y_POS, MENU_CURSOR_X_POS, "memory[%d] = 0x%04X", choiceAdd, memory[choiceAdd]);
	
	clrtoeol();
	refresh();
	
	mvprintw(MENU_CURSOR_Y_POS, MENU_CURSOR_X_POS, "Enter the desired value: ");
	scanw("%X", &choiceVal);
	
	memory[choiceAdd] = choiceVal;
	startMem = choiceAdd;

}

void breakPt(){
	int choice, i, contains = 0;
	mvprintw(MENU_CURSOR_Y_POS, MENU_CURSOR_X_POS, "Enter location to Set/Unset breakpoint: ");
	
	scanw("%X", &choice);
	choice-=LC3_START_ADD;
	
	for (i = 0; i < sizeof(breakPts); i++){
		if (breakPts[i] == choice){
			contains = 1;
			breakPts[i] = 99999;
		}
	}
	
	if (!contains){
		breakPts[3] = choice;
	}
}

int executeChoice(CPU_p c){
	int loaded = 0, i, choice;
	for (;;)
	{
		displayScreen(c);
		//Display cursor on its own line
		//refresh();
		//move(20, 0);
		clrtoeol();  
		//Read input and Branch
		scanw("%d", &choice);
		switch(choice)
		{
			case LOAD:
				for (i = 0; i < SIZE_OF_MEM; i++)
					memory[i] = 0;
				initALU(c->alu);
				initCPU(c, c->alu);
				presetBuffers();
				loaded = storeInMem(c);
				refresh();

				break;
			case SAVE:
				save(c);
				break;
			case STEP:
				if (!loaded)
				{
					mvprintw(MENU_CURSOR_Y_POS, MENU_CURSOR_X_POS, "Error: File must be loaded. Press <ENTER> to continue.");
					getch();
					clrtoeol();
					refresh();
					//displayScreen(c);
				}
				else
				{
					pipelineController(c, STEP_MODE);
					//displayScreen(c);
					refresh();
				}
				break;
			case RUN:
				if (!loaded)
				{
					mvprintw(MENU_CURSOR_Y_POS, MENU_CURSOR_X_POS, "Error: File must be loaded. Press <ENTER> to continue.");
					getch();
					clrtoeol();
					refresh();
					//displayScreen(c);
				}
				else
				{
					
					pipelineController(c, RUN_MODE);
					//displayScreen(c);
					refresh();
				}
				break;
			case SHOW:
				showMem();
				break;
			case EDIT:
				edit(c);
				break;
			case SetBrkpt:
				breakPt();
				break;
			case EXIT:
				return 0;

		}
	}
}


//Builds subwindows (e.g, register, memory) and puts them together to create debug monitor
void displayScreen(CPU_p c){
	char title[] = "Welcome to the LC-3 Pipeline Simulator";
	int choice;
	
	initscr();
	refresh();
	
	//Title
	mvprintw(SCR_TITLE_Y_POS, SCR_TITLE_X_POS, "%s", title);
	
	//Memory subwindow
	WINDOW* memoryDisplay = displayMem();
	//box(memoryDisplay, 0, 0);				//Allows you to actually see the window
	wrefresh(memoryDisplay);
	
	//Register subwindow
	WINDOW* registerDisplay = displayRegisters(c);
	//box(registerDisplay, 0, 0);				//Allows you to actually see the window
	wrefresh(registerDisplay);

	
	//Fbuff subwindow
	mvprintw(FBUFF_TITLE_Y_POS, FBUFF_TITLE_X_POS, "FBUFF:");
	WINDOW* fbuffDisplay = displayFBUFF();
	box(fbuffDisplay, 0, 0);				//Allows you to actually see the window
	wrefresh(fbuffDisplay);
	
	//Dbuff subwindow
	mvprintw(DBUFF_TITLE_Y_POS, DBUFF_TITLE_X_POS, "DBUFF:");
	WINDOW* dbuffDisplay = displayDBUFF();
	box(dbuffDisplay, 0, 0);				//Allows you to actually see the window
	wrefresh(dbuffDisplay);
	
	//Ebuff subwindow
	mvprintw(EBUFF_TITLE_Y_POS, EBUFF_TITLE_X_POS, "EBUFF:");
	WINDOW* ebuffDisplay = displayEBUFF();
	box(ebuffDisplay, 0, 0);				//Allows you to actually see the window
	wrefresh(ebuffDisplay);
	
	//Mbuff subwindow
	mvprintw(MBUFF_TITLE_Y_POS, MBUFF_TITLE_X_POS, "MBUFF:");
	WINDOW* mbuffDisplay = displayMBUFF();
	box(mbuffDisplay, 0, 0);				//Allows you to actually see the window
	wrefresh(mbuffDisplay);
	
	//Store subwindow
	WINDOW* storeDisplay = displayStore();
	//box(storeDisplay, 0, 0);				//Allows you to actually see the window
	wrefresh(storeDisplay);
	
	mvprintw(MENU_Y_POS, MENU_X_POS, "Select: 1) Load, 2) Save, 3) Step, 4) Run, 5) Display Mem, 6) Edit, 7)(UN) Set Brkpt, 8) Exit");
	
	//Draw line to separate option menu and input output box
	move(21, 0);
	hline(0, LENGTH_OF_IO_LINE);
	
	//I/O subwindow
	mvprintw(INPUT_Y_POS, INPUT_X_POS, "Input:");
	mvprintw(OUTPUT_Y_POS, OUTPUT_X_POS, "Output:");
	
	WINDOW* ioDisplay = displayIO();
	box(ioDisplay, 0, 0);
	wrefresh(ioDisplay);
	
	//Move cursor back
	move(MENU_CURSOR_Y_POS, MENU_CURSOR_X_POS);
	clrtoeol();
	
	refresh();
	
}

void test(CPU_p c){
	pipelineController(c, STEP_MODE);
}

int main() {
	
	int i;
	ALU_p alu_p = malloc(sizeof(ALU_s));
	CPU_p cpu_p = malloc(sizeof(CPU_s));
	
	fbuffer = malloc(sizeof(FBuffer_s));
	dbuffer = malloc(sizeof(DBuffer_s));
	ebuffer = malloc(sizeof(EBuffer_s));
	mbuffer = malloc(sizeof(MBuffer_s));
	
	
	for (i = 0; i < SIZE_OF_MEM; i++)
		memory[i] = 0;
	initALU(alu_p);
	initCPU(cpu_p, alu_p);
    //preset the first fbuffer before the pipeline starts
    presetBuffers();
	//test(cpu_p);
	displayScreen(cpu_p);
	executeChoice(cpu_p);
	endwin();

    return 0;
}
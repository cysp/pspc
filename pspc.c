#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curses.h>

#include "pspc.h"

extern char *__progname;

FILE *f;
size_t filesize;

#define EXTS(x) (((x) & 0x8000)?((x) | 0xFFFF0000):((x)))

/*{{{ getword */
unsigned int getword(cpu_state *cpu, unsigned int va) {
    unsigned int pa;
    unsigned int instr;
    unsigned int mask;
    int i;

    for (i = 0; i < 64; ++i) {
        mask = 0xFFFFFC00;

        if (!cpu->tlb[i].word0 & TLB_V_MASK)
            continue;
        /*
        if (cpu->tlb[i].word0.parts.TS != AS)
            continue;
        */
        if (cpu->tlb[i].TID != 0 && cpu->tlb[i].TID != (cpu->MMUCR & MMUCR_STID_MASK))
            continue;
        mask <<= ((cpu->tlb[i].word0 & TLB_SIZE_MASK) >> TLB_SIZE_SHIFT) * 2;
        if (((cpu->tlb[i].word0) & mask) == (va & mask))
            break;
    }

    if (i == 64) {
        printf(" TLB miss {%08x}", va);
        /* HACK */
    } else {
        printf(" <%d>", i);

        /* HACKIER HACK */
        if (cpu->tlb[i].word1)
            va = ((cpu->tlb[i].word1 & TLB_RPN_MASK) & mask) | (va & ~mask);
    }

    pa = filesize - (0 - va);
    //printf("fz: %08x va: %08x pa: %08x\n", filesize, va, pa);
    fseek(f, pa, SEEK_SET);

#if 1
    instr = 0;
    for (i = 0; i < 4; ++i) {
        int c = fgetc(f);
        instr |= c << (8 * (3-i));
    }
#else
    fread(&instr, 4, 1, f);
    instr ^= (instr << 24) & 0xFF000000;
    instr ^= (instr <<  8) & 0x00FF0000;
    instr ^= (instr >>  8) & 0x0000FF00;
    instr ^= (instr >> 24) & 0x000000FF;
#endif

    return instr;
}
/*}}}*/

/*{{{ opcode: nop */
int oc_nop(cpu_state *cpu, __attribute__((unused)) unsigned int data) {
    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 10 cmpli */
typedef struct {
    unsigned int ui : 16;
    unsigned int ra : 5;
    unsigned int un1 : 2;
    unsigned int bf : 3;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_cmpli;

int oc_cmpli(cpu_state *cpu, unsigned int data) {
    instr_cmpli instr;
    unsigned int a, b;
    char comp;

    *((unsigned int *)&instr) = data;

    printf(" (%s)", "cmpli");

    a = cpu->GPR[instr.ra];
    b = instr.ui;

    CR_CLEAR(instr.bf);
    if (a < b) {
        comp = '<';
        CR_SET(instr.bf, CR_LT);
    } else if (a > b) {
        comp = '>';
        CR_SET(instr.bf, CR_GT);
    } else {
        comp = '=';
        CR_SET(instr.bf, CR_EQ);
    }

    printf(" CR%d{%01x} = [%d]{%08x} %c %08x", instr.bf, (cpu->CR >> ((7 - instr.bf) * 4)) & 0xF, instr.ra, cpu->GPR[instr.ra], comp, b);

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 11 cmpi */
typedef struct {
    signed int ui : 16;
    unsigned int ra : 5;
    unsigned int un1 : 2;
    unsigned int bf : 3;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_cmpi;

int oc_cmpi(cpu_state *cpu, unsigned int data) {
    instr_cmpi instr;
    signed int a, b;
    char comp;

    *((unsigned int *)&instr) = data;

    printf(" (%s)", "cmpi");

    a = *(signed int *)&cpu->GPR[instr.ra];
    b = EXTS(instr.ui);

    CR_CLEAR(instr.bf);
    if (a < b) {
        comp = '<';
        CR_SET(instr.bf, CR_LT);
    } else if (a > b) {
        comp = '>';
        CR_SET(instr.bf, CR_GT);
    } else {
        comp = '=';
        CR_SET(instr.bf, CR_EQ);
    }

    printf(" CR%d{%01x} = [%d]{%08x} %c %08x", instr.bf, (cpu->CR >> ((7 - instr.bf) * 4)) & 0xF, instr.ra, cpu->GPR[instr.ra], comp, b);

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 14 addi */
typedef struct {
    unsigned int si : 16;
    unsigned int ra : 5;
    unsigned int rt : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_addi;

int oc_addi(cpu_state *cpu, unsigned int data) {
    instr_addi instr;
    unsigned int tmp;

    *((unsigned int *)&instr) = data;

    printf(" (%s)", "addi");

    if (instr.ra == 0) {
        tmp = EXTS(instr.si);
        printf(" [%d]{%08x} = %08x", instr.rt, tmp, EXTS(instr.si));
    } else {
        tmp = cpu->GPR[instr.ra] + EXTS(instr.si);
        printf(" [%d]{%08x} = [%d]{%08x} + %08x", instr.rt, tmp, instr.ra, cpu->GPR[instr.ra], EXTS(instr.si));
    }
    cpu->GPR[instr.rt] = tmp;

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 15 addis */
typedef struct {
    unsigned int si : 16;
    unsigned int ra : 5;
    unsigned int rt : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_addis;

int oc_addis(cpu_state *cpu, unsigned int data) {
    instr_addis instr;
    unsigned int tmp;

    *((unsigned int *)&instr) = data;

    printf(" (%s)", "addis");

    if (instr.ra == 0) {
        tmp = instr.si << 16;
        printf(" [%d]{%08x} = %08x", instr.rt, tmp, instr.si << 16);
    } else {
        tmp = cpu->GPR[instr.ra] + (instr.si << 16);
        printf(" [%d]{%08x} = [%d]{%08x} + %08x", instr.rt, tmp, instr.ra, cpu->GPR[instr.ra], instr.si << 16);
    }

    cpu->GPR[instr.rt] = tmp;

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 16 bc */
typedef struct {
    unsigned int lk : 1;
    unsigned int aa : 1;
    unsigned int bd : 14;
    unsigned int bi : 5;
    unsigned int bo : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_bc;

int oc_bc(cpu_state *cpu, unsigned int data) {
    unsigned int address;
    instr_bc instr;

    *((unsigned int *)&instr) = data;

    printf(" (%s%s%s)", "bc", (instr.lk)?("l"):(""), (instr.aa)?("a"):(""));

    if (instr.aa)
        address = EXTS(instr.bd << 2);
    else
        address = cpu->PC + EXTS(instr.bd << 2);

    if (instr.lk)
        cpu->LR = cpu->PC + 4;

    cpu->PC += 4;

    switch (instr.bo) {
        case 20: /* 10100b */
            printf(" always");
            cpu->PC = address;
            break;
        case 4: /* 00100b */
        case 5: /* 00101b */
            /* FALSE */
            printf(" !%02x", 1 << (4 - instr.bi));
            if (!(cpu->CR & (1 << (31 - instr.bi))))
                cpu->PC = address;
            break;
        case 12: /* 00100b */
        case 13: /* 00101b */
            /* TRUE */
            printf(" %02x", 1 << (4 - instr.bi));
            if (cpu->CR & (1 << (31 - instr.bi)))
                cpu->PC = address;
            break;
        case 16: /* 10000b */
        case 24: /* 11000b */
            cpu->CTR = (((cpu->CTR & 0xFFFFFFFF) - 1) & 0xFFFFFFFF);
            printf(" --CTR != 0 (%d)", cpu->CTR);
            if (cpu->CTR != 0)
                cpu->PC = address;
            break;
        case 18: /* 10010b */
        case 26: /* 11010b */
            cpu->CTR = (((cpu->CTR & 0xFFFFFFFF) - 1) & 0xFFFFFFFF);
            printf(" --CTR == 0 (%d)", cpu->CTR);
            if (cpu->CTR == 0)
                cpu->PC = address;
            break;
        default:
            printf(" unknown BO (%x)\n", instr.bo);
            exit(1);
    }

    printf(" addr: %08x", address);
    return 0;
}
/*}}}*/

/*{{{ opcode: 18 b */
typedef struct {
    unsigned int lk : 1;
    unsigned int aa : 1;
    unsigned int li : 24;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_b;

int oc_b(cpu_state *cpu, unsigned int data) {
    unsigned int address;
    instr_b instr;

    *((unsigned int *)&instr) = data;

    printf(" (%s%s%s)", "b", (instr.lk)?("l"):(""), (instr.aa)?("a"):(""));

    if (instr.aa) {
        address = instr.li << 2;
        if (address & 0x02000000)
            address |= 0xFC000000;
    } else {
        address = instr.li << 2;
        if (address & 0x02000000)
            address |= 0xFC000000;
        printf(" %08x", address);
        address = cpu->PC + address;
    }
    if (instr.lk) {
        cpu->LR = cpu->PC + 4;
    }
    printf(" addr: %08x", address);
    cpu->PC = address;
    return 0;
}
/*}}}*/

/*{{{ opcode: 19 16 bclr */
typedef struct {
    unsigned int lk : 1;
    unsigned int un2 : 10;
    unsigned int bh : 2;
    unsigned int un1 : 3;
    unsigned int bi : 5;
    unsigned int bo : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_bclr;

int oc_bclr(cpu_state *cpu, unsigned int data) {
    unsigned int address;
    instr_bclr instr;

    *((unsigned int *)&instr) = data;

    printf(" (%s%s)", "bclr", (instr.lk)?("l"):(""));

    if (instr.un1 != 0) {
        printf(" borked (%x)", instr.un1);
    }

    address = cpu->PC + 4;

    switch (instr.bo) {
        case 20: /* 10100b */
            printf(" always");
            address = cpu->LR;
            break;
        case 16: /* 10000b */
        case 24: /* 11000b */
            cpu->CTR = (((cpu->CTR & 0xFFFFFFFF) - 1) & 0xFFFFFFFF);
            printf(" --CTR != 0 (%d)", cpu->CTR);
            if (cpu->CTR != 0)
                address = cpu->LR;
            break;
        case 18: /* 10010b */
        case 26: /* 11010b */
            cpu->CTR = (((cpu->CTR & 0xFFFFFFFF) - 1) & 0xFFFFFFFF);
            printf(" --CTR == 0 (%d)", cpu->CTR);
            if (cpu->CTR == 0)
                address = cpu->LR;
            break;
        default:
            printf(" unknown BO (%x)\n", instr.bo);
            exit(1);
    }

    if (instr.lk) {
        cpu->LR = cpu->PC + 4;
    }

    cpu->PC = address;
    return 0;
}
/*}}}*/

/*{{{ opcode: 19 50 rfi */
typedef struct {
    unsigned int un3 : 1;
    unsigned int un2 : 10;
    unsigned int un1 : 15;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_rfi;

int oc_rfi(cpu_state *cpu, unsigned int data) {
    instr_rfi instr;

    *((unsigned int *)&instr) = data;

    printf("(%s)", "rfi");

    if (instr.un1 != 0 || instr.un3 != 0) {
        printf(" borked (%x %x)", instr.un1, instr.un3);
    }

    cpu->MSR = cpu->SRR1;
    cpu->PC = cpu->SRR0 & ~0x3;

    return 0;
}
/*}}}*/

/*{{{ opcode: 19 */
typedef struct {
    unsigned int un5 : 1;
    unsigned int xo : 10;
    unsigned int un4 : 2;
    unsigned int un3 : 3;
    unsigned int un2 : 5;
    unsigned int un1 : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_19;

int oc_19(cpu_state *cpu, unsigned int data) {
    instr_19 instr;

    *((unsigned int *)&instr) = data;

    switch (instr.xo) {
        case 16:
            oc_bclr(cpu, data);
            break;
        case 50:
            oc_rfi(cpu, data);
            break;
        case 150:
            printf(" (isync)");
            oc_nop(cpu, data);
            break;
        default:
            printf(" (19) unknown xo (%d)\n", instr.xo);
            exit(1);
    }

    return 0;
}
/*}}}*/

/*{{{ opcode: 20 rlwimi */
typedef struct {
    unsigned int rc : 1;
    unsigned int me : 5;
    unsigned int mb : 5;
    unsigned int sh : 5;
    unsigned int ra : 5;
    unsigned int rs : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_rlwimi;

int oc_rlwimi(cpu_state *cpu, unsigned int data) {
    instr_rlwimi instr;
    unsigned int rot, mask;
    unsigned int tmp;
    int i;

    *((unsigned int *)&instr) = data;

    printf(" (%s%s)", "rlwimi", (instr.rc)?("."):(""));

    rot = cpu->GPR[instr.rs] << instr.sh;
    mask = 0;
    if (instr.mb < instr.me + 1) {
        for (i = instr.mb; i < instr.me; ++i)
            mask |= 1 << i;
    } else {
        mask = 0xFFFFFFFF;
        if (instr.mb > instr.me + 1) {
            for (i = instr.mb - 1; i < instr.me + 1; ++i)
                mask &= ~(1 << i);
        }
    }

    tmp = (cpu->GPR[instr.ra] & ~mask) | (rot & mask);

    printf(" [%d]{%08x} = ([%d]{%08x} & %08x) | (([%d]{%08x} << %d) & %08x)", instr.ra, tmp, instr.ra, cpu->GPR[instr.ra], ~mask, instr.rs, cpu->GPR[instr.rs], instr.sh, mask);

    cpu->GPR[instr.ra] = tmp;

    if (instr.rc) {
        CR_CLEAR(0);
        if (cpu->GPR[instr.ra] & 0x80000000)
            CR_SET(0, CR_LT);
        else if (cpu->GPR[instr.ra])
            CR_SET(0, CR_GT);
        else
            CR_SET(0, CR_EQ);
    }

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 21 rlwinm */
typedef struct {
    unsigned int rc : 1;
    unsigned int me : 5;
    unsigned int mb : 5;
    unsigned int sh : 5;
    unsigned int ra : 5;
    unsigned int rs : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_rlwinm;

int oc_rlwinm(cpu_state *cpu, unsigned int data) {
    instr_rlwinm instr;
    unsigned int rot, mask;
    unsigned int tmp;
    int i;

    *((unsigned int *)&instr) = data;

    printf(" (%s%s)", "rlwinm", (instr.rc)?("."):(""));

    rot = cpu->GPR[instr.rs] << instr.sh;
    mask = 0;
    if (instr.mb < instr.me + 1) {
        for (i = instr.mb; i < instr.me; ++i)
            mask |= 1 << i;
    } else {
        mask = 0xFFFFFFFF;
        if (instr.mb > instr.me + 1) {
            for (i = instr.mb - 1; i < instr.me + 1; ++i)
                mask &= ~(1 << i);
        }
    }

    tmp = rot & mask;

    printf(" [%d]{%08x} = ([%d]{%08x} << %d) & %08x", instr.ra, tmp, instr.rs, cpu->GPR[instr.rs], instr.sh, mask);

    cpu->GPR[instr.ra] = tmp;

    if (instr.rc) {
        CR_CLEAR(0);
        if (cpu->GPR[instr.ra] & 0x80000000)
            CR_SET(0, CR_LT);
        else if (cpu->GPR[instr.ra])
            CR_SET(0, CR_GT);
        else
            CR_SET(0, CR_EQ);
    }

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 24 ori */
typedef struct {
    unsigned int ui : 16;
    unsigned int ra : 5;
    unsigned int rs : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_ori;

int oc_ori(cpu_state *cpu, unsigned int data) {
    instr_ori instr;
    unsigned int tmp;

    *((unsigned int *)&instr) = data;

    printf(" (%s)", "ori");

    tmp = cpu->GPR[instr.rs] | instr.ui;

    printf(" [%d]{%08x} = [%d]{%08x} | %08x", instr.ra, tmp, instr.rs, cpu->GPR[instr.rs], instr.ui);

    cpu->GPR[instr.ra] = tmp;

    CR_CLEAR(0);
    if (cpu->GPR[instr.ra] & 0x80000000)
        CR_SET(0, CR_LT);
    else if (cpu->GPR[instr.ra])
        CR_SET(0, CR_GT);
    else
        CR_SET(0, CR_EQ);

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 25 oris */
typedef struct {
    unsigned int ui : 16;
    unsigned int ra : 5;
    unsigned int rs : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_oris;

int oc_oris(cpu_state *cpu, unsigned int data) {
    instr_oris instr;
    unsigned int tmp;

    *((unsigned int *)&instr) = data;

    printf(" (%s)", "oris");

    tmp = cpu->GPR[instr.rs] | (instr.ui << 16);

    printf(" [%d]{%08x} = [%d]{%08x} | %08x", instr.ra, tmp, instr.rs, cpu->GPR[instr.rs], instr.ui << 16);

    cpu->GPR[instr.ra] = tmp;

    CR_CLEAR(0);
    if (cpu->GPR[instr.ra] & 0x80000000)
        CR_SET(0, CR_LT);
    else if (cpu->GPR[instr.ra])
        CR_SET(0, CR_GT);
    else
        CR_SET(0, CR_EQ);

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 28 andi */
typedef struct {
    unsigned int ui : 16;
    unsigned int ra : 5;
    unsigned int rs : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_andi;

int oc_andi(cpu_state *cpu, unsigned int data) {
    instr_andi instr;
    unsigned int tmp;

    *((unsigned int *)&instr) = data;

    printf(" (%s%s)", "andi", ".");

    tmp = cpu->GPR[instr.rs] & instr.ui;

    printf(" [%d]{%08x} = [%d]{%08x} & %08x", instr.ra, tmp, instr.rs, cpu->GPR[instr.rs], instr.ui);

    cpu->GPR[instr.ra] = tmp;

    CR_CLEAR(0);
    if (cpu->GPR[instr.ra] & 0x80000000)
        CR_SET(0, CR_LT);
    else if (cpu->GPR[instr.ra])
        CR_SET(0, CR_GT);
    else
        CR_SET(0, CR_EQ);

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 29 andis */
typedef struct {
    unsigned int ui : 16;
    unsigned int ra : 5;
    unsigned int rs : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_andis;

int oc_andis(cpu_state *cpu, unsigned int data) {
    instr_andis instr;
    unsigned int tmp;

    *((unsigned int *)&instr) = data;

    printf(" (%s%s)", "andis", ".");

    tmp = cpu->GPR[instr.rs] & (instr.ui << 16);

    printf(" [%d]{%08x} = [%d]{%08x} & %08x", instr.ra, tmp, instr.rs, cpu->GPR[instr.rs], instr.ui << 16);

    cpu->GPR[instr.ra] = tmp;

    CR_CLEAR(0);
    if (cpu->GPR[instr.ra] & 0x80000000)
        CR_SET(0, CR_LT);
    else if (cpu->GPR[instr.ra])
        CR_SET(0, CR_GT);
    else
        CR_SET(0, CR_EQ);

    printf(" {%08x}", cpu->CR);

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 31 0 cmp */
typedef struct {
    unsigned int un3 : 1;
    unsigned int un2 : 10;
    unsigned int rb : 5;
    unsigned int ra : 5;
    unsigned int un1 : 2;
    unsigned int bf : 3;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_cmp;

int oc_cmp(cpu_state *cpu, unsigned int data) {
    instr_cmp instr;
    signed int a, b;
    char comp;

    *((unsigned int *)&instr) = data;

    printf(" (%s)", "cmp");

    a = *((signed int *)&cpu->GPR[instr.ra]);
    b = *((signed int *)&cpu->GPR[instr.rb]);

    CR_CLEAR(instr.bf);
    if (a < b) {
        comp = '<';
        CR_SET(instr.bf, CR_LT);
    } else if (a > b) {
        comp = '>';
        CR_SET(instr.bf, CR_GT);
    } else {
        comp = '=';
        CR_SET(instr.bf, CR_EQ);
    }

    printf(" CR%d{%01x} = [%d]{%08x} %c [%d]{%08x}", instr.bf, (cpu->CR >> ((7 - instr.bf) * 4)) & 0xF, instr.ra, cpu->GPR[instr.ra], comp, instr.rb, cpu->GPR[instr.rb]);

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 31 8 subfc */
typedef struct {
    unsigned int rc : 1;
    unsigned int un1 : 9;
    unsigned int oe : 1;
    unsigned int rb : 5;
    unsigned int ra : 5;
    unsigned int rt : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_subfc;

int oc_subfc(cpu_state *cpu, unsigned int data) {
    instr_subfc instr;
    unsigned int tmp;

    *((unsigned int *)&instr) = data;

    printf(" (%s%s%s)", "subfc", (instr.oe)?("o"):(""), (instr.rc)?("."):(""));

    tmp = ~cpu->GPR[instr.ra] + cpu->GPR[instr.rb] + 1;

    printf(" [%d]{%08x} = [%d]{%08x} + [%d]{%08x} + 1", instr.rt, tmp, instr.ra, ~cpu->GPR[instr.ra], instr.rb, cpu->GPR[instr.rb]);

    cpu->GPR[instr.rt] = tmp;

    if (instr.oe) {
        printf("\n");
        exit(1);
    }

    if (instr.rc) {
        printf("\n");
        exit(1);
    }

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 31 28 and */
typedef struct {
    unsigned int rc : 1;
    unsigned int un1 : 10;
    unsigned int rb : 5;
    unsigned int ra : 5;
    unsigned int rs : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_and;

int oc_and(cpu_state *cpu, unsigned int data) {
    instr_and instr;
    unsigned int tmp;

    *((unsigned int *)&instr) = data;

    printf(" (%s%s)", "and", (instr.rc)?("."):(""));

    tmp = cpu->GPR[instr.rs] & cpu->GPR[instr.rb];

    printf(" [%d]{%08x} = [%d]{%08x} & [%d]{%08x}", instr.ra, tmp, instr.rs, cpu->GPR[instr.rs], instr.rb, cpu->GPR[instr.rb]);

    cpu->GPR[instr.ra] = tmp;

    if (instr.rc) {
        CR_CLEAR(0);
        if (cpu->GPR[instr.ra] & 0x80000000)
            CR_SET(0, CR_LT);
        else if (cpu->GPR[instr.ra])
            CR_SET(0, CR_GT);
        else
            CR_SET(0, CR_EQ);
    }

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 31 32 cmpl */
typedef struct {
    unsigned int un3 : 1;
    unsigned int un2 : 10;
    unsigned int rb : 5;
    unsigned int ra : 5;
    unsigned int un1 : 2;
    unsigned int bf : 3;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_cmpl;

int oc_cmpl(cpu_state *cpu, unsigned int data) {
    instr_cmpl instr;
    unsigned int a, b;
    char comp;

    *((unsigned int *)&instr) = data;

    printf(" (%s)", "cmpl");

    a = cpu->GPR[instr.ra];
    b = cpu->GPR[instr.rb];

    CR_CLEAR(instr.bf);
    if (a < b) {
        comp = '<';
        CR_SET(instr.bf, CR_LT);
    } else if (a > b) {
        comp = '>';
        CR_SET(instr.bf, CR_GT);
    } else {
        comp = '=';
        CR_SET(instr.bf, CR_EQ);
    }

    printf(" CR%d{%01x} = [%d]{%08x} %c [%d]{%08x}", instr.bf, (cpu->CR >> ((7 - instr.bf) * 4)) & 0xF, instr.ra, cpu->GPR[instr.ra], comp, instr.rb, cpu->GPR[instr.rb]);

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 31 83 mfmsr */
typedef struct {
    unsigned int un3 : 1;
    unsigned int un2 : 10;
    unsigned int un1 : 10;
    unsigned int rt : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_mfmsr;

int oc_mfmsr(cpu_state *cpu, unsigned int data) {
    instr_mfmsr instr;
    const char *dcr;

    *((unsigned int *)&instr) = data;

    printf(" (%s)", "mfmsr");

    cpu->GPR[instr.rt] = cpu->MSR;

    printf(" [%d]{%08x} <- msr", instr.rt, cpu->GPR[instr.rt]);

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 31 146 mtmsr */
typedef struct {
    unsigned int un3 : 1;
    unsigned int un2 : 10;
    unsigned int un1 : 10;
    unsigned int rt : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_mtmsr;

int oc_mtmsr(cpu_state *cpu, unsigned int data) {
    instr_mtmsr instr;
    const char *dcr;

    *((unsigned int *)&instr) = data;

    printf(" (%s)", "mtmsr");

    cpu->MSR = cpu->GPR[instr.rt];

    printf(" [%d]{%08x} <- msr", instr.rt, cpu->GPR[instr.rt]);

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 31 266 add */
typedef struct {
    unsigned int rc : 1;
    unsigned int un1 : 9;
    unsigned int oe : 1;
    unsigned int rb : 5;
    unsigned int ra : 5;
    unsigned int rs : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_add;

int oc_add(cpu_state *cpu, unsigned int data) {
    instr_add instr;
    unsigned int tmp;

    *((unsigned int *)&instr) = data;

    printf(" (%s%s%s)", "add", (instr.oe)?("o"):(""), (instr.rc)?("."):(""));

    tmp = cpu->GPR[instr.rs] + cpu->GPR[instr.rb];

    printf(" [%d]{%08x} = [%d]{%08x} + [%d]{%08x}", instr.ra, tmp, instr.rs, cpu->GPR[instr.rs], instr.rb, cpu->GPR[instr.rb]);

    cpu->GPR[instr.ra] = tmp;

    if (instr.oe) {
        printf("\n");
        exit(1);
    }

    if (instr.rc) {
        printf("\n");
        exit(1);
    }

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 31 323 mfdcr */
typedef struct {
    unsigned int un2 : 1;
    unsigned int un1 : 10;
    unsigned int dcr : 10;
    unsigned int rs : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_mfdcr;

int oc_mfdcr(cpu_state *cpu, unsigned int data) {
    instr_mfdcr instr;
    const char *dcr;

    *((unsigned int *)&instr) = data;

    printf(" (%s)", "mfdcr");

    switch (((instr.dcr & 0x1f) << 5) | ((instr.dcr >> 5) & 0x1f)) {
        case 0x00c:
            dcr = "CPR0_CFGADDR";
            cpu->GPR[instr.rs] = cpu->CPR0_CFGADDR;
            break;
        case 0x00d:
            dcr = "CPR0_CFGDATA";
            switch (cpu->CPR0_CFGADDR) {
                case 0x40:
                    dcr = "CPR0_PLLC";
                    cpu->GPR[instr.rs] = cpu->CPR0_PLLC;
                    break;
                default:
                    printf(" unknown CPR0_CFGADDR (%03x)\n", cpu->CPR0_CFGADDR);
                    exit(1);
            }
            break;
        case 0x00e:
            dcr = "SDR0_CFGADDR";
            cpu->GPR[instr.rs] = cpu->SDR0_CFGADDR;
            break;
        case 0x00f:
            dcr = "SDR0_CFGDATA";
            switch (cpu->SDR0_CFGADDR) {
                case 0x0060:
                    dcr = "SDR0_SDCS";
                    cpu->GPR[instr.rs] = cpu->SDR0_SDCS;
                    break;
                case 0x4100:
                    dcr = "SDR0_PFC0";
                    cpu->GPR[instr.rs] = cpu->SDR0_PFC0;
                    break;
                default:
                    printf(" unknown SDR0_CFGADDR (%03x)\n", cpu->SDR0_CFGADDR);
                    exit(1);
            }
            break;
        case 0x012:
            dcr = "EBC0_CFGADDR";
            cpu->GPR[instr.rs] = cpu->EBC0_CFGADDR;
            break;
        case 0x013:
            dcr = "EBC0_CFGDATA";
            switch (cpu->EBC0_CFGADDR) {
                case 0x00:
                    cpu->GPR[instr.rs] = cpu->EBC0_B0CR;
                    break;
                case 0x01:
                    cpu->GPR[instr.rs] = cpu->EBC0_B1CR;
                    break;
                default:
                    printf(" unknown EBC0_CFGADDR (%03x)\n", cpu->EBC0_CFGADDR);
                    exit(1);
            }
            break;
        default:
            printf(" unknown dcr (%03x)\n", ((instr.dcr & 0x1f) << 5) | ((instr.dcr >> 5) & 0x1f));
            exit(1);
    }

    printf(" [%d]{%08x} <- dcr(%s)", instr.rs, cpu->GPR[instr.rs], dcr);

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 31 339 mfspr */
typedef struct {
    unsigned int un2 : 1;
    unsigned int un1 : 10;
    unsigned int spr1 : 5;
    unsigned int spr2 : 5;
    unsigned int rs : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_mfspr;

int oc_mfspr(cpu_state *cpu, unsigned int data) {
    instr_mfspr instr;
    const char *spr;

    *((unsigned int *)&instr) = data;

    printf(" (%s)", "mfspr");

    switch (instr.spr1 << 5 | instr.spr2) {
        case 0x008: /* 00000 0 1000 */
            spr = "LR";
            cpu->GPR[instr.rs] = cpu->LR;
            break;
        case 0x009: /* 00000 0 1001 */
            spr = "CTR";
            cpu->GPR[instr.rs] = cpu->CTR;
            break;
        case 0x012: /* 00000 1 0010 */
            spr = "DSISR";
            cpu->GPR[instr.rs] = cpu->DSISR;
            break;
        case 0x013: /* 00000 1 0011 */
            spr = "DAR";
            cpu->GPR[instr.rs] = cpu->DAR;
            break;
        case 0x01A: /* 00000 1 1010 */
            spr = "SRR0";
            cpu->GPR[instr.rs] = cpu->SRR0;
            break;
        case 0x01B: /* 00000 1 1011 */
            spr = "SRR1";
            cpu->GPR[instr.rs] = cpu->SRR1;
            break;
        case 0x03A: /* 00001 1 1010 */
            spr = "CSRR0";
            cpu->GPR[instr.rs] = cpu->CSRR0;
            break;
        case 0x03B: /* 00001 1 1011 */
            spr = "CSRR1";
            cpu->GPR[instr.rs] = cpu->CSRR1;
            break;
        case 0x130: /* 01001 1 0000 */
            spr = "DBSR";
            cpu->GPR[instr.rs] = cpu->DBSR;
            break;
        case 0x3b2: /* 01001 1 1111 */
            spr = "MMUCR";
            cpu->GPR[instr.rs] = cpu->MMUCR;
            break;
        default:
            if ((instr.spr1 << 5 | instr.spr2) > 0x200) {
                printf(" reserved");
            }
            printf(" unknown spr (%03x)", instr.spr1 << 5 | instr.spr2);
            exit(1);
    }

    printf(" [%d]{%08x} <- spr(%s)", instr.rs, cpu->GPR[instr.rs], spr);

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 31 444 or */
typedef struct {
    unsigned int rc : 1;
    unsigned int un1 : 10;
    unsigned int rb : 5;
    unsigned int ra : 5;
    unsigned int rs : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_or;

int oc_or(cpu_state *cpu, unsigned int data) {
    instr_or instr;
    unsigned int tmp;

    *((unsigned int *)&instr) = data;

    printf(" (%s%s)", "or", (instr.rc)?("."):(""));

    tmp = cpu->GPR[instr.rs] | cpu->GPR[instr.rb];

    printf(" [%d]{%08x} = [%d]{%08x} | [%d]{%08x}", instr.ra, tmp, instr.rs, cpu->GPR[instr.rs], instr.rb, cpu->GPR[instr.rb]);

    cpu->GPR[instr.ra] = tmp;

    if (instr.rc) {
        CR_CLEAR(0);
        if (cpu->GPR[instr.ra] & 0x80000000)
            CR_SET(0, CR_LT);
        else if (cpu->GPR[instr.ra])
            CR_SET(0, CR_GT);
        else
            CR_SET(0, CR_EQ);
    }

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 31 451 mtdcr */
typedef struct {
    unsigned int un2 : 1;
    unsigned int un1 : 10;
    unsigned int dcr : 10;
    unsigned int rs : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_mtdcr;

int oc_mtdcr(cpu_state *cpu, unsigned int data) {
    instr_mtdcr instr;
    const char *dcr;

    *((unsigned int *)&instr) = data;

    printf(" (%s)", "mtdcr");

    switch (((instr.dcr & 0x1f) << 5) | ((instr.dcr >> 5) & 0x1f)) {
        case 0x00c:
            dcr = "CPR0_CFGADDR";
            cpu->CPR0_CFGADDR = cpu->GPR[instr.rs];
            break;
        case 0x00d:
            dcr = "CPR0_CFGDATA";
            switch (cpu->CPR0_CFGADDR) {
                case 0xa0:
                    dcr = "CPR0_PRIMBD";
                    cpu->CPR0_PRIMBD = cpu->GPR[instr.rs];
                    break;
                case 0x100:
                    dcr = "CPR0_MALD";
                    cpu->CPR0_MALD = cpu->GPR[instr.rs];
                    break;
                default:
                    printf(" unknown CPR0_CFGADDR (%03x)\n", cpu->CPR0_CFGADDR);
                    exit(1);
            }
            break;
        case 0x00e:
            dcr = "SDR0_CFGADDR";
            cpu->SDR0_CFGADDR = cpu->GPR[instr.rs];
            break;
        case 0x00f:
            dcr = "SDR0_CFGDATA";
            switch (cpu->SDR0_CFGADDR) {
                case 0x4100:
                    dcr = "SDR0_PFC0";
                    cpu->SDR0_PFC0 = cpu->GPR[instr.rs];
                    break;
                default:
                    printf(" unknown SDR0_CFGADDR (%03x)\n", cpu->SDR0_CFGADDR);
                    exit(1);
            }
            break;
        case 0x010:
            dcr = "SDRAM0_CFGADDR";
            cpu->SDRAM0_CFGADDR = cpu->GPR[instr.rs];
            break;
        case 0x011:
            dcr = "SDRAM0_CFGDATA";
            switch (cpu->SDRAM0_CFGADDR) {
                case 0x20:
                    dcr = "SDRAM0_CFG0";
                    cpu->SDRAM0_CFG0 = cpu->GPR[instr.rs];
                    break;
                default:
                    printf(" unknown SDRAM0_CFGADDR (%03x)\n", cpu->SDRAM0_CFGADDR);
                    exit(1);
            }
            break;
        case 0x012:
            dcr = "EBC0_CFGADDR";
            cpu->EBC0_CFGADDR = cpu->GPR[instr.rs];
            break;
        case 0x013:
            dcr = "EBC0_CFGDATA";
            switch (cpu->EBC0_CFGADDR) {
                case 0x00:
                    dcr = "EBC0_B0CR";
                    cpu->EBC0_B0CR = cpu->GPR[instr.rs];
                    break;
                case 0x01:
                    dcr = "EBC0_B1CR";
                    cpu->EBC0_B1CR = cpu->GPR[instr.rs];
                    break;
                case 0x02:
                    dcr = "EBC0_B2CR";
                    cpu->EBC0_B2CR = cpu->GPR[instr.rs];
                    break;
                case 0x03:
                    dcr = "EBC0_B3CR";
                    cpu->EBC0_B3CR = cpu->GPR[instr.rs];
                    break;
                case 0x04:
                    dcr = "EBC0_B4CR";
                    cpu->EBC0_B4CR = cpu->GPR[instr.rs];
                    break;
                case 0x05:
                    dcr = "EBC0_B5CR";
                    cpu->EBC0_B5CR = cpu->GPR[instr.rs];
                    break;
                case 0x06:
                    dcr = "EBC0_B6CR";
                    cpu->EBC0_B6CR = cpu->GPR[instr.rs];
                    break;
                case 0x07:
                    dcr = "EBC0_B7CR";
                    cpu->EBC0_B7CR = cpu->GPR[instr.rs];
                    break;
                case 0x23:
                    dcr = "EBC0_CFG";
                    cpu->EBC0_CFG = cpu->GPR[instr.rs];
                    break;
                default:
                    printf(" unknown EBC0_CFGADDR (%03x)\n", cpu->EBC0_CFGADDR);
                    exit(1);
            }
            break;
        case 0x014:
            dcr = "EBM0_CFGADDR";
            cpu->EBM0_CFGADDR = cpu->GPR[instr.rs];
            break;
        case 0x015:
            dcr = "EBM0_CFGDATA";
            switch (cpu->EBM0_CFGADDR) {
                case 0x00:
                    dcr = "EBM0_CTL";
                    cpu->EBM0_CTL = cpu->GPR[instr.rs];
                    break;
                default:
                    printf(" unknown EBM0_CFGADDR (%03x)\n", cpu->EBM0_CFGADDR);
                    exit(1);
            }
            break;
        case 0x0c0:
            dcr = "UIC0_SR";
            cpu->UIC0_SR = cpu->GPR[instr.rs];
            break;
        case 0x0c2:
            dcr = "UIC0_ER";
            cpu->UIC0_ER = cpu->GPR[instr.rs];
            break;
        case 0x0c3:
            dcr = "UIC0_CR";
            cpu->UIC0_CR = cpu->GPR[instr.rs];
            break;
        case 0x0c4:
            dcr = "UIC0_PR";
            cpu->UIC0_PR = cpu->GPR[instr.rs];
            break;
        case 0x0c5:
            dcr = "UIC0_TR";
            cpu->UIC0_TR = cpu->GPR[instr.rs];
            break;
        case 0x0d0:
            dcr = "UIC1_SR";
            cpu->UIC1_SR = cpu->GPR[instr.rs];
            break;
        case 0x0d2:
            dcr = "UIC1_ER";
            cpu->UIC1_ER = cpu->GPR[instr.rs];
            break;
        case 0x0d4:
            dcr = "UIC1_PR";
            cpu->UIC1_PR = cpu->GPR[instr.rs];
            break;
        case 0x0d5:
            dcr = "UIC1_TR";
            cpu->UIC1_TR = cpu->GPR[instr.rs];
            break;
        default:
            printf(" unknown dcr (%03x)\n", (((instr.dcr & 0x1f) << 5) | ((instr.dcr >> 5) & 0x1f)));
            exit(1);
    }

    printf(" [%d]{%08x} -> dcr(%s)", instr.rs, cpu->GPR[instr.rs], dcr);

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 31 476 mtspr */
typedef struct {
    unsigned int un2 : 1;
    unsigned int un1 : 10;
    unsigned int spr1 : 5;
    unsigned int spr2 : 5;
    unsigned int rs : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_mtspr;

int oc_mtspr(cpu_state *cpu, unsigned int data) {
    instr_mtspr instr;
    const char *spr;

    *((unsigned int *)&instr) = data;

    printf(" (%s)", "mtspr");

    switch (instr.spr1 << 5 | instr.spr2) {
        case 0x008: /* 00000 0 1000 */
            spr = "LR";
            cpu->LR = cpu->GPR[instr.rs];
            break;
        case 0x009: /* 00000 0 1001 */
            spr = "CTR";
            cpu->CTR = cpu->GPR[instr.rs];
            break;
        case 0x012: /* 00000 1 0010 */
            spr = "DSISR";
            cpu->DSISR = cpu->GPR[instr.rs];
            break;
        case 0x013: /* 00000 1 0011 */
            spr = "DAR";
            cpu->DAR = cpu->GPR[instr.rs];
            break;
        case 0x01a: /* 00000 1 1010 */
            spr = "SRR0";
            cpu->SRR0 = cpu->GPR[instr.rs];
            break;
        case 0x01b: /* 00000 1 1011 */
            spr = "SRR1";
            cpu->SRR1 = cpu->GPR[instr.rs];
            break;
        case 0x03a: /* 00001 1 1010 */
            spr = "CSRR0";
            cpu->CSRR0 = cpu->GPR[instr.rs];
            break;
        case 0x03b: /* 00001 1 1011 */
            spr = "CSRR1";
            cpu->CSRR1 = cpu->GPR[instr.rs];
            break;
        case 0x130: /* 01001 1 0000 */
            spr = "DBSR";
            cpu->DBSR = cpu->GPR[instr.rs];
            break;
        case 0x13f: /* 01001 1 1111 */
            spr = "DVC2";
            break;
        case 0x3b2: /* 01001 1 1111 */
            spr = "MMUCR";
            cpu->MMUCR = cpu->GPR[instr.rs];
            break;
        default:
            if ((instr.spr1 << 5 | instr.spr2) > 0x200) {
                printf(" reserved");
            }
            printf(" unknown spr (%03x)\n", instr.spr1 << 5 | instr.spr2);
            exit(1);
    }

    printf(" [%d]{%08x} -> spr(%s)", instr.rs, cpu->GPR[instr.rs], spr);

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 31 914 tlbsx */
typedef struct {
    unsigned int rc : 1;
    unsigned int un1 : 10;
    unsigned int rb : 5;
    unsigned int ra : 5;
    unsigned int rt : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_tlbsx;

int oc_tlbsx(cpu_state *cpu, unsigned int data) {
    instr_tlbsx instr;
    unsigned int address;
    int i;

    *((unsigned int *)&instr) = data;

    printf(" (%s%s)", "tlbsx", (instr.rc)?("."):(""));

    printf(" [%d]", instr.rt);
    printf(" [%d]", instr.ra);
    printf(" [%d]", instr.rb);

    if (instr.rc)
        CR_CLEAR(0);

    address = 0;
    if (instr.ra)
        address = cpu->GPR[instr.ra];

    address = address + EXTS(cpu->GPR[instr.rb]);

    for (i = 0; i < 64; ++i) {
        unsigned int mask = 0xFFFFFC00;

        if (!cpu->tlb[i].word0 & TLB_V_MASK)
            continue;

        printf("\n");
        /*
        if (cpu->tlb[i].word0.parts.TS != AS)
            continue;
        */
        if (cpu->tlb[i].TID != 0 && cpu->tlb[i].TID != (cpu->MMUCR & MMUCR_STID_MASK))
            continue;
        mask <<= ((cpu->tlb[i].word0 & TLB_SIZE_MASK) >> TLB_SIZE_SHIFT) * 2;
        printf(" %x", mask);
        printf(" %x", cpu->tlb[i].word0 & TLB_EPN_MASK);
        printf(" %x", address);
        if (((cpu->tlb[i].word0 & TLB_EPN_MASK) & mask) == (address & mask))
            break;
    }

    if (i != 64) {
        cpu->GPR[instr.rt] = i;
        CR_SET(0, CR_EQ);
        printf(" hit");
    } else
        printf(" miss");

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 31 978 tlbwe */
typedef struct {
    unsigned int un2 : 1;
    unsigned int un1 : 10;
    unsigned int ws : 5;
    unsigned int ra : 5;
    unsigned int rs : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_tlbwe;

int oc_tlbwe(cpu_state *cpu, unsigned int data) {
    instr_tlbwe instr;

    *((unsigned int *)&instr) = data;

    printf(" (%s)", "tlbwe");

    printf(" TLB([%d]{%08x})", instr.ra, cpu->GPR[instr.ra]);
    printf(" w%d", instr.ws);
    printf(" [%d]{%08x}", instr.rs, cpu->GPR[instr.rs]);

    switch (instr.ws) {
        case 0:
            cpu->tlb[cpu->GPR[instr.ra]].word0 = cpu->GPR[instr.rs] & 0xFFFFFFF0;
            printf(" EPN %x V %d TS %d SIZE %x", (cpu->GPR[instr.rs] & TLB_EPN_MASK), ((cpu->GPR[instr.rs] & TLB_V_MASK) >> TLB_V_SHIFT), ((cpu->GPR[instr.rs] & TLB_TS_MASK) >> TLB_TS_SHIFT), ((cpu->GPR[instr.rs] & TLB_SIZE_MASK) >> TLB_SIZE_SHIFT));
            cpu->tlb[instr.ra].TID = cpu->MMUCR & MMUCR_STID_MASK;
            break;
        case 1:
            cpu->tlb[cpu->GPR[instr.ra]].word1 = cpu->GPR[instr.rs] & 0xFFFC000F;
            printf(" RPN %x ERPN %x", (cpu->GPR[instr.rs] & TLB_RPN_MASK), (cpu->GPR[instr.rs] & TLB_ERPN_MASK) >> TLB_ERPN_SHIFT);
            break;
        case 2:
            cpu->tlb[cpu->GPR[instr.ra]].word2 = cpu->GPR[instr.rs] & 0x0000FFBF;
            break;
        default:
            printf(" borked ws (%d)\n", instr.ws);
            exit(1);
    }

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 31 */
typedef struct {
    unsigned int un2 : 1;
    unsigned int xo : 10;
    unsigned int spr : 10;
    unsigned int un1 : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_31;

int oc_31(cpu_state *cpu, unsigned int data) {
    instr_31 instr;
    int i;

    *((unsigned int *)&instr) = data;

    switch (instr.xo) {
        case 0:
            return oc_cmp(cpu, data);
        case 8:
            return oc_subfc(cpu, data);
        case 28:
            return oc_and(cpu, data);
        case 32:
            return oc_cmpl(cpu, data);
        case 83:
            return oc_mfmsr(cpu, data);
            /*
        case 86:
            printf(" (%s)", "dcbf");
            oc_nop(cpu, data);
            break;
            */
        case 146:
            return oc_mtmsr(cpu, data);
        case 266:
            return oc_add(cpu, data);
        case 323:
            return oc_mfdcr(cpu, data);
        case 339:
            return oc_mfspr(cpu, data);
        case 444:
            return oc_or(cpu, data);
        case 451:
            return oc_mtdcr(cpu, data);
        case 454:
            printf(" (dccci)");
            oc_nop(cpu, data);
            break;
        case 467:
            return oc_mtspr(cpu, data);
        case 598:
            printf(" (sync)");
            oc_nop(cpu, data);
            break;
        case 914:
            return oc_tlbsx(cpu, data);
        case 966:
            printf(" (iccci)");
            oc_nop(cpu, data);
            break;
        case 978:
            return oc_tlbwe(cpu, data);
        default:
            printf(" unknown xo (%d) spr (", instr.xo);
            for (i = 5; i < 10; ++i)
                printf("%c", (instr.spr & (1 << (9-i)))?('1'):('0'));
            printf(" ");
            for (i = 0; i < 5; ++i)
                printf("%c", (instr.spr & (1 << (9-i)))?('1'):('0'));
            printf(") (%d)\n", instr.spr);
            exit(1);
    }

    return 0;
}
/*}}}*/

/*{{{ opcode: 32 lwz */
typedef struct {
    signed int d : 16;
    unsigned int ra : 5;
    unsigned int rt : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_lwz;

int oc_lwz(cpu_state *cpu, unsigned int data) {
    instr_lwz instr;
    unsigned int address;
    unsigned int tmp;

    *((unsigned int *)&instr) = data;

    printf(" (%s)", "lwz");

    if (instr.ra == 0) {
        address = EXTS(instr.d);
        tmp = getword(cpu, address);
        printf(" [%d]{%08x} = ({%08x})", instr.rt, tmp, address);
    } else {
        address = cpu->GPR[instr.ra] + EXTS(instr.d);
        tmp = getword(cpu, address);
        printf(" [%d]{%08x} = ([%d]{%08x} + %08x){%08x}", instr.rt, tmp, instr.ra, cpu->GPR[instr.ra], EXTS(instr.d), address);
    }

    cpu->GPR[instr.rt] = tmp;

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 33 lwzu */
typedef struct {
    signed int d : 16;
    unsigned int ra : 5;
    unsigned int rt : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_lwzu;

int oc_lwzu(cpu_state *cpu, unsigned int data) {
    instr_lwzu instr;
    unsigned int address;
    unsigned int tmp;

    *((unsigned int *)&instr) = data;

    printf(" (%s)", "lwzu");

    if (!instr.ra || instr.ra == instr.rt)
        printf(" borked");

    address = cpu->GPR[instr.ra] + EXTS(instr.d);

    tmp = getword(cpu, address);

    printf(" [%d]{%08x} = ([%d]{%08x} + %08x){%08x}", instr.rt, tmp, instr.ra, cpu->GPR[instr.ra], EXTS(instr.d), address);

    cpu->GPR[instr.ra] = address;

    cpu->GPR[instr.rt] = tmp;

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 36 stw */
typedef struct {
    signed int d : 16;
    unsigned int ra : 5;
    unsigned int rs : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_stw;

int oc_stw(cpu_state *cpu, unsigned int data) {
    instr_stw instr;
    unsigned int address;

    *((unsigned int *)&instr) = data;

    printf("(%s)", "stw");

    address = 0;
    if (instr.ra)
        address = cpu->GPR[instr.ra];

    address = address + EXTS(instr.d);

    printf(" [%d]{%08x} = [%d]{%08x}", instr.ra, address, instr.rs, cpu->GPR[instr.rs]);

    /* mem[address] = cpu->GPR[instr.rs]; */

    cpu->GPR[instr.ra] = address;

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcode: 37 stwu */
typedef struct {
    signed int d : 16;
    unsigned int ra : 5;
    unsigned int rs : 5;
    unsigned int opcode : 6;
} __attribute__((__packed__)) instr_stwu;

int oc_stwu(cpu_state *cpu, unsigned int data) {
    instr_stwu instr;
    unsigned int address;

    *((unsigned int *)&instr) = data;

    printf("(%s)", "stwu");

    if (!instr.ra)
        printf(" borked");

    address = cpu->GPR[instr.ra];

    address = address + EXTS(instr.d);

    printf(" [%d]{%08x} = [%d]{%08x}", instr.ra, address, instr.rs, cpu->GPR[instr.rs]);

    /* mem[address] = cpu->GPR[instr.rs]; */

    cpu->GPR[instr.ra] = address;

    cpu->PC += 4;
    return 0;
}
/*}}}*/

/*{{{ opcodes */
struct {
    char *mnemonic;
    int (*func)(cpu_state *, unsigned int);
} opcodes[64] = {
    /* 00 */ { "",       NULL },
    /* 01 */ { "",       NULL },
    /* 02 */ { "",       NULL },
    /* 03 */ { "twi",    NULL },
    /* 04 */ { "",       NULL },
    /* 05 */ { "",       NULL },
    /* 06 */ { "",       NULL },
    /* 07 */ { "mulli",  NULL },
    /* 08 */ { "subfic", NULL },
    /* 09 */ { "",       NULL },
    /* 10 */ { "cmpli",  oc_cmpli },
    /* 11 */ { "cmpi",   oc_cmpi },
    /* 12 */ { "addic",  NULL },
    /* 13 */ { "addic.", NULL },
    /* 14 */ { "addi",   oc_addi },
    /* 15 */ { "addis",  oc_addis },
    /* 16 */ { "bc",     oc_bc },
    /* 17 */ { "sc",     NULL },
    /* 18 */ { "b" ,     oc_b },
    /* 19 */ { "19",     oc_19 },
    /* 20 */ { "rlwimi", oc_rlwimi },
    /* 21 */ { "rlwinm", oc_rlwinm },
    /* 22 */ { "",       NULL },
    /* 23 */ { "rlwnm",  NULL },
    /* 24 */ { "ori",    oc_ori },
    /* 25 */ { "oris",   oc_oris },
    /* 26 */ { "xori",   NULL },
    /* 27 */ { "xoris",  NULL },
    /* 28 */ { "andi",   oc_andi },
    /* 29 */ { "andis",  oc_andis },
    /* 30 */ { "",       NULL },
    /* 31 */ { "31",     oc_31 },
    /* 32 */ { "lwz",    oc_lwz },
    /* 33 */ { "lwzu",   oc_lwzu },
    /* 34 */ { "lbz",    NULL },
    /* 35 */ { "lbzu",   NULL },
    /* 36 */ { "stw",    oc_stw },
    /* 37 */ { "stwu",   oc_stwu },
    /* 38 */ { "stb",    NULL },
    /* 39 */ { "",       NULL },
    /* 40 */ { "lhz",    NULL },
    /* 41 */ { "lhzu",   NULL },
    /* 42 */ { "lha",    NULL },
    /* 43 */ { "lhau",   NULL },
    /* 44 */ { "sth",    NULL },
    /* 45 */ { "sthu",   NULL },
    /* 46 */ { "lmw",    NULL },
    /* 47 */ { "stmw",   NULL },
    /* 48 */ { "lfs",    NULL },
    /* 49 */ { "lfsu",   NULL },
    /* 50 */ { "lfd",    NULL },
    /* 51 */ { "lfdu",   NULL },
    /* 52 */ { "stfs",   NULL },
    /* 53 */ { "stfsu",  NULL },
    /* 54 */ { "stfd",   NULL },
    /* 55 */ { "stfdu",  NULL },
    /* 56 */ { "lfq",    NULL },
    /* 57 */ { "lfqu",   NULL },
    /* 58 */ { "",       NULL },
    /* 59 */ { "",       NULL },
    /* 60 */ { "",       NULL },
    /* 61 */ { "",       NULL },
    /* 62 */ { "",       NULL },
    /* 63 */ { "",       NULL },
};
/*}}}*/

/*{{{ usage */
void usage(void) {
    fprintf(stderr, "Usage: %s file.bin\n", __progname);
    exit(1);
}
/*}}}*/

/*{{{ main */
int main(int argc, char *argv[]) {
    cpu_state cpu;

    if (argc != 2)
        usage();

    f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "Cannot open input binary\n");
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    filesize = ftell(f);

    memset(&cpu, 0, sizeof(cpu));

    cpu.PC = 0xFFFFFFFC;
    cpu.tlb[0].word0 = (0x3FFFFC << TLB_EPN_SHIFT) | (0x1 << TLB_V_SHIFT) | (0x1 << TLB_SIZE_SHIFT);
    cpu.tlb[0].TID = 0;
    cpu.tlb[0].word1 = (0x3FFFFC << TLB_RPN_SHIFT) | (0x01 << TLB_ERPN_SHIFT);
    cpu.EBC0_B0CR = 0xFFE28000;

    cpu.tlb[63].word0 = (0x3FC000 << TLB_EPN_SHIFT) | (0x1 << TLB_V_SHIFT) | (0x1 << TLB_SIZE_SHIFT);
    cpu.tlb[63].TID = 0;
    cpu.tlb[63].word1 = (0x3FC000 << TLB_RPN_SHIFT) | (0x01 << TLB_ERPN_SHIFT);

    for (;;) {
        unsigned int instr;

        /* XXX */
        /*
        if (cpu.PC == 0xfffff110 ||
            cpu.PC == 0xfffff130)
            cpu.PC += 4;
        */
        instr = getword(&cpu, cpu.PC);

        printf(" [%08x]", cpu.PC);
        printf(" [%08x]", instr);

        printf(" [%hd]", OPCODE(instr));

        if (opcodes[OPCODE(instr)].func)
            opcodes[OPCODE(instr)].func(&cpu, instr);
        else {
            printf(" (%s) unhandled opcode\n", opcodes[OPCODE(instr)].mnemonic);
            exit(1);
        }

        printf("\n");
    }

    return 0;
}
/*}}}*/

#ifndef PSPC_H
#define PSPC_H

struct tlb_entry {
    unsigned int TID : 8;
#define TLB_EPN_MASK   0xFFFFFC00
#define TLB_EPN_SHIFT  10 
#define TLB_V_MASK     0x00000200
#define TLB_V_SHIFT    9
#define TLB_TS_MASK    0x00000100
#define TLB_TS_SHIFT   8
#define TLB_SIZE_MASK  0x000000F0
#define TLB_SIZE_SHIFT 4
#define TLB_TPAR_MASK  0x0000000F
#define TLB_TPAR_SHIFT 0
    unsigned int word0;
#define TLB_RPN_MASK   0xFFFFFC00
#define TLB_RPN_SHIFT  10 
#define TLB_PAR1_MASK  0x000000F0
#define TLB_PAR1_SHIFT 4
#define TLB_ERPN_MASK  0x0000000F
#define TLB_ERPN_SHIFT 0
    unsigned int word1;
    unsigned int word2;
};

typedef struct cpu_state {
    unsigned int PC;
    unsigned int MSR;
    unsigned int GPR[32];
    unsigned int CR;
    unsigned int XER;
    unsigned int LR;
    signed int CTR;
    unsigned int DSISR;
    unsigned int DAR;
    unsigned int DBSR;
    unsigned long long FPR[32];
    unsigned int SPR[1024];
    unsigned int SRR0;
    unsigned int SRR1;
    unsigned int CSRR0;
    unsigned int CSRR1;
#define MMUCR_STS_MASK   0x00010000
#define MMUCR_STS_SHIFT  16
#define MMUCR_STID_MASK  0x0000000F
#define MMUCR_STID_SHIFT 0
    unsigned int MMUCR;

    struct tlb_entry tlb[64];

    /* dcr 0x0c */
    unsigned int CPR0_CFGADDR;
    unsigned int CPR0_PLLC; /* 0x40 */
    unsigned int CPR0_PRIMBD; /* 0xa0 */
    unsigned int CPR0_MALD; /* 0x100 */

    /* dcr 0x0e */
    unsigned int SDR0_CFGADDR;
    unsigned int SDR0_SDCS; /* 0x0060 */
    unsigned int SDR0_PFC0; /* 0x4100 */

    /* dcr 0x10 */
    unsigned int SDRAM0_CFGADDR;
    unsigned int SDRAM0_CFG0; /* 0x20 */

    /* dcr 0x12 */
    unsigned int EBC0_CFGADDR;
    unsigned int EBC0_B0CR; /* 0x00 */
    unsigned int EBC0_B1CR; /* 0x01 */
    unsigned int EBC0_B2CR; /* 0x02 */
    unsigned int EBC0_B3CR; /* 0x03 */
    unsigned int EBC0_B4CR; /* 0x04 */
    unsigned int EBC0_B5CR; /* 0x05 */
    unsigned int EBC0_B6CR; /* 0x06 */
    unsigned int EBC0_B7CR; /* 0x07 */
    unsigned int EBC0_CFG;  /* 0x23 */

    /* dcr 0x14 */
    unsigned int EBM0_CFGADDR;
    unsigned int EBM0_CTL; /* 0x00 */

    /* dcr 0xc0 */
    unsigned int UIC0_SR;

    /* dcr 0xc2 */
    unsigned int UIC0_ER;

    /* dcr 0xc3 */
    unsigned int UIC0_CR;

    /* dcr 0xc4 */
    unsigned int UIC0_PR;

    /* dcr 0xc5 */
    unsigned int UIC0_TR;

    /* dcr 0xd0 */
    unsigned int UIC1_SR;

    /* dcr 0xd2 */
    unsigned int UIC1_ER;

    /* dcr 0xd4 */
    unsigned int UIC1_PR;

    /* dcr 0xd5 */
    unsigned int UIC1_TR;
} cpu_state;

#define OPCODE(i) ((i) >> 26)

#define CR_LT 0x8
#define CR_GT 0x4
#define CR_EQ 0x2
#define CR_SO 0x1

#define CR_CLEAR(n) do { \
    cpu->CR &= ~(0xFF << ((7-(n))*4));\
} while (0)

#define CR_SET(n, b) do { \
    cpu->CR |= ((b) << ((7-(n))*4));\
} while (0)

#endif

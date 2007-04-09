#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern char *__progname;

#define OPCODE(i) ((i) >> 26)

unsigned int count[64] = { 0 };

void usage(void) {
    fprintf(stderr, "Usage: %s file.bin\n", __progname);
    exit(1);
}

int main(int argc, char *argv[]) {
    FILE *f;
    int i;

    if (argc != 2)
        usage();

    f = fopen(argv[0], "rb");
    if (!f) {
        fprintf(stderr, "horked\n");
        return 1;
    }

    for (;;) {
        unsigned int instr = 0;
        for (i = 0; i < 4; ++i) {
            int c = fgetc(f);
            instr |= c << (8 * (3-i));
        }
        if (feof(f))
            break;

        ++count[OPCODE(instr)];
    }

    fclose(f);

    for (i = 0; i < 64; ++i) {
        if (count[i])
            printf("[%02d] (%s) %d\n", i, "", count[i]);
    }

    return 0;
}

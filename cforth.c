// C implementation of a simple forth machine structure

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

// Need this to determine which are atomic vs, non-atomic functions
// This isn't the normal way to do it, but will match how the 8051 works.
// On that system, it needs to know where it is earlier than running the
// the actual word because it is in a different memory location.
#pragma GCC optimize ("align-functions=16")

// This is implementing a simple fixed forth word to perform the following:
// : fib  ( n -- )  1 1 rot 0 do dup rot dup . + loop drop drop ; 
// : idea#1         10 fib ;

typedef    void*(*fword)(void);

fword* i_ptr = NULL;

#define FORTH_WORD(wp)       (fword)((uint32_t)(wp) + 1)
#define FORTH_LIT(x)         (fword)(x)

void* atom_next (void)
{
    uintptr_t tmp = (uintptr_t)*i_ptr;
    i_ptr++;

    if (tmp & 0x01) {
        printf("Forth Word Found (%d): ", tmp);
        // Now we would push down, but I don't have a stack implementation yet.

        // So just clear the bit before calling it.
        tmp &= ~0x01;
        
        return (void*)tmp;
    } else {
        return (void*)tmp;
    }
}

void* atom_literal (void)
{
    // Interpret the next location as a number, print it and skip.
    int num = (intptr_t)*i_ptr;
    printf("%d\n", num);
    i_ptr++;

    return atom_next();

}

void* atom_exit (void)
{
    printf("EXIT\n");
    return NULL;
}

void* atom_ten (void)
{
    printf("TEN\n");
    return atom_next();
}

void* fib (void)
{
    printf("FIB\n");
    return atom_next();
}




fword pgm_1[] = {
    atom_literal,
    FORTH_LIT(10),
    FORTH_WORD(fib),
    atom_exit
};
        



int main (void)
{
    // Set to the start of the program
    i_ptr = pgm_1;

    // Returns the next word to run
    fword next_word = atom_next();

    // Run until done.
    while (next_word != NULL) {

        next_word = next_word();
    }

    return 0;
}




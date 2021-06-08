// C implementation of a simple forth machine structure

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// Need this to determine which are atomic vs, non-atomic functions
// This isn't the normal way to do it, but will match how the 8051 works.
// On that system, it needs to know where it is earlier than running the
// the actual word because it is in a different memory location.
#pragma GCC optimize ("align-functions=16")

// This is implementing a simple fixed forth word to perform the following:
// : fib  ( n -- )  1 1 rot 0 do dup rot dup . + loop drop drop ; 
// : pgm_1         10 fib ;

typedef    void*(*fword)(void);

fword* i_ptr = NULL;

#define get_shift()    3*(tos - BASE_OF_STACK)

#define FORTH_WORD(wp)       (fword)((uint32_t)(wp) + 1)
#define FORTH_LIT(x)         (fword)(x)

#define PRINT_ADDR
#ifdef PRINT_ADDR
#define print_fn(p)  printf("%p: %*s%s\n", p, get_shift(), "", __func__)
#define print_fn_msg(p,m)  printf("%p: %*s%s %s\n", p, get_shift(), "", __func__, m)
#else
#define print_fn(p)  printf("%*s%s\n", get_shift(), "", __func__)
#define print_fn_msg(p,m)  printf("%*s%s %s\n", get_shift(), "", __func__, m)
#endif

#define BASE_OF_STACK   10
uintptr_t return_stack[1000];
unsigned int tos;

inline static void push_r (fword* v)
{
    return_stack[++tos] = (uintptr_t)v;
}

inline static fword* pop_r (void)
{
    return (fword*)return_stack[tos--];
}


void* atom_next (void)
{
    uintptr_t tmp = (uintptr_t)*i_ptr;
    i_ptr++;

    if (tmp & 0x01) {
        // Clear the flag before calling it.
        tmp--;

        push_r(i_ptr);
        i_ptr = (fword*)tmp;

        return atom_next();
    } else {
        return (void*)tmp;
    }
}

void* atom_literal (void)
{
    char numstr[20];

    // Interpret the next location as a number, print it and skip.
    int num = (intptr_t)*i_ptr;

    sprintf(numstr, "%d", num);
    print_fn_msg(atom_literal, numstr);

    i_ptr++;

    return atom_next();

}

void* atom_exit (void)
{
    print_fn(atom_exit);

    i_ptr = pop_r();

    if (i_ptr != NULL) {
        return atom_next();
    } else {
        return NULL;
    }
}

void* atom_do (void)
{
    void* next_instruction;

    print_fn(atom_do);
    push_r(i_ptr);

    return atom_next();
}
 
void* atom_loop (void)
{
    char tmpstr[40];

    fword* loop_ptr = pop_r();

    sprintf(tmpstr, "to %p", *loop_ptr);

    print_fn_msg(atom_loop, tmpstr);



    return atom_next();
}
 

#define CREATE_PLACEHOLDER(fn)      \
void* fn (void)                     \
{                                   \
    print_fn(fn);                   \
    return atom_next();             \
}                                   \
                                    \

CREATE_PLACEHOLDER(atom_rot)
CREATE_PLACEHOLDER(atom_dup)
CREATE_PLACEHOLDER(atom_drop)
CREATE_PLACEHOLDER(atom_plus)
CREATE_PLACEHOLDER(atom_dot)

// : fib  ( n -- )  1 1 rot 0 do dup rot dup . + loop drop drop ; 

fword test[] = {
    atom_literal,
    FORTH_LIT(1),
    atom_literal,
    FORTH_LIT(2),
    atom_literal,
    FORTH_LIT(3),
    atom_literal,
    FORTH_LIT(4),
    atom_exit
};


fword fib[] = {
    atom_literal, 
    FORTH_LIT(1),
    atom_literal, 
    FORTH_LIT(1),
    atom_rot,
    atom_literal, 
    FORTH_LIT(0),
    atom_do,
    atom_dup,
    atom_rot,
    atom_dup,
    FORTH_WORD(test),
    atom_dot,
    atom_plus,
    atom_loop,
    atom_drop,
    atom_drop,
    atom_exit
};
 

// : pgm_1         10 fib ;
fword pgm_1[] = {
    atom_literal,
    FORTH_LIT(10),
    FORTH_WORD(fib),
    atom_exit
};



int main (void)
{
    // Init machine
    memset(return_stack, 0, sizeof(return_stack));
    tos = BASE_OF_STACK;

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




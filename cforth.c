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
void print_ds(void);
void print_fn_impl (void* fp, const char* msg, const char* fname);

#define get_shift()    3*(tors - BASE_OF_STACK)

#define FORTH_WORD(wp)       (fword)((uint32_t)(wp) + 1)
#define FORTH_LIT(x)         (fword)(x)

#if 1
#define print_fn(p)         print_fn_impl (p, "", __func__)
#define print_fn_msg(p,m)   print_fn_impl (p, m, __func__)
#else
#define print_fn(p)
#define print_fn_msg(p,m)
#endif

bool enable_print_ds = true;


#define BASE_OF_STACK   10
uintptr_t return_stack[1000];
uintptr_t data_stack[1000];
unsigned int tors;
unsigned int tods;

#define PRINT_BUF_SIZE  40
void print_fn_impl (void* fp, const char* msg, const char* fname)
{
    char str[PRINT_BUF_SIZE];
    int len;

    // Space fill buffer and null terminate
    memset(str, ' ', sizeof(str));
    str[PRINT_BUF_SIZE - 1] = '\0';

    len = snprintf(str, PRINT_BUF_SIZE, "%p: %*s%s %s", fp, get_shift(), "", fname, msg);
    
    if (enable_print_ds) {
        str[len] = ' '; // Extend spaces to end of buffer.
        str[PRINT_BUF_SIZE - 1] = '\0';
    }

    printf("%s", str);

    if (enable_print_ds) {
        print_ds();
    }

}



inline static void push_r (fword* v)
{
    return_stack[++tors] = (uintptr_t)v;
}

inline static fword* pop_r (void)
{
    return (fword*)return_stack[tors--];
}

inline static void push_d (intptr_t v)
{
    data_stack[++tods] = v;
}
    
inline static intptr_t pop_d (void)
{
    return data_stack[tods--];
}

void print_ds(void)
{
    int idx = tods;

    printf("     TOS ---> ");

    for (idx = tods; idx>BASE_OF_STACK; idx--)
    {
        int dat = (int)data_stack[idx];

        printf ("%3d ", dat);
    }
    puts("");
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

    push_d(num);

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
    print_fn(atom_do);
    push_r(i_ptr);

    push_r((void*)pop_d());  // Limit
    push_r((void*)pop_d());  // Start

    return atom_next();
}
 
void* atom_loop (void)
{
    char tmpstr[40];

    pop_r();    // Start
    pop_r();    // Limit
    fword* loop_ptr = pop_r();

    sprintf(tmpstr, "to %p", *loop_ptr);

    print_fn_msg(atom_loop, tmpstr);



    return atom_next();
}
 
void* atom_rot (void)
{
    uintptr_t tmp = data_stack[tods];

    tmp                  = data_stack[tods - 2];
    data_stack[tods - 2] = data_stack[tods - 1];
    data_stack[tods - 1] = data_stack[tods];
    data_stack[tods]     = tmp;

    print_fn(atom_rot);
    return atom_next();
}

void* atom_dup (void)
{
    push_d (data_stack[tods]);

    print_fn(atom_dup);
    return atom_next();
}

void* atom_drop ()
{
    pop_d ();

    print_fn(atom_drop);
    return atom_next();
}

void* atom_plus (void)
{
    int a, b;
    
    a = (int)pop_d();
    b = (int)pop_d();
    a += b;
    push_d((intptr_t)a);
    
    print_fn(atom_plus);
    return atom_next();
}

void* atom_dot ()
{
    char numstr[20];
    int val;

    val = pop_d ();

    sprintf(numstr, "print: %d", val);
    print_fn_msg(atom_literal, numstr);

    print_fn(atom_dot);
    return atom_next();
}



#define CREATE_PLACEHOLDER(fn)      \
void* fn (void)                     \
{                                   \
    print_fn(fn);                   \
    return atom_next();             \
}                                   \
                                    \


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
//    FORTH_WORD(test),
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
    tors = BASE_OF_STACK;
    tods = BASE_OF_STACK;

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




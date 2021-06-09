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
int print_ds(char* str, int len);
int print_rs(char* str, int len);
void print_fn_impl (void* fp, const char* msg, const char* out, const char* fname);

#define get_shift()    3*(tors - BASE_OF_STACK)

#define FORTH_WORD(wp)       (fword)((uint32_t)(wp) + 1)
#define FORTH_LIT(x)         (fword)(x)

#if 1
#define print_fn(p)         print_fn_impl (p, "", "", __func__)
#define print_fn_msg(p,m)   print_fn_impl (p, m, "", __func__)
#define print_fn_out(p,m)   print_fn_impl (p, "", m, __func__)
#else
#define print_fn(p)
#define print_fn_msg(p,m)
#define print_fn_out(p,m)
#endif

bool enable_print_addr = true;
bool enable_print_opcode = true;
bool enable_stack_shift = true;
bool enable_print_ds = true;
bool enable_print_rs = true;


#define BASE_OF_STACK   10
uintptr_t return_stack[1000];
uintptr_t data_stack[1000];
unsigned int tors;
unsigned int tods;
#define INPUT_BUFFER_SIZE 256
char input_buffer[INPUT_BUFFER_SIZE];
unsigned int input_offset;


#define PRINT_BUF_SIZE  120
void print_fn_impl (void* fp, const char* msg, const char* out, const char* fname)
{
    char str[PRINT_BUF_SIZE];
    int start_col = 0;
    int null_idx = 0;
    int len;

    // Space fill buffer and null terminate
    memset(str, ' ', sizeof(str));
    str[PRINT_BUF_SIZE - 1] = '\0';

    if (enable_print_addr) {
        len = snprintf(str, PRINT_BUF_SIZE, "%p: ", fp);
        null_idx = start_col + len;
        start_col += len;
    }

    if (enable_stack_shift) {
        str[null_idx] = ' '; // Remove the null to leave full buffer available.
        len = snprintf(str+start_col, PRINT_BUF_SIZE - start_col, "%*s", get_shift(), "");
        null_idx = start_col + len;
        start_col += len;
    }

    if (enable_print_opcode) {
        str[null_idx] = ' '; // Remove the null to leave full buffer available.
        len = snprintf(str+start_col, PRINT_BUF_SIZE - start_col, "%s %s", fname, msg);
        null_idx = start_col + len;
        start_col += len;
    }

    str[null_idx] = ' '; // Remove the null to leave full buffer available.
    len = snprintf(str+start_col, PRINT_BUF_SIZE - start_col, "%s", out);
    null_idx = start_col + len;

    // Start tables in a fixed column.
    start_col = 50;
    
    if (enable_print_ds) {
        str[null_idx] = ' '; // Remove the null to leave full buffer available.
        len = print_ds(str+start_col, PRINT_BUF_SIZE - start_col);
        null_idx = start_col + len;
        start_col += 30;
    }

    if (enable_print_rs) {
        str[null_idx] = ' '; // Remove the null to leave full buffer available.
        len = print_rs(str+start_col, PRINT_BUF_SIZE - start_col);
        null_idx = start_col + len;
    }

    // Make sure the full buffer is terminated
    str[PRINT_BUF_SIZE - 1] = '\0';

    if (null_idx > 0) {
        printf("%s\n", str);
    }
}

int print_ds(char* str, int len)
{
    int used = 0;
    int idx;

    used += snprintf (str, len, "     TOS ---> ");

    for (idx = tods; idx>BASE_OF_STACK; idx--)
    {
        int dat = (int)data_stack[idx];

        if (used >= len) {
            break;
        }

        used += snprintf (str+used, len - used, "%3d ", dat);
    }

    return used;
}

int print_rs(char* str, int len)
{
    int used = 0;
    int idx;

    used += snprintf (str, len, "     TOS ---> ");

    for (idx = tors; idx>BASE_OF_STACK; idx--)
    {
        unsigned int dat = (unsigned int)return_stack[idx];

        if (used >= len) {
            break;
        }

        used += snprintf (str+used, len - used, "%d ", dat);
    }

    return used;
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

    push_r((void*)pop_d());  // Index
    push_r((void*)pop_d());  // Limit

    return atom_next();
}
 
void* atom_loop (void)
{
    char tmpstr[40];
    fword* loop_ptr;


    // do {} while (++index < limit)

    if (++return_stack[tors-1] < return_stack[tors]) {
        loop_ptr = (fword*)return_stack[tors-2];
        
        i_ptr = loop_ptr;   // Jump back
 
        sprintf(tmpstr, "back to %p", *loop_ptr);
    } else {
        intptr_t idx, lim;
        lim = (int)pop_r();    // Limit
        idx = (int)pop_r();    // Index
        pop_r();
        strcpy (tmpstr, "loop exit");
    }

    print_fn_msg(atom_loop, tmpstr);



    return atom_next();
}
 
void* atom_begin (void)
{
    print_fn(atom_begin);
    push_r(i_ptr);

    return atom_next();
}
 
void* atom_while (void)
{
    char tmpstr[40];
    fword* loop_ptr;

    int val = (int)pop_d();

    if (val) {
        loop_ptr = (fword*)return_stack[tors];
        
        i_ptr = loop_ptr;   // Jump back
 
        sprintf(tmpstr, "back to %p", *loop_ptr);
    } else {
        pop_r();
        strcpy (tmpstr, "loop exit");
    }

    print_fn_msg(atom_while, tmpstr);

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

void* atom_swap (void)
{
    uintptr_t tmp = data_stack[tods];

    data_stack[tods] = data_stack[tods - 1];
    data_stack[tods-1]     = tmp;

    print_fn(atom_swap);
    return atom_next();
}

void* atom_dup (void)
{
    push_d (data_stack[tods]);

    print_fn(atom_dup);
    return atom_next();
}

void* atom_not (void)
{
    data_stack[tods] = !(data_stack[tods]);

    print_fn(atom_not);
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

void* atom_subtract (void)
{
    int a, b;
    
    a = (int)pop_d();
    b = (int)pop_d();
    b -= a;
    push_d((intptr_t)b);
    
    print_fn(atom_subtract);
    return atom_next();
}



void* atom_dot ()
{
    char numstr[20];
    int val;

    val = pop_d ();

    sprintf(numstr, "out: %d", val);
    print_fn_out(atom_literal, numstr);

    print_fn(atom_dot);
    return atom_next();
}

void* atom_if (void)
{
    char numstr[20];
    int val;

    int jmp = (intptr_t)*i_ptr;

    val = pop_d ();

    if (val)
    {
        i_ptr++;
    }
    else
    {
        i_ptr += jmp;
    }


    sprintf(numstr, "(%d) %d", jmp, val);
    print_fn_msg(atom_if, numstr);

    return atom_next();
}

void* atom_else (void)
{
    char numstr[20];

    int jmp = (intptr_t)*i_ptr;

    i_ptr += jmp;


    sprintf(numstr, "(%d)", jmp);
    print_fn_msg(atom_else, numstr);

    return atom_next();
}

void* atom_input (void)
{
    input_offset = 0;
    input_buffer[0]='\0';
    fgets(input_buffer, sizeof(input_buffer), stdin);
    // Strip newlines.
    input_buffer[strcspn(input_buffer, "\n\r")] = ' ';

    print_fn_msg(atom_input, input_buffer);
    return atom_next();
}




#define CREATE_PLACEHOLDER(fn)      \
void* fn (void)                     \
{                                   \
    print_fn(fn);                   \
    return atom_next();             \
}                                   \
                                    \

CREATE_PLACEHOLDER(atom_find_word);
CREATE_PLACEHOLDER(atom_execute);
CREATE_PLACEHOLDER(atom_number);
CREATE_PLACEHOLDER(atom_lex);

#include "pgm_data.h"
    

int main (void)
{
    // Init machine
    memset(return_stack, 0, sizeof(return_stack));
    tors = BASE_OF_STACK;
    tods = BASE_OF_STACK;
    input_buffer[0] = '\0';

    // Set to the start of the program
    i_ptr = test_input;

    // Returns the next word to run
    fword next_word = atom_next();

    // Run until done.
    while (next_word != NULL) {

        next_word = next_word();
    }

    return 0;
}




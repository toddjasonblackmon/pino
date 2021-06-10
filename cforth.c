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

#define get_shift()         3*(tors - BASE_OF_STACK)

#define FORTH_WORD(wp)      (fword)((uint32_t)(wp) + 1)
#define FORTH_LIT(x)        (fword)(x)

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


void* atom_next (void);
void* atom_literal (void);
void* atom_exit (void);
void* atom_do (void);
void* atom_loop (void);
void* atom_begin (void);
void* atom_while (void);
void* atom_again (void);
void* atom_rot (void);
void* atom_swap (void);
void* atom_dup (void);
void* atom_not (void);
void* atom_drop (void);
void* atom_plus (void);
void* atom_subtract (void);
void* atom_dot (void);
void* atom_if (void);
void* atom_else (void);
void* atom_input (void);
void* atom_emit (void);
void* atom_lex (void);
void* atom_bye (void);
void* atom_execute (void);
void* atom_number (void);

bool check_stack_underflows(void);




typedef struct {
    uint32_t next;
    char name[8];
    fword fn;
} native_fword;

#define NATIVE_ENTRY(n, name, fn)   {n, name, fn}

native_fword native_dictionary[] = {
    NATIVE_ENTRY(0,  "",         NULL),
    NATIVE_ENTRY(0,  "next",     atom_next),
    NATIVE_ENTRY(1,  "literal",  atom_literal),
    NATIVE_ENTRY(2,  "exit",     atom_exit),
    NATIVE_ENTRY(3,  "do",       atom_do),
    NATIVE_ENTRY(4,  "loop",     atom_loop),
    NATIVE_ENTRY(5,  "begin",    atom_begin),
    NATIVE_ENTRY(6,  "while",    atom_while),
    NATIVE_ENTRY(7,  "rot",      atom_rot),
    NATIVE_ENTRY(8,  "swap",     atom_swap),
    NATIVE_ENTRY(9,  "dup",      atom_dup),
    NATIVE_ENTRY(10, "not",      atom_not),
    NATIVE_ENTRY(11, "drop",     atom_drop),
    NATIVE_ENTRY(12, "+",        atom_plus),
    NATIVE_ENTRY(13, "-",        atom_subtract),
    NATIVE_ENTRY(14, ".",        atom_dot),
    NATIVE_ENTRY(15, "if",       atom_if),
    NATIVE_ENTRY(16, "else",     atom_else),
    NATIVE_ENTRY(17, "input",    atom_input),
    NATIVE_ENTRY(18, "emit",     atom_emit),
    NATIVE_ENTRY(19, "lex",      atom_lex),
    NATIVE_ENTRY(20, "bye",      atom_bye),
    NATIVE_ENTRY(21, "execute",  atom_execute),
    NATIVE_ENTRY(22, "again",    atom_again),
    NATIVE_ENTRY(23, "number",   atom_number),

};

unsigned int end_of_dict = sizeof(native_dictionary)/sizeof(native_fword) - 1;





#define BASE_OF_STACK   10
#define MAX_STACK_SIZE 1000
uintptr_t return_stack[1000];
uintptr_t data_stack[1000];
unsigned int tors;
unsigned int tods;
#define INPUT_BUFFER_SIZE 256
char input_buffer[INPUT_BUFFER_SIZE];
unsigned int input_offset;
bool compiler_state = false;    // true = Compiler, false = Interpreter

void check_stack_range(void)
{
    if (tors < BASE_OF_STACK) {
        printf("Return stack underflow by %d entries\n", BASE_OF_STACK - tors);
        exit(1);
    }

    if (tods < BASE_OF_STACK) {
        printf("Data stack underflow by %d entries\n", BASE_OF_STACK - tods);
        exit(1);
    }

    if (tors >= MAX_STACK_SIZE) {
        printf("Return stack overflow by %d entries\n", tors - (MAX_STACK_SIZE - 1));
        exit(1);
    }

    if (tods >= MAX_STACK_SIZE) {
        printf("Data stack overflow by %d entries\n", tods - (MAX_STACK_SIZE - 1));
        exit(1);
    }
}





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

    fflush(stdout);
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

void* atom_again (void)
{
    char tmpstr[40];
    fword* loop_ptr;


    loop_ptr = (fword*)return_stack[tors];

    i_ptr = loop_ptr;   // Jump back

    sprintf(tmpstr, "back to %p", *loop_ptr);

    print_fn_msg(atom_again, tmpstr);

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
    char* ret;

    input_offset = 0;
    input_buffer[0]='\0';

    ret = fgets(input_buffer, sizeof(input_buffer), stdin);
    if (ret == NULL) {
        atom_bye(); // End of input file
    }

    // Strip newlines.
    input_buffer[strcspn(input_buffer, "\n\r")] = ' ';

    print_fn_msg(atom_input, input_buffer);
    return atom_next();
}

void* atom_emit ()
{
    char val;

    val = pop_d ();

    print_fn(atom_emit);
    printf("%c ", val);
    fflush(stdout);

    return atom_next();
}

void* atom_lex ()
{
    bool string_start = (input_offset == 0);
    char* tok;
    char tokstr[20];

    // Parse the next token, push address of next token on stack
    // If none seen, push 0. (NULL)
    // Update the input pointer

    if (string_start) {
        tok = strtok(input_buffer, "\r\n\t ");
    } else {
        tok = strtok(NULL, "\r\n\t ");
    }

    push_d((intptr_t)tok);

    if (tok == NULL) {
        tok = "NIL";
    } else {
        input_offset += strlen(tok);
    }

    snprintf(tokstr, sizeof(tokstr), "\"%s\"", tok);

    print_fn_msg(atom_lex, tokstr);
    return atom_next();
}

void* atom_find_word (void)
{
    char* word_to_find;
    native_fword* cur_entry;
    unsigned int cur_idx = end_of_dict;

    word_to_find = (char*)pop_d();

    do {
        cur_entry = &native_dictionary[cur_idx];

        if (!strncmp (word_to_find, cur_entry->name, 7)) {
            break;
        }

        cur_idx = cur_entry->next;

    } while (cur_idx != 0);

    if (cur_idx == 0) {
            printf("Word not found\n");
            push_d((intptr_t)word_to_find);
            push_d(0);
    } else {
            printf("Match found \"%s\" == \"%s\" %p \n", word_to_find, cur_entry->name, cur_entry->fn);
            push_d((intptr_t)cur_entry->fn);
            push_d(1);
    }

    print_fn(atom_find_word);


    return atom_next();
}


fword exec_springboard[] = {
    atom_exit,
    atom_exit
};

void* atom_execute (void)
{
    char numstr[20];
    fword word_to_execute = (fword)pop_d();


    sprintf(numstr, "%p", word_to_execute);

    print_fn_msg(atom_execute, numstr);

    // Copy to springboard and jump

    exec_springboard[0] = word_to_execute;
    push_r(i_ptr);
    i_ptr = exec_springboard;
    return atom_next();
}

void* atom_bye (void)
{
    print_fn(atom_execute);
    printf("bye!\n");
    exit(0);
}

void* atom_number (void)
{
    char* tok = (char*)pop_d();
    int num;
    int retval;


    // Attempt to interpret it as a number.
    retval = sscanf(tok, "%d", &num);
    printf ("sscanf retval: %d\n", retval);
    if (retval > 0) {
        push_d(num);    // Push the number
        push_d(1);      // Success
    } else {
        push_d(0);      // Failed to read the number
    }

    print_fn(atom_number);
    return atom_next();
};

// Example:
#define CREATE_PLACEHOLDER(fn)      \
void* fn (void)                     \
{                                   \
    print_fn(fn);                   \
    return atom_next();             \
}                                   \
                                    \




// Get interpretation working on native words only.
// Get bye working, so repeated typing is possible.
// Get numbers interpreted correctly.
// Add colon native word to create header in user dictionary
// Get simple colon definition of 4 2 + working.

#include "pgm_data.h"


int main (void)
{
    // Init machine
    memset(return_stack, 0, sizeof(return_stack));
    tors = BASE_OF_STACK;
    tods = BASE_OF_STACK;
    input_buffer[0] = '\0';

    // Set to the start of the program
    i_ptr = repl;

    // Returns the next word to run
    fword next_word = atom_next();

    // Run until done.
    while (next_word != NULL) {

        next_word = next_word();

        check_stack_range();
    }

    return 0;
}




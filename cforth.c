// C implementation of a simple forth machine structure

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdalign.h>

// Need this to determine which are atomic vs, non-atomic functions
#pragma GCC optimize ("align-functions=16")

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


void* atom_bye (void);
void* atom_dup (void);
void* atom_swap (void);
void* atom_drop (void);
void* atom_not (void);
void* atom_plus (void);
void* atom_exit (void);
void* atom_literal (void);
void* atom_def (void);
void* atom_semicolon (void);
void* atom_immediate (void);
void* atom_jmp0 (void);
void* atom_jmp (void);
void* atom_if (void);
void* atom_else (void);
void* atom_then (void);
void* atom_begin (void);
void* atom_until (void);
void* atom_1compile1 (void);
void* atom_postpone (void);


void* next (void);
void* atom_input (void);
void* atom_lex (void);
void* execute (void);
void* atom_number (void);
char* find_word (char* word_to_find, bool* is_user_word);


#define CREATE_PLACEHOLDER(fn)      \
void* fn (void)                     \
{                                   \
    print_fn(fn);                   \
    return next();             \
}                                   \
                                    \

CREATE_PLACEHOLDER(atom_def);
CREATE_PLACEHOLDER(atom_semicolon);
CREATE_PLACEHOLDER(atom_immediate);
CREATE_PLACEHOLDER(atom_jmp0);
CREATE_PLACEHOLDER(atom_jmp);
CREATE_PLACEHOLDER(atom_then);
CREATE_PLACEHOLDER(atom_until);
CREATE_PLACEHOLDER(atom_1compile1);
CREATE_PLACEHOLDER(atom_postpone);



bool check_stack_underflows(void);


typedef struct {
//    uint32_t link;
    void* link;
    char name[8];
} fword_hdr;


typedef struct {
    void* link;
    char name[8];
    fword fn;
} native_fword;

native_fword native_dictionary[1000] = {
    {NULL, "bye", atom_bye}, 
    {&native_dictionary[0],     "dup",      atom_dup},
    {&native_dictionary[1],     "swap",     atom_swap},
    {&native_dictionary[2],     "drop",     atom_drop},
    {&native_dictionary[3],     "not",      atom_not},
    {&native_dictionary[4],     "+",        atom_plus},
    {&native_dictionary[5],     "exit",     atom_exit},
    {&native_dictionary[6],     "literal",  atom_literal},
    {&native_dictionary[7],     "def",      atom_def},
    {&native_dictionary[8],     ";",        atom_semicolon},
    {&native_dictionary[9],     "immedia",  atom_immediate},
    {&native_dictionary[10],    "jmp0",     atom_jmp0},
    {&native_dictionary[11],    "jmp",      atom_jmp},
    {&native_dictionary[12],    "if",       atom_if},
    {&native_dictionary[13],    "else",     atom_else},
    {&native_dictionary[14],    "then",     atom_then},
    {&native_dictionary[15],    "begin",    atom_begin},
    {&native_dictionary[16],    "until",    atom_until},
    {&native_dictionary[17],    "[compil",  atom_1compile1},
    {&native_dictionary[18],    "postpon",  atom_postpone},
};

#define LAST_ENTRY_IDX 19

uint8_t* dictionary = (uint8_t*)native_dictionary;

// unsigned int top_of_dict = (LAST_ENTRY_IDX - 1)*sizeof(native_fword);  // Points to current valid word.
uint8_t* entry;
uint8_t* here;


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



void* next (void)
{
    uintptr_t tmp = (uintptr_t)*i_ptr;
    i_ptr++;

    if (tmp & 0x01) {
        // Clear the flag before calling it.
        tmp--;

        push_r(i_ptr);
        i_ptr = (fword*)tmp;

        return next();
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

    return next();

}

void* atom_exit (void)
{
    print_fn(atom_exit);

    i_ptr = pop_r();

    if (i_ptr != NULL) {
        return next();
    } else {
        return NULL;
    }
}


void* atom_begin (void)
{
    print_fn(atom_begin);
    push_r(i_ptr);

    return next();
}


void* atom_swap (void)
{
    uintptr_t tmp = data_stack[tods];

    data_stack[tods] = data_stack[tods - 1];
    data_stack[tods-1]     = tmp;

    print_fn(atom_swap);
    return next();
}

void* atom_dup (void)
{
    push_d (data_stack[tods]);

    print_fn(atom_dup);
    return next();
}

void* atom_not (void)
{
    data_stack[tods] = !(data_stack[tods]);

    print_fn(atom_not);
    return next();
}

void* atom_drop ()
{
    pop_d ();

    print_fn(atom_drop);
    return next();
}

void* atom_plus (void)
{
    int a, b;

    a = (int)pop_d();
    b = (int)pop_d();
    a += b;
    push_d((intptr_t)a);

    print_fn(atom_plus);
    return next();
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

    return next();
}

void* atom_else (void)
{
    char numstr[20];

    int jmp = (intptr_t)*i_ptr;

    i_ptr += jmp;


    sprintf(numstr, "(%d)", jmp);
    print_fn_msg(atom_else, numstr);

    return next();
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
    return next();
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
    return next();
}

char* find_word (char* word_to_find, bool* is_user_word)
{
    uint8_t* cur;
    bool found = false;
    uint32_t link;
    uint8_t* name;
    uint8_t* body;

    cur = entry;

    int i = 5;

    while (cur != NULL) {
        link = *(uint32_t*)cur;
        name = cur + 4;
        body = cur + 12;

        printf ("link = %08x, name = %8s, body = %p\n", link, name, body);

//        if (--i == 0)
//            break;

        // Is entry active?
        if ((link & 0x02) == 0) {

            if (!strncmp (word_to_find, name, 7)) {
                found = true;
                break;
            }
        }

        cur = (uint8_t*)(link & ~0x7);
    }

    if (found) {
        *is_user_word = (link & 0x01);
        return body;
    }

    return NULL;
}


fword exec_springboard[] = {
    atom_exit,  // Replaced by word to execute.
    atom_exit
};

void* execute (void)
{
    char numstr[20];
    fword word_to_execute = (fword)pop_d();


    sprintf(numstr, "%p", word_to_execute);

    print_fn_msg(execute, numstr);

    // Copy to springboard and jump

    exec_springboard[0] = word_to_execute;
    push_r(i_ptr);
    i_ptr = exec_springboard;
    return next();
}

void* atom_bye (void)
{
    print_fn(atom_bye);
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
    return next();
};

void create_user_entries (void)
{
    uint32_t val;
    uint8_t* push4_addr;

    printf ("dictionary: %p, here: %p offset: %d diff: %d\n",
                dictionary, here, 
                LAST_ENTRY_IDX * sizeof(native_fword), 
                here - dictionary);

    printf ("dictionary entry: %p\n", entry);
    
    // Add push4 to dictionary
    val = (uint32_t)entry | 0x01;
    entry = here;
    memcpy(here, &val, 4);      here += 4;
    strcpy(here, "push4");      here += 8;
    push4_addr = here;          // Save this for later.
    val = (uint32_t) atom_literal;
    memcpy(here, &val, 4);      here += 4;
    val = 4;
    memcpy(here, &val, 4);      here += 4;
    val = (uint32_t) atom_exit;
    memcpy(here, &val, 4);      here += 4;

    // Add push8 to dictionary
    val = (uint32_t)entry | 0x01;
    entry = here;
    memcpy(here, &val, 4);      here += 4;
    strcpy(here, "push8");      here += 8;
    val = (uint32_t) push4_addr | 0x01;
    memcpy(here, &val, 4);      here += 4;
    memcpy(here, &val, 4);      here += 4;
    val = (uint32_t) atom_plus;
    memcpy(here, &val, 4);      here += 4;
    val = (uint32_t) atom_exit;
    memcpy(here, &val, 4);      here += 4;


    printf ("dictionary: %p, here: %p offset: %d diff: %d\n",
                dictionary, here, 
                LAST_ENTRY_IDX * sizeof(native_fword), 
                here - dictionary);

    printf ("dictionary entry: %p\n", entry);
    
    fflush(stdout);

}

int read_input_line (void)
{
    return 0;
}

char* lex(void)
{
    return NULL;
}

void execute_word(uint8_t* body, bool is_user_word)
{
}

bool parse_number(char* tok, int32_t* val)
{
    return false;
}


void repl (void)
{

    while (1) {
        bool error = false;

        int num_read = read_input_line();
        if (num_read == 0)
            continue;

        while (1) {
            bool is_user_word;
            int32_t val;
            char* tok = lex();

            if (!tok)
                break;

            uint8_t* body_ptr = find_word(tok, &is_user_word);
            if (body_ptr != NULL) {
                execute_word(body_ptr, is_user_word);
            } else if (parse_number(tok, &val)) {
                push_d(val);
            } else {
                printf ("%s?\n");
                error = true;
                break;
            }
        }

        if (!error) {
            printf ("  ok\n");
        }
    }
}



#if 0

//        read input from user until newline
//        while not end of line:
//            lex next token
//            if token found:
//                search for token in dictionary
//                if found
//                    execute the word
//                else
//                    attempt to parse the word as a number
//                    if it can be parsed as a number
//                        place number on data stack
//                    else
//                        print “Error: word not found”
//                        break
//            else:
//                print “  ok\n>”
    }






    // Set to the start of the program
    i_ptr = (fword*)(entry + 12);

    bool is_user_word = false;
    char* push8ptr = find_word ("bye", &is_user_word);

//    printf ("push8ptr = %p, i_ptr = %p, is_user_word = %d\n", push8ptr, i_ptr, is_user_word);

    i_ptr = (fword*)push8ptr;
    


    push_r(NULL);

    // Returns the next word to run
    fword next_word = next();

    // Run until done.
    while (next_word != NULL) {

        next_word = next_word();
        check_stack_range();    // Check every time, but could be done only in certain points.
    }

    print_fn(repl);
}
#endif


int main (void)
{
    // Init machine
    memset(return_stack, 0, sizeof(return_stack));
    tors = BASE_OF_STACK;
    tods = BASE_OF_STACK;
    input_buffer[0] = '\0';
    here = dictionary + LAST_ENTRY_IDX * sizeof(native_fword);
    entry = here - sizeof(native_fword);


    create_user_entries();

    repl();


    return 0;
}




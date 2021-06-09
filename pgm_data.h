#ifndef _PGM_DATA_H_
#define _PGM_DATA_H_

// : fib  ( n -- )  1 1 rot 0 do dup rot dup . + loop drop drop ; 
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
    atom_dot,
    atom_plus,
    atom_loop,
    atom_drop,
    atom_drop,
    atom_exit
};
 

// : pgm_1 10 fib ;
fword pgm_1[] = {
    atom_literal,
    FORTH_LIT(10),
    FORTH_WORD(fib),
    atom_exit
};

fword parse_word[] = {
    atom_find_word,
    atom_if,
    FORTH_LIT(4),
    atom_execute,
    atom_else,
    FORTH_LIT(2),
    atom_number,
    // atom_then,
    atom_exit
};


fword interpret[] = {
    atom_begin,
    atom_lex,
    atom_dup,
    atom_if,
    FORTH_LIT(3),
    atom_drop,
    FORTH_WORD(parse_word),
    // atom_then,
    atom_while,
    atom_exit
};


fword pgm_2[] = {
    atom_literal,
    FORTH_LIT(0),
    atom_dup,
    atom_begin,
    atom_input,
    FORTH_WORD(interpret),
    atom_not,
    atom_while,
    atom_exit
};



fword if_test[] = {
    atom_if,
    FORTH_LIT(5),
    atom_literal,
    FORTH_LIT(1234),
    atom_else,
    FORTH_LIT(3),
    atom_literal,
    FORTH_LIT(5678),
    // atom_next,
    atom_exit
};

// If/else testing
fword pgm_3[] = {
    atom_literal,
    FORTH_LIT(1),
    FORTH_WORD(if_test),
    atom_literal,
    FORTH_LIT(0),
    FORTH_WORD(if_test),
    atom_exit
};



// Loop test
fword pgm_4[] = {
    atom_literal,
    FORTH_LIT(5),
    atom_literal,
    FORTH_LIT(4),
    atom_begin,
    atom_swap,
    atom_not,
    atom_dup,
    atom_dot,
    atom_swap,
    atom_literal,
    FORTH_LIT(1),
    atom_subtract,
    atom_dup,
    atom_while,
    atom_drop,
    atom_drop,
    atom_exit
};

fword test_input[] = {
    atom_input,
    atom_exit
};



#endif /* #ifndef _PGM_DATA_H_ */
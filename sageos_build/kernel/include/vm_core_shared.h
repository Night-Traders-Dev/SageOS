#ifndef SAGE_VM_CORE_SHARED_H
#define SAGE_VM_CORE_SHARED_H

// ============================================================================
// Shared VM Core Definition
// ============================================================================
// Defines shared opcodes and instructions used by both bare-metal and
// standard SageLang VM implementations.
// Updated for SageLang 3.6.3 (SGVM 2.0)

typedef enum {
    OP_CONSTANT       = 0,
    OP_NIL            = 1,
    OP_TRUE           = 2,
    OP_FALSE          = 3,
    OP_POP            = 4,
    OP_GET_GLOBAL     = 5,
    OP_DEFINE_GLOBAL  = 6,
    OP_SET_GLOBAL     = 7,
    OP_DEFINE_FN      = 8,
    OP_GET_PROPERTY   = 9,
    OP_SET_PROPERTY   = 10,
    OP_GET_INDEX      = 11,
    OP_SET_INDEX      = 12,
    OP_LOAD_FUNCTION  = 13,
    OP_SLICE          = 14,
    OP_ADD            = 15,
    OP_SUB            = 16,
    OP_MUL            = 17,
    OP_DIV            = 18,
    OP_MOD            = 19,
    OP_NEGATE         = 20,
    OP_EQUAL          = 21,
    OP_NOT_EQUAL      = 22,
    OP_GREATER        = 23,
    OP_GREATER_EQ     = 24,
    OP_LESS           = 25,
    OP_LESS_EQ        = 26,
    OP_BIT_AND        = 27,
    OP_BIT_OR         = 28,
    OP_BIT_XOR        = 29,
    OP_BIT_NOT        = 30,
    OP_SHIFT_LEFT     = 31,
    OP_SHIFT_RIGHT    = 32,
    OP_NOT            = 33,
    OP_TRUTHY         = 34,
    OP_JUMP           = 35,
    OP_JUMP_IF_FALSE  = 36,
    OP_CALL           = 37,
    OP_CALL_METHOD    = 38,
    OP_ARRAY          = 39,
    OP_TUPLE          = 40,
    OP_DICT           = 41,
    OP_PRINT          = 42,
    OP_EXEC_AST_STMT  = 43,
    OP_RETURN         = 44,
    OP_PUSH_ENV       = 45,
    OP_POP_ENV        = 46,
    OP_DUP            = 47,
    OP_ARRAY_LEN      = 48,
    OP_BREAK          = 49,
    OP_CONTINUE       = 50,
    OP_LOOP_BACK      = 51,
    OP_IMPORT         = 52,
    OP_CLASS          = 53,
    OP_METHOD         = 54,
    OP_INHERIT        = 55,
    OP_SETUP_TRY      = 56,
    OP_END_TRY        = 57,
    OP_RAISE          = 58,
    OP_HALT           = 0xFF
} OpCode;

#endif // SAGE_VM_CORE_SHARED_H

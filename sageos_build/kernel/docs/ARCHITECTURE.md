# SGVM Sage Documentation

## Opcodes

The following opcodes are supported by `sgvm.sage` and emitted by `sgvmc.sage`:

| Opcode | Value | Description |
|--------|-------|-------------|
| OP_CONSTANT | 0 | Push a constant onto the stack |
| OP_NIL | 1 | Push nil onto the stack |
| OP_TRUE | 2 | Push true onto the stack |
| OP_FALSE | 3 | Push false onto the stack |
| OP_POP | 4 | Pop the top value from the stack |
| OP_GET_GLOBAL | 5 | Get a global variable value |
| OP_DEFINE_GLOBAL | 6 | Define a global variable |
| OP_SET_GLOBAL | 7 | Assign a value to a global variable |
| OP_DEFINE_FUNCTION | 8 | Define a function |
| OP_GET_PROPERTY | 9 | Get an object property |
| OP_SET_PROPERTY | 10 | Set an object property |
| OP_GET_INDEX | 11 | Get an element at an index (array/dict) |
| OP_SET_INDEX | 12 | Set an element at an index |
| OP_LOAD_FUNCTION | 13 | Load a function onto the stack |
| OP_SLICE | 14 | Perform an array or string slice |
| OP_ADD | 15 | Addition |
| OP_SUB | 16 | Subtraction |
| OP_MUL | 17 | Multiplication |
| OP_DIV | 18 | Division |
| OP_MOD | 19 | Modulo |
| OP_NEGATE | 20 | Unary negation |
| OP_EQUAL | 21 | Equality check |
| OP_NOT_EQUAL | 22 | Inequality check |
| OP_GREATER | 23 | Greater than |
| OP_GREATER_EQUAL | 24 | Greater than or equal |
| OP_LESS | 25 | Less than |
| OP_LESS_EQUAL | 26 | Less than or equal |
| OP_BIT_AND | 27 | Bitwise AND |
| OP_BIT_OR | 28 | Bitwise OR |
| OP_BIT_XOR | 29 | Bitwise XOR |
| OP_BIT_NOT | 30 | Bitwise NOT |
| OP_SHIFT_LEFT | 31 | Bitwise left shift |
| OP_SHIFT_RIGHT | 32 | Bitwise right shift |
| OP_NOT | 33 | Logical NOT |
| OP_TRUTHY | 34 | Truthiness check |
| OP_JUMP | 35 | Unconditional jump |
| OP_JUMP_IF_FALSE | 36 | Jump if the top value is false |
| OP_CALL | 37 | Call a function |
| OP_CALL_METHOD | 38 | Call an object method |
| OP_ARRAY | 39 | Create an array |
| OP_TUPLE | 40 | Create a tuple |
| OP_DICT | 41 | Create a dictionary |
| OP_PRINT | 42 | Print a value |
| OP_EXEC_AST_STMT | 43 | Execute an AST statement (fallback) |
| OP_RETURN | 44 | Return from a function |
| OP_PUSH_ENV | 45 | Push a new environment scope |
| OP_POP_ENV | 46 | Pop an environment scope |
| OP_DUP | 47 | Duplicate a value on the stack |
| OP_ARRAY_LEN | 48 | Get array length |
| OP_BREAK | 49 | Loop break |
| OP_CONTINUE | 50 | Loop continue |
| OP_LOOP_BACK | 51 | Jump to the start of a loop |
| OP_IMPORT | 52 | Import and execute an external module |
| OP_CLASS | 53 | Define a class |
| OP_METHOD | 54 | Define a method on a class |
| OP_INHERIT | 55 | Set up class inheritance |
| OP_SETUP_TRY | 56 | Push an exception handler |
| OP_END_TRY | 57 | Pop the current exception handler |
| OP_RAISE | 58 | Raise an exception |
| OP_HALT | 0xFF | Halt execution |

## Native Bridge

SGVM provides a high-performance native bridge to the host SageLang environment. This allows guest bytecode to call standard library functions directly without the overhead of guest-side implementations.

The following modules are currently bridged:
- **math**: `sqrt`, `sin`, `cos`, `tan`, `floor`, `ceil`, `abs`, `pow`.
- **io**: `read` (maps to `readfile`), `write` (maps to `writefile`).
- **sys**: `args()`, `getenv()`, `clock()`, `exit()`.
- **re**: `search()`, `match()` (full match), `test()`.
- **thread**: `spawn()`, `join()`, `mutex()`, `lock()`, `unlock()`, `sleep()`.
- **ffi**: `open()`, `call()`, `close()`.
- **mem**: `alloc()`, `free()`, `read()`, `write()`, `size()`, `usage()`, `limit()`.
- **struct**: `def()`, `new()`, `get()`, `set()`, `size()`.
- **gc**: `collect()`, `stats()`, `enable()`, `disable()`.

Native bridging is implemented by tagging objects and function-like dictionaries with a `__native__` property. The `OP_CALL` and `OP_CALL_METHOD` opcodes detect these tags and dispatch execution to the VM's `call_native()` handler.

## Multi-threading & GIL

SGVM supports true multi-threading by leveraging the host SageLang `thread` module. Each guest thread runs in its own `MetalVM` instance, sharing the same constant pool and global environment.

To maintain consistency within the SageLang-based interpreter, a **Global Interpreter Lock (GIL)** is used to serialize guest bytecode execution. The GIL is automatically released during blocking operations such as `thread.sleep()`, `thread.join()`, and `thread.lock()` to allow for maximum concurrency.

## Sandboxing & Isolation

For high-isolation environments, `MetalVM` provides several security features:
- **safe_mode**: When enabled, access to sensitive modules (`ffi`, `mem`, `struct`, `io`) is restricted.
- **ffi_enabled**: A dedicated flag to toggle FFI support independently of other security settings.
- **Resource Limits**: The `mem.limit()` function allows the host to cap the amount of raw memory the guest can allocate.

## Function Arguments

Arguments to SGVM functions are passed positionally and bound to the function's local scope using the naming convention `__argN`, where `N` is the zero-based index of the argument. For example, the first argument is accessible as `__arg0`, the second as `__arg1`, and so on.

## Binary Format & I/O

SGVM uses a dedicated **BYTES** type for high-performance binary I/O, ensuring that bytecode and constants are parsed correctly across backends. The native `io.readbytes` bridge returns this type instead of a standard string, allowing for integer-based byte indexing.

The SGVM binary format consists of:
0. Optional Shebang: `#!/usr/bin/env sgvm\n` (added by `sgvmc --shebang`)
1. Header: "SGVM" (4 bytes)
2. Version: 0x01 0x00 (2 bytes)
3. Constant Pool:
   - Count (2 bytes, big-endian)
   - Constants (Type, then value)
4. Chunk Count (4 bytes, big-endian)
5. Chunks:
   - Length (4 bytes, big-endian)
   - Code (Byte array)

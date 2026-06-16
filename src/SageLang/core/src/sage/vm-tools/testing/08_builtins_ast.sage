# Test: Builtins & AST Delegation
print("Testing print builtin")

import math
print("math.pi: " + str(math.pi))

# EXEC_AST_STMT (sys.exec)
import sys
sys.exec("print \"Hello from AST delegation\"")

# OP_MATH_PRINTM (requires array of arrays)
var m = [[1, 0], [0, 1]]
# math.printm(m)

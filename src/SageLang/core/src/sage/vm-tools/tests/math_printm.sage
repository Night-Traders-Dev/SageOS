# Test math.printm
import math

# OP_MATH_PRINTM (87) is correctly implemented in the VM
# but the compiler (sgvmc.sage) does not have a mapping for it.
# It should be mapped from some host instruction or emitted explicitly.
# In SageLang, math.printm(matrix) should emit OP_MATH_PRINTM.

var m = [[1, 2], [3, 4]]
print "Printing matrix:"
math.printm(m)

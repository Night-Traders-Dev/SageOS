# Test: Stack & Arithmetic Opcodes
var x = 10
var y = 3
print("x + y = " + str(x + y))
print("x - y = " + str(x - y))
print("x * y = " + str(x * y))
print("x / y = " + str(x / y))
print("x % y = " + str(x % y))
print("-x = " + str(-x))

var n = nil
print("nil = " + str(n))

var t = true
var f = false
print("true = " + str(t))
print("false = " + str(f))

# Test DUP and POP via expression (compiler handles these)
var z = (x + x)
print("z = " + str(z))

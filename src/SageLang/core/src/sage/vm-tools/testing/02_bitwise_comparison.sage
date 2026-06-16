# Test: Bitwise & Comparison Opcodes
var a = 10
var b = 20

print("a == b: " + str(a == b))
print("a != b: " + str(a != b))
print("a > b: " + str(a > b))
print("a >= b: " + str(a >= b))
print("a < b: " + str(a < b))
print("a <= b: " + str(a <= b))

var t = true
print("not t: " + str(not t))

var res = ""
if a > 0:
    res = "true"
else:
    res = "false"
print("truthy a: " + res)

var x = 5
var y = 3
print("t and f: " + str(t and false))
print("t or f: " + str(t or false))

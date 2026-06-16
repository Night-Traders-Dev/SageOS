# Test: Control Flow
var i = 0
while i < 3:
    print("loop: " + str(i))
    i = i + 1

proc say_hello(name):
    return "Hello, " + name

print(say_hello("Sage"))

if i == 3:
    print("if: true")
else:
    print("if: false")

# Note: BREAK and CONTINUE are emitted for loops
var j = 0
while j < 10:
    if j == 2:
        j = j + 1
        continue
    if j == 4:
        break
    print("j: " + str(j))
    j = j + 1

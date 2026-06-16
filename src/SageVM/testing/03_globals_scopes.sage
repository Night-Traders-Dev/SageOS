# Test: Globals & Scopes
var g = "global"

proc test_scope():
    var l = "local"
    print("inside: " + g + " " + l)
    g = "updated global"

print("before: " + g)
test_scope()
print("after: " + g)

# Test environment nesting (PUSH_ENV/POP_ENV)
if true:
    var nested = "nested"
    print("nested: " + nested)

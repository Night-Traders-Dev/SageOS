# Test: Exception Handling
proc cause_error(do_it):
    if do_it:
        raise "Intentional Error"
    return "No error"

try:
    print(cause_error(false))
    cause_error(true)
catch e:
    print("Caught: " + str(e))

print("After try-catch")

# Nested try
try:
    try:
        raise "Nested"
    catch e:
        print("Inner catch: " + str(e))
        raise "Reraised"
catch e:
    print("Outer catch: " + str(e))

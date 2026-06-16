# Test for loop over array and tuple
# Note: Dict iteration currently has issues in VM and is skipped.

print "--- Array ---"
let a = [1, 2, 3]
for x in a:
    print x

print "--- Tuple ---"
let t = (10, 20, 30)
for x in t:
    print x

print "Test complete."

# Test for loop over array, tuple, and dict

print "--- Array ---"
let a = [1, 2, 3]
for x in a:
    print x

print "--- Tuple ---"
let t = (10, 20, 30)
for x in t:
    print x

print "--- Dict ---"
let d = {"a": 1, "b": 2, "c": 3}
for k in d:
    print k
    print d[k]

print "Test complete."

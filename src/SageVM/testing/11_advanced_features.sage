# Test: Advanced Features (Dynamic Loading, GC, Reflection)
import gc
import reflect
import math
import sys

print "--- GC Test ---"
let stats = gc.stats()
print "DEBUG: stats=" + str(stats)
print "objects before: " + str(stats["num_objects"])
gc.collect()
print "gc run complete"

print "--- Reflection Test ---"
class TestReflect:
    proc hello(self):
        pass
    proc bye(self):
        pass

let tr = TestReflect()
let methods = reflect.get_methods(tr)
print "methods: " + str(len(methods))
# Check for hello and bye
var has_hello = false
var has_bye = false
for m in methods:
    if m == "hello": has_hello = true
    if m == "bye": has_bye = true

if has_hello: print "found hello"
if has_bye: print "found bye"

print "--- Math Expansion Test ---"
print "tan(0): " + str(math.tan(0))
print "pow(2, 3): " + str(math.pow(2, 3))
print "round(3.7): " + str(math.round(3.7))

print "--- Sys Call Test ---"
# Test calling a host function via sys.call explicitly
print "sys.call(math.sin, 0): " + str(sys.call(math.sin, 0))

print "--- Dynamic Loading Test ---"
# We'll try to import a module that we'll compile on the fly in the runner
try:
    import dynamic_mod
    print "dynamic_mod imported"
    print "dynamic_mod result: " + str(dynamic_mod.get_val())
catch e:
    print "Dynamic loading failed: " + str(e)

print "Test complete."

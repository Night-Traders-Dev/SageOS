# Test: Data Structures
var arr = [1, 2, 3]
print("arr len: " + str(len(arr)))
print("arr[1]: " + str(arr[1]))
arr[1] = 20
print("arr[1] updated: " + str(arr[1]))

var d = {"a": 1, "b": 2}
print("dict len: " + str(len(d)))
print("dict[a]: " + str(d["a"]))
d["c"] = 3
print("dict[c]: " + str(d["c"]))

var t = (10, 20, 30)
print("tuple len: " + str(len(t)))
print("tuple[0]: " + str(t[0]))

var s = arr[0:2]
print("slice len: " + str(len(s)))
print("slice[0]: " + str(s[0]))

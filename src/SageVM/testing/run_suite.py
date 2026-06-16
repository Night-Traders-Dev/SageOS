import os
import subprocess
import sys

def run_suite():
    test_dir = "testing"
    test_files = [f for f in os.listdir(test_dir) if f.endswith(".sage") and not f.startswith("run_suite")]
    
    passed = 0
    failed = 0
    
    print("==================================================")
    print("           SageVM Opcode Test Suite               ")
    print("==================================================")
    
    for f in sorted(test_files):
        test_path = os.path.join(test_dir, f)
        expected_path = os.path.join(test_dir, f.replace(".sage", ".expected"))
        sgvm_path = os.path.join(test_dir, f.replace(".sage", ".sgvm"))
        
        # Compile
        res = subprocess.run(["./sgvmc", test_path, sgvm_path], capture_output=True, text=True)
        if res.returncode != 0:
            print(f"[FAIL] {f} (Compilation failed)")
            failed += 1
            continue
            
        # Run
        res = subprocess.run(["./sgvm", sgvm_path], capture_output=True, text=True)
        actual_output = res.stdout
        
        if not os.path.exists(expected_path):
            print(f"[SKIP] {f} (No .expected file)")
            if os.path.exists(sgvm_path): os.remove(sgvm_path)
            continue
            
        with open(expected_path, "r") as exp_file:
            expected_output = exp_file.read()
            
        if actual_output == expected_output:
            print(f"[PASS] {f}")
            passed += 1
        else:
            print(f"[FAIL] {f}")
            print("--- Expected ---")
            print(expected_output)
            print("--- Actual ---")
            print(actual_output)
            failed += 1
            
        if os.path.exists(sgvm_path): os.remove(sgvm_path)
        
    print("==================================================")
    print(f"Summary: {passed} passed, {failed} failed")
    print("==================================================")
    return failed == 0

if __name__ == "__main__":
    if not run_suite():
        sys.exit(1)

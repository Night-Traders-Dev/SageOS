import subprocess
import os
import sys
import time

def run_test(sage_file):
    print(f"Running test: {sage_file}")
    
    # 1. Prepare environment
    if not os.path.exists("build"):
        os.makedirs("build")
    
    # 2. Build the test kernel
    # We temporary swap kernel/main.sage with the test file
    orig_kernel = "kernel/main.sage"
    temp_kernel = "kernel/main.sage.bak"
    os.rename(orig_kernel, temp_kernel)
    try:
        with open(sage_file, "r") as f:
            content = f.read()
        with open(orig_kernel, "w") as f:
            f.write(content)
        
        # Build
        subprocess.run(["make", "all"], check=True)
        
        # Run QEMU
        cmd = ["qemu-system-riscv64", "-M", "virt", "-cpu", "rv64", "-m", "128M", "-display", "none", "-serial", "stdio", "-kernel", "build/sageos_rv64.elf"]
        process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        
        # Capture output for 5 seconds
        output = ""
        start_time = time.time()
        while time.time() - start_time < 5:
            line = process.stdout.readline()
            if line:
                output += line
                print(f"  {line.strip()}")
            if "SUCCESS" in output or "FAILURE" in output:
                break
        
        process.kill()
        
        if "SUCCESS" in output:
            print(f"TEST PASSED: {sage_file}")
            return True
        else:
            print(f"TEST FAILED: {sage_file}")
            return False
            
    finally:
        if os.path.exists(temp_kernel):
            if os.path.exists(orig_kernel):
                os.remove(orig_kernel)
            os.rename(temp_kernel, orig_kernel)

if __name__ == "__main__":
    tests = [
        "tests/test-suite/kernel_boot.sage"
    ]
    
    all_passed = True
    for test in tests:
        if not run_test(test):
            all_passed = False
    
    if all_passed:
        print("\nAll tests passed!")
        sys.exit(0)
    else:
        print("\nSome tests failed.")
        sys.exit(1)

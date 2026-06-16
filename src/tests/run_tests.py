import subprocess
import os
import sys
import time

def run_test(sage_file):
    print(f"Running test: {sage_file}")
    
    # 1. Prepare environment
    # Paths relative to src/
    orig_kernel = "kernel/main.sage"
    temp_kernel = "kernel/main.sage.bak"
    
    os.rename(orig_kernel, temp_kernel)
    try:
        with open(sage_file, "r") as f:
            content = f.read()
        with open(orig_kernel, "w") as f:
            f.write(content)

        # Force rebuild by touching the kernel
        subprocess.run(["touch", "kernel/main.sage"], cwd=".", check=True)

        # Build - Run make in the src/ directory (relative to here is just '.')
        subprocess.run(["make", "all"], cwd=".", check=True)

        # Run QEMU (Path relative to src/)
        log_file = "build/qemu.log"
        if os.path.exists(log_file): os.remove(log_file)
        cmd = ["qemu-system-riscv64", "-M", "virt", "-cpu", "rv64", "-m", "128M", "-display", "none", "-serial", "file:build/qemu.log", "-kernel", "build/sageos_rv64.elf"]

        process = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, cwd=".")
        
        # Capture output from file for 15 seconds
        output = ""
        start_time = time.time()
        while time.time() - start_time < 15:
            if os.path.exists(log_file):
                with open(log_file, "r") as f:
                    output = f.read()
            if "SageOS System Ready." in output:
                break
            time.sleep(0.1)
        
        process.kill()
        
        if "SageOS System Ready." in output:
            print(f"TEST PASSED: {sage_file}")
            return True
        else:
            print(f"TEST FAILED: {sage_file}")
            print(f"Captured Output (from {log_file}):")
            print(output)
            _, err = process.communicate()
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

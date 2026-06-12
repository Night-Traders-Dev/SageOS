import sys
import io

proc run_test(filename):
    let test_path = "testsuite/" + filename
    let temp_sgvm = "testsuite/" + filename + ".temp.sgvm"
    
    # 1. Compile the test using our compiled sgvmc tool
    let compile_cmd = "./sgvmc " + test_path + " " + temp_sgvm
    let comp_status = sys.exec(compile_cmd)
    if comp_status != 0:
        print "  [FAIL] " + filename + " (Compilation failed with status " + str(comp_status) + ")"
        return false

    # 2. Run the compiled test on our sgvm interpreter
    let guest_cmd = "./sgvm " + temp_sgvm
    let guest_out = sys.shell_exec(guest_cmd)

    # 3. Run the source test on the host SageLang interpreter for baseline
    let host_cmd = ".deps/SageLang/core/sage " + test_path
    let host_out = sys.shell_exec(host_cmd)

    # 4. Clean up temporary compiled binary
    io.remove(temp_sgvm)

    # 5. Compare stdout
    if guest_out == host_out:
        print "  [PASS] " + filename
        return true
    else:
        print "  [FAIL] " + filename
        print "    Expected (Host):"
        print "----------------------------------------"
        print host_out
        print "----------------------------------------"
        print "    Actual (Guest VM):"
        print "----------------------------------------"
        print guest_out
        print "----------------------------------------"
        return false

proc main():
    print "=================================================="
    print "            SageVM Automated Test Suite            "
    print "=================================================="
    
    let files = io.listdir("testsuite")
    if files == nil:
        print "Error: Could not list testsuite directory."
        sys.exit(1)
        return

    var passed = 0
    var failed = 0
    var test_files = []
    
    # Collect test files
    var i = 0
    while i < len(files):
        let f = files[i]
        if endswith(f, ".sage") and f != "run_tests.sage":
            push(test_files, f)
        i = i + 1

    print "Found " + str(len(test_files)) + " tests to execute.\n"

    # Execute all tests
    i = 0
    while i < len(test_files):
        let f = test_files[i]
        let res = run_test(f)
        if res:
            passed = passed + 1
        else:
            failed = failed + 1
        i = i + 1

    print "\n=================================================="
    print "Test Summary:"
    print "  Passed: " + str(passed)
    print "  Failed: " + str(failed)
    print "=================================================="

    if failed > 0:
        sys.exit(1)
    else:
        sys.exit(0)

main()

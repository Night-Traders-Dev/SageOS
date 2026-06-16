# Trap Dispatcher for SageOS
# Routes hardware interrupts and system calls to SageLang handlers.

# RISC-V Exception/Interrupt Causes
let CAUSE_USER_ECALL = 8
let CAUSE_SUPERVISOR_ECALL = 9
let CAUSE_TIMER_INTERRUPT = 5

proc dispatch_trap(cause, epc):
    # cause is the scause CSR value
    # epc is the sepc CSR value
    
    # Check for ECALL (System Call)
    if cause == CAUSE_SUPERVISOR_ECALL:
        print "Trap: Supervisor ECALL at " + str(epc)
        return
    end

    # Check for Timer Interrupt
    if cause == CAUSE_TIMER_INTERRUPT:
        print "Trap: Timer interrupt"
        return
    end
    
    print "Trap: Unknown cause " + str(cause) + " at " + str(epc)
end

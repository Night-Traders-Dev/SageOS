# Trap Dispatcher for SageOS
# Routes hardware interrupts and system calls to SageLang handlers.
import drivers.memory.bare_metal as bm

# RISC-V Exception/Interrupt Causes
let CAUSE_USER_ECALL = 8
let CAUSE_SUPERVISOR_ECALL = 9
let CAUSE_TIMER_INTERRUPT = 5

proc dispatch_trap(cause, epc):
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

proc poll_traps():
    let trap = bm.get_trap()
    let cause = trap[0]
    let epc = trap[1]
    
    if cause != -1:
        dispatch_trap(cause, epc)
    end
end

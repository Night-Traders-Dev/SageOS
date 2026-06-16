import kernel.interrupts as trap_mgr

proc kmain():
    print "SageOS Booting..."
    print "Kernel started."
    
    while true:
        trap_mgr.poll_traps()
    end
end

kmain()

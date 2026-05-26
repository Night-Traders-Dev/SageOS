# dmesg.sage - Pure Sage kernel log viewer

proc main():
    let logs = os_get_dmesg()
    if logs == nil:
        os_write_str("\nError: Could not fetch kernel logs")
        return
    
    os_write_str("\n--- Kernel Logs ---\n")
    os_write_str(logs)
    os_write_str("\n--- End of Logs ---\n")

main()

import sys
import os.linux.namespace as ns

let args = sys.args()

if len(args) < 3:
    print("Usage: ./sage scripts/run_chroot.sage <arch>")
    exit(1)
end

let arch = args[2]
let rootfs = "SageRoot/" + arch

# Check if we can use a built-in for existence or os.system test
# Since 'import os' failed before, we rely on what works.
# Let's try to keep it simple.

print("Launching SageOS container for " + arch + "...")
# We'll assume the path is fine or let unshare handle it.
let absolute_rootfs = rootfs 

let config = ns.minimal_container("sage-os-" + arch, absolute_rootfs)

# The command inside the container will be the Sage interpreter running the shell
# Since we copied 'sage' to /bin/sage in the rootfs
let inner_cmd = "/bin/sage /etc/sagelang/shell.sage"

let unshare_cmd = ns.ns_emit_unshare_cmd(config)
let full_cmd = unshare_cmd + " " + inner_cmd

print("Executing: " + full_cmd)
# Note: This requires root/sudo privileges for namespace operations (CLONE_NEWNS, etc.)
sys.exec(full_cmd)

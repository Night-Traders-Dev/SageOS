import sys
import os.linux.namespace as ns

let args = sys.args()

if len(args) < 3:
    print("Usage: ./sage scripts/run_chroot.sage <arch>")
    exit(1)
end

let arch = args[2]
let rootfs = "SageRoot/" + arch

print("Launching SageOS container for " + arch + "...")
let absolute_rootfs = rootfs 

let config = ns.minimal_container("sage-os-" + arch, absolute_rootfs)

# Explicitly set SAGE_PATH inside the container command since 'sudo -E' might fail
let sage_path = "SAGE_PATH=/lib/sagelang"
let inner_cmd = sage_path + " /bin/sage /etc/sagelang/shell.sage"

let unshare_cmd = ns.ns_emit_unshare_cmd(config)
let full_cmd = unshare_cmd + " " + inner_cmd

print("Executing: " + full_cmd)
# Note: This requires root/sudo privileges for namespace operations (CLONE_NEWNS, etc.)
sys.exec(full_cmd)

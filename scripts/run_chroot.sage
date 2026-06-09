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

# Populate stubs into the rootfs so the shell can use them
print("Injecting FFI stubs into container...")
let stubs_path = absolute_rootfs + "/etc/sagelang/chroot_stubs.sage"
# We'll use sys.exec to copy it
sys.exec("cp scripts/chroot_stubs.sage " + stubs_path)

# The command inside the container will be the Sage interpreter running the shell
# We prepend the stubs so functions like os_read_key are defined
let inner_cmd = "env SAGE_PATH=/lib/sagelang /bin/sage /etc/sagelang/chroot_stubs.sage /etc/sagelang/shell.sage"

let unshare_cmd = ns.ns_emit_unshare_cmd(config)
let full_cmd = unshare_cmd + " " + inner_cmd

print("Executing: " + full_cmd)
# Note: This requires root/sudo privileges for namespace operations (CLONE_NEWNS, etc.)
sys.exec(full_cmd)

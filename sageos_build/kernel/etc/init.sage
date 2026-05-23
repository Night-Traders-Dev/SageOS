# SageOS Init System
# Written in SageLang

# Set color to green (0x79FFB0)
os_set_color_hex(7995312)

os_write_str("  ____    _    ____ _____ ___  ____  \n")
os_write_str(" / ___|  / \\  / ___| ____/ _ \\/ ___| \n")
os_write_str(" \\___ \\ / _ \\| |  _|  _|| | | \\___ \\ \n")
os_write_str("  ___) / ___ \\ |_| | |__| |_| |___) |\n")
os_write_str(" |____/_/   \\_\\____|_____\\___/|____/ \n")

# Reset color
os_set_color_hex(15263976)

os_write_str("\n--- SageOS Init System (SageInit) ---\n")
os_write_str("Initializing system services...\n")

# 1. Display MOTD
if os_path_exists("/etc/motd"):
    os_cat("/etc/motd")

os_write_str("\nStorage Status:\n")

# 2. Check FAT32
if os_path_exists("/fat32"):
    os_write_str("  [OK] /fat32 mounted (EFI System Partition)\n")
else:
    os_write_str("  [--] /fat32 not mounted\n")

# 3. Check BTRFS
if os_path_exists("/btrfs"):
    os_write_str("  [OK] /btrfs mounted (Root Filesystem)\n")
else:
    os_write_str("  [--] /btrfs not mounted\n")

# 4. Check Swap (via a new native binding or just checking device)
os_write_str("  [..] checking swap...\n")
os_shell_exec("swapinfo")

os_write_str("\nSystem ready.\n\n")

# The kernel will launch the shell after this script finishes.

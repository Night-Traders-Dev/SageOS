import os
import sys

def generate_header(etc_dir, output_header):
    all_files = []
    
    # Recursively scan etc_dir
    if os.path.exists(etc_dir):
        for root, dirs, files in os.walk(etc_dir):
            for f in files:
                if (f.endswith('.sage') or f.endswith('.json') or f.endswith('.sgvm')):
                    abs_path = os.path.join(root, f)
                    rel_path = os.path.relpath(abs_path, etc_dir)
                    target_path = f"/etc/{rel_path}"
                    all_files.append((f, root, target_path))

    # /bin files (from the workspace root's bin directory if it exists)
    # Note: We'll point the script to the kernel/etc dir, so we look for ../bin relative to it
    bin_dir = os.path.join(os.path.dirname(etc_dir), "bin")
    if os.path.exists(bin_dir):
        for f in os.listdir(bin_dir):
            if os.path.isfile(os.path.join(bin_dir, f)):
                all_files.append((f, bin_dir, f"/bin/{f}"))
    
    all_files.sort()

    with open(output_header, 'w') as f:
        f.write("/* Auto-generated command embeddings */\n#pragma once\n\n")
        
        # Write byte arrays
        for filename, src_dir, target_path in all_files:
            # Clean name for C variable
            clean_name = target_path.replace("/", "_").replace(".", "_").replace("-", "_")
            var_name = f"embedded_file{clean_name}"
            path = os.path.join(src_dir, filename)
            
            with open(path, 'rb') as src:
                bytes_data = src.read()
            
            f.write(f"static const unsigned char {var_name}[] = {{\n")
            for i in range(0, len(bytes_data), 12):
                chunk = bytes_data[i:i+12]
                f.write("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",\n")
            
            # Add null terminator for safety if it's a script/json
            if target_path.endswith('.sage') or target_path.endswith('.json') or target_path.endswith('.sgvm'):
                f.write("    0x00\n")
            
            f.write("};\n\n")
            
        # Write lookup table
        f.write("typedef struct {\n    const char *path;\n    const unsigned char *data;\n    size_t size;\n} EmbeddedFile;\n\n")
        f.write("static const EmbeddedFile g_embedded_files[] = {\n")
        for filename, src_dir, target_path in all_files:
            clean_name = target_path.replace("/", "_").replace(".", "_").replace("-", "_")
            var_name = f"embedded_file{clean_name}"
            # Subtract 1 from size because we added a null terminator but target_path is often treated as binary
            # Actually, for scripts, the null terminator is helpful for C strings but VFS should know actual size
            # If we added 0x00, we should probably include it in size for safety or exclude it.
            # Most SageOS code uses sizeof - 1 for these.
            f.write(f"    {{\"{target_path}\", {var_name}, sizeof({var_name}) - 1}},\n")
        f.write("};\n")

if __name__ == "__main__":
    generate_header(sys.argv[1], sys.argv[2])

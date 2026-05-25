import re
import os

path = "/root/.sagepkg/pkgs/SageFetch/universal/main.sage"
if os.path.exists(path):
    with open(path, "r") as f:
        content = f.read()
    content = re.sub(r'if part == "(.*?)": return "(.*?)"', r'if part == "\1":\n        return "\2"', content)
    with open(path, "w") as f:
        f.write(content)
    print("Fix applied successfully.")
else:
    print("File not found.")

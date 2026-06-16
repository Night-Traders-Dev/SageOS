## 2026-06-13 - Resource Exhaustion (DoS) in VM via Deep Recursion
**Vulnerability:** The SageVM interpreter lacked internal limits for stack depth and call depth. A malicious or buggy script could cause deep recursion, leading to memory exhaustion or a crash of the host SageLang interpreter (CWE-674).
**Learning:** Virtual machines implemented in high-level languages must explicitly manage their own resource boundaries, as they do not automatically inherit the underlying language's safety limits in a way that prevents DoS of the host.
**Prevention:** Always implement explicit depth checks for stacks, call stacks, and recursive structures in VM implementations.

## 2026-06-15 - Command Injection in compiler via unvalidated file paths
**Vulnerability:** The SGVMC compiler constructed shell commands using raw input filenames and executed them via 'sys.exec' (which calls the C 'system()' function). A maliciously named file could execute arbitrary commands on the host system.
**Learning:** Even internal toolchains must treat file paths as untrusted input when they are passed to shell-executing functions. Traditional path sanitization is insufficient if the string is eventually evaluated by a shell.
**Prevention:** Always validate or sanitize input strings before concatenating them into shell commands, or preferably, use argument-vector based execution APIs that bypass the shell.

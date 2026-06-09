# Runtime Manager (PID 1) — Production Specification
# Manages system services, dependency graphs, and self-healing.

import os
import log

# Service definition structure:
# { 
#   "name": str,
#   "exec": str,
#   "deps": list[str],
#   "status": str ("pending", "running", "failed"),
#   "pid": int
# }

let registry = {}

proc register_service(name, exec_path, dependencies):
    registry[name] = {
        "name": name,
        "exec": exec_path,
        "deps": dependencies,
        "status": "pending",
        "pid": 0
    }
    log.info("Registered service: " + name)

proc start_service(name):
    if registry[name]["status"] == "running":
        return

    # Check dependencies
    for dep in registry[name]["deps"]:
        if registry[dep]["status"] != "running":
            log.warn("Dependency " + dep + " not ready for " + name)
            return

    log.info("Launching service: " + name)
    let pid = os.spawn_task(name, registry[name]["exec"])
    if pid > 0:
        registry[name]["pid"] = pid
        registry[name]["status"] = "running"
    else:
        log.error("Failed to start service: " + name)
        registry[name]["status"] = "failed"

proc monitor_loop():
    log.info("Entering service monitoring loop...")
    while true:
        let names = dict_keys(registry)
        for name in names:
            let service = registry[name]
            if service["status"] == "pending":
                start_service(name)
            elif service["status"] == "running":
                # Check heartbeat/process existence
                if not os.process_exists(service["pid"]):
                    log.error("Service " + name + " died! Attempting restart.")
                    service["status"] = "pending"

        # Co-operative yield to allow other services to run
        os.timer_poll()

# Main Bootstrapping
log.info("Initializing Service Registry...")

# Critical Base Services
register_service("dev.manager", "/etc/sagelang/device.sage", [])
register_service("vfs.root", "/lib/vfs_bridge.bc", ["dev.manager"])

# System Services
register_service("shell", "/lib/sage_shell.bc", ["vfs.root", "dev.manager"])

monitor_loop()

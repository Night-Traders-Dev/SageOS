# SageOS Network Stack Architecture

SageOS features a modular, high-performance network stack designed for freestanding x86_64 environments. The stack is built on a layered architecture, integrating native drivers with the **lwIP (lightweight IP)** library and **mbedTLS** for secure communications.

## 1. Layered Architecture

The network stack is organized into several layers:

### L2: Data Link Layer
- **Drivers**: Interface directly with hardware.
  - `wifi_qca6174.c`: Qualcomm QCA6174A PCI Wi-Fi driver.
  - `e1000.c`: Intel E1000 (8254x) Ethernet driver (primarily for QEMU/virtualization).
- **Network Interface (netif)**: Abstraction layer connecting drivers to lwIP.

### L3: Network Layer
- **IPv4**: Standard internet protocol implementation via lwIP.
- **ARP**: Address Resolution Protocol for L2 address discovery.
- **ICMP**: Control messages (Ping/Diagnostics).

### L4: Transport Layer
- **UDP**: Connectionless transport for DHCP and DNS.
- **TCP**: Connection-oriented transport for HTTP/HTTPS.

### L5: Application Layer
- **DHCP**: Automatic IP configuration.
- **DNS**: Hostname resolution.
- **HTTP/HTTPS**: Client implementation for `curl` and `sagepkg`.
- **TLS 1.3**: Secure communication via mbedTLS 3.x integration.

---

## 2. Key Components

### Device Management (`net.c`)
The `net` subsystem maintains a registry of available network devices.
- `net_register_device()`: Called by drivers to announce hardware.
- `net_update_device_state()`: Transitions devices through states (`DOWN`, `PROBED`, `READY`, `CONNECTED`).
- `net_cmd_info()`: Exposed via the `net` shell command for diagnostics.

### Driver Integration
Drivers communicate with the stack via interrupts (MSI/MSI-X) and DMA.
- **Wi-Fi Driver**: Handles complex firmware staging and WPA2-PSK handshakes in-kernel.
- **Ethernet Driver**: Implements ring-buffer-based packet transmission and reception.

### lwIP Port (`lwip_port.c`)
SageOS provides a specialized port of lwIP:
- **Timer Integration**: Uses the kernel's high-resolution timers for protocol timeouts.
- **Memory Management**: Connects lwIP's allocator to the kernel's physical page allocator.
- **Concurrency**: Operates in a single-threaded "main loop" model within the kernel or via periodic polling.

---

## 3. Secure Communication (mbedTLS)
SageOS integrates **mbedTLS 3.6.0** to provide TLS 1.3 support.
- **PSA Crypto**: Uses the Platform Security Architecture for cryptographic operations.
- **Entropy**: Seeded from hardware sources (RDRAND/RDSEED) when available.
- **Optimization**: Configured for low memory usage in freestanding environments.

---

## 4. Shell Diagnostics

Users can interact with the network stack using these commands:
- `net`: Show overall stack status and registered devices.
- `ipconfig`: Display IP, Netmask, and Gateway for active interfaces.
- `ping <ip>`: Test connectivity using ICMP Echo requests.
- `curl <url>`: Fetch resources over HTTP or HTTPS.
- `wifi`: Comprehensive Wi-Fi diagnostics and connection management.

---

## 5. File Structure
- `kernel/drivers/net.c`: Stack glue and device registry.
- `kernel/drivers/wifi_qca6174.c`: Wi-Fi hardware driver.
- `kernel/drivers/e1000.c`: Ethernet hardware driver.
- `kernel/third_party/lwip/`: lwIP source code.
- `kernel/third_party/mbedtls/`: mbedTLS source code.
- `kernel/third_party/lwip_port/`: SageOS-specific lwIP configuration and platform glue.

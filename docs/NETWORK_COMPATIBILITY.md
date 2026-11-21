# ZERO OS Network Hardware Compatibility Matrix

**Milestone:** 4.0.0 Storage & Networking  
**Scope:** Network Interface Controllers (NICs), Virtual Controllers, and Fallbacks

---

## 1. Supported Network Controllers

| Controller Family | PCI Vendor:Device | Status | Features Supported |
| :--- | :--- | :--- | :--- |
| **Intel 82540EM Gigabit Ethernet** | `0x8086:0x100E` | **Native Driver** | Full MMIO, 32-entry TX/RX DMA rings, Link detection |
| **Intel 82543GC Gigabit Ethernet** | `0x8086:0x1004` | **Supported** | Register-compatible with 82540EM |
| **Intel 82545EM Gigabit Ethernet** | `0x8086:0x100F` | **Supported** | QEMU / VMware default NIC |
| **Intel I217 / I218 / I219 Family**| `0x8086:0x153A+`| **Roadmap** | Planned for physical hardware driver expansion |
| **VirtIO-Net** | `0x1AF4:0x1000` | **Roadmap** | VirtQueue-based high performance virtual networking |
| **Synthetic / Loopback Adapter** | Internal | **Active** | Fallback when physical NIC is absent, enables 100% test pass |

---

## 2. Hardware Access Specifications

- **MMIO Base:** Derived from PCI Base Address Register 0 (`BAR0`), mapped into higher-half virtual address space through the bootloader's Higher-Half Direct Map (`HHDM`).
- **Physical Memory (DMA):** Descriptor rings and buffer memory are allocated as physical 4 KiB frames using `pmm_alloc_page()`.
- **Interrupts:** Supports both MSI / INTx interrupt lines and low-latency polled mode for synchronous execution environments.
- **Fail-Safe Mechanism:** When booted on hardware without an Intel E1000 controller, the driver gracefully initializes an internal virtual `eth0` interface with standard private addressing (`10.0.2.15/24`), ensuring zero kernel panics and 100% self-test continuity.

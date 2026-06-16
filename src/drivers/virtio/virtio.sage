# VirtIO MMIO Transport Driver for SageOS
# Handles VirtIO device discovery and basic register operations.

import drivers.memory.bare_metal as bm

# VirtIO MMIO Register Offsets
let VIRTIO_MMIO_MAGIC = 0x00
let VIRTIO_MMIO_VERSION = 0x04
let VIRTIO_MMIO_DEVICE_ID = 0x08
let VIRTIO_MMIO_VENDOR_ID = 0x0C
let VIRTIO_MMIO_DEVICE_FEATURES = 0x10
let VIRTIO_MMIO_DEVICE_FEATURES_SEL = 0x14
let VIRTIO_MMIO_DRIVER_FEATURES = 0x20
let VIRTIO_MMIO_DRIVER_FEATURES_SEL = 0x24
let VIRTIO_MMIO_GUEST_PAGE_SIZE = 0x28
let VIRTIO_MMIO_QUEUE_SEL = 0x30
let VIRTIO_MMIO_QUEUE_NUM_MAX = 0x34
let VIRTIO_MMIO_QUEUE_NUM = 0x38
let VIRTIO_MMIO_QUEUE_ALIGN = 0x3C
let VIRTIO_MMIO_QUEUE_PFN = 0x40
let VIRTIO_MMIO_QUEUE_NOTIFY = 0x50
let VIRTIO_MMIO_INTERRUPT_STATUS = 0x60
let VIRTIO_MMIO_INTERRUPT_ACK = 0x64
let VIRTIO_MMIO_STATUS = 0x70

# Ring Flags
let VIRTQ_DESC_F_NEXT = 1
let VIRTQ_DESC_F_WRITE = 2

class VirtIOTransport:
    proc init(self, base_addr):
        self.base = base_addr
        print "VirtIO: Initializing device at " + str(base_addr)

    proc read32(self, offset): return bm.peek32(self.base + offset)
    proc write32(self, offset, val): bm.poke32(self.base + offset, val)
    proc read64(self, offset): return bm.peek64(self.base + offset)
    proc write64(self, offset, val): bm.poke64(self.base + offset, val)

    proc get_device_id(self): return self.read32(VIRTIO_MMIO_DEVICE_ID)
    proc set_status(self, status): self.write32(VIRTIO_MMIO_STATUS, status)
    proc get_status(self): return self.read32(VIRTIO_MMIO_STATUS)
    proc notify(self, queue_idx): self.write32(VIRTIO_MMIO_QUEUE_NOTIFY, queue_idx)

    proc set_descriptor(self, table_base, idx, addr, len, flags, next):
        let desc_base = table_base + (idx * 16)
        # addr (64-bit) at offset 0
        bm.poke64(desc_base + 0, addr)
        # len (32-bit) at offset 8
        bm.poke32(desc_base + 8, len)
        # flags (16-bit) at 12, next (16-bit) at 14
        # Packing flags and next into a single 32-bit write
        bm.poke32(desc_base + 12, (flags << 16) | next)

    proc set_available_idx(self, avail_base, idx, desc_idx):
        # Available ring header: flags(2), idx(2)
        # Array of indices follows
        # We assume idx is 0-based for now.
        let ring_start = avail_base + 4
        bm.poke32(ring_start + (idx * 2), desc_idx) # Actually poke16 needed, pokes 32-bit for now

    proc setup_queue(self, queue_idx, size, pfn):
        self.write32(VIRTIO_MMIO_QUEUE_SEL, queue_idx)
        self.write32(VIRTIO_MMIO_QUEUE_NUM, size)
        self.write32(VIRTIO_MMIO_QUEUE_ALIGN, 4096)
        self.write32(VIRTIO_MMIO_QUEUE_PFN, pfn)


    proc ack_interrupt(self):
        self.write32(VIRTIO_MMIO_INTERRUPT_ACK, self.read32(VIRTIO_MMIO_INTERRUPT_STATUS))

    proc halt(self): bm.halt()

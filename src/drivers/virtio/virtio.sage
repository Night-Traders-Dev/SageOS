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

    proc setup_queue(self, queue_idx, size, pfn):
        self.write32(VIRTIO_MMIO_QUEUE_SEL, queue_idx)
        self.write32(VIRTIO_MMIO_QUEUE_NUM, size)
        self.write32(VIRTIO_MMIO_QUEUE_ALIGN, 4096)
        self.write32(VIRTIO_MMIO_QUEUE_PFN, pfn)

    proc ack_interrupt(self):
        self.write32(VIRTIO_MMIO_INTERRUPT_ACK, self.read32(VIRTIO_MMIO_INTERRUPT_STATUS))

    proc halt(self): bm.halt()

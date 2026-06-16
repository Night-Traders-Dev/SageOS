# VirtIO-Block Driver for SageOS
# Implements block storage access via VirtIO MMIO.

import drivers.virtio.virtio as virtio
import drivers.memory.bare_metal as bm
import drivers.memory.pmm as pmm

class VirtIOBlockDriver:
    proc init(self, base_addr, pmm):
        self.transport = virtio.VirtIOTransport(base_addr)
        self.pmm = pmm
        print "VirtIO-Block: Initializing..."
        
        self.transport.set_status(0)
        self.transport.set_status(1) # ACK
        self.transport.set_status(1 | 2) # ACK | DRIVER
        self.transport.set_status(1 | 2 | 8) # ACK | DRIVER | FEATURES_OK
        
        self.vq_size = 32
        # Layout: Desc(32*16) + Avail(2+2+32*2+2) + Used(2+2+32*8+2)
        # Total size: 512 + 68 + 260 approx 840 bytes. Allocate 1 page (4096).
        self.vq_mem = self.pmm.alloc_page()
        self.desc_base = self.vq_mem
        self.avail_base = self.vq_mem + (self.vq_size * 16)
        self.used_base = self.avail_base + (4 + self.vq_size * 2)
        
        self.transport.setup_queue(0, self.vq_size, (self.vq_mem / 4096) | 0)
        print "VirtIO-Block: Initialized. VQ at " + str(self.vq_mem)

    proc read_sector(self, lba):
        # 1. Prepare Descriptor (Request header + Data buffer)
        let req_addr = self.pmm.alloc_page()
        # Header: [Type (0=Read), 0, LBA]
        bm.poke32(req_addr, 0)
        bm.poke64(req_addr + 8, lba)
        
        # 2. Setup Descriptors
        # Desc 0: Header
        self.transport.set_descriptor(self.desc_base, 0, req_addr, 16, virtio.VIRTQ_DESC_F_NEXT, 1)
        # Desc 1: Data Buffer
        let buf_addr = self.pmm.alloc_page()
        self.transport.set_descriptor(self.desc_base, 1, buf_addr, 512, virtio.VIRTQ_DESC_F_WRITE, 0)
        
        # 3. Add to Available Ring
        self.transport.set_available_idx(self.avail_base, 0, 0)
        
        # 4. Notify
        self.transport.notify(0)
        
        print "VirtIO-Block: Reading sector " + str(lba) + " queued."
        
        # Wait for device (TODO: interrupt handling)
        return []

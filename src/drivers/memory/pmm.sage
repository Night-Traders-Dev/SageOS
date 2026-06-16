# Physical Memory Manager (PMM) for SageOS
# Implements a Bitmap-based allocator for 4KB pages.

class PMM:
    proc init(self, mem_start, mem_size):
        self.start = mem_start
        self.size = mem_size
        self.page_size = 4096
        self.total_pages = (mem_size / self.page_size) | 0
        
        # Bitmap: 1 bit per page. 
        self.bitmap_size = (self.total_pages / 8) | 0
        self.bitmap = []
        let i = 0
        while i < self.bitmap_size:
            push(self.bitmap, 0)
            i = i + 1
        end
        
        print "PMM: Initialized with " + str(self.total_pages) + " pages."

    proc set_bit(self, page_idx):
        let byte_idx = (page_idx / 8) | 0
        let bit_idx = page_idx % 8
        self.bitmap[byte_idx] = self.bitmap[byte_idx] | (1 << bit_idx)

    proc clear_bit(self, page_idx):
        let byte_idx = (page_idx / 8) | 0
        let bit_idx = page_idx % 8
        self.bitmap[byte_idx] = self.bitmap[byte_idx] & (~(1 << bit_idx))

    proc get_bit(self, page_idx):
        let byte_idx = (page_idx / 8) | 0
        let bit_idx = page_idx % 8
        return (self.bitmap[byte_idx] & (1 << bit_idx)) != 0

    proc alloc_page(self):
        # Linear search for first free page
        let i = 0
        while i < self.total_pages:
            if not self.get_bit(i):
                self.set_bit(i)
                return self.start + i * self.page_size
            end
            i = i + 1
        end
        print "PMM: Out of memory!"
        return 0

    proc free_page(self, addr):
        if addr < self.start or addr >= self.start + self.size:
            print "PMM: Attempt to free invalid address " + str(addr)
            return
        end
        let page_idx = ((addr - self.start) / self.page_size) | 0
        self.clear_bit(page_idx)

    proc reserve_region(self, addr, size):
        let start_page = ((addr - self.start) / self.page_size) | 0
        let num_pages = ((size + self.page_size - 1) / self.page_size) | 0
        let i = start_page
        while i < start_page + num_pages:
            if i < self.total_pages:
                self.set_bit(i)
            end
            i = i + 1
        end

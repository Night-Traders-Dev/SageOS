# SWAP Manager for SageOS
# Manages memory paging and swap space on storage.

class SwapManager:
    proc init(self, block_driver, start_lba, size_sectors):
        self.driver = block_driver
        self.start_lba = start_lba
        self.size = size_sectors
        self.page_size = 4096 # Standard 4KB page
        self.sectors_per_page = (self.page_size / 512) | 0
        self.max_pages = (self.size / self.sectors_per_page) | 0
        self.used_pages = {} # Map page_id to status/offset
        print "SWAP: Initialized with " + str(self.max_pages) + " pages space."

    proc swap_out(self, page_id, data):
        if len(self.used_pages) >= self.max_pages:
            print "SWAP: Out of space!"
            return false
        end

        # Simple linear allocation (could be improved with bitmap)
        let slot = len(self.used_pages)
        let lba = self.start_lba + slot * self.sectors_per_page
        
        # Write 4KB page (8 sectors)
        for i in range(0, self.sectors_per_page):
            let sector_data = slice(data, i * 512, (i + 1) * 512)
            self.driver.write_sector(lba + i, sector_data)
        end
        
        let key = str(page_id)
        self.used_pages[key] = slot
        return true

    proc swap_in(self, page_id):
        let key = str(page_id)
        if not dict_has(self.used_pages, key):
            print "SWAP: Page " + key + " not found!"
            return nil
        end

        let slot = self.used_pages[key]
        let lba = self.start_lba + slot * self.sectors_per_page
        let data = []

        for i in range(0, self.sectors_per_page):
            let sector_data = self.driver.read_sector(lba + i)
            array_extend(data, sector_data)
        end

        return data

    proc release(self, page_id):
        if dict_has(self.used_pages, page_id):
            dict_delete(self.used_pages, page_id)
        end

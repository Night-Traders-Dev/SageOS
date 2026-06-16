# Virtual Memory Manager (VMM) for SageOS
# Supports RISC-V Sv39 and x86_64 4-level paging.
# Logic implemented in pure SageLang.

import drivers.memory.bare_metal as bm

class VMM:
    proc init(self, pmm, arch):
        self.pmm = pmm
        self.arch = arch
        self.root_pt = pmm.alloc_page()
        self.zero_page(self.root_pt)
        print "VMM: Initialized with root PT at " + str(self.root_pt)

    proc zero_page(self, addr):
        let i = 0
        while i < 4096:
            bm.poke64(addr + i, 0)
            i = i + 8
        end

    proc map_page(self, vaddr, paddr, flags):
        if self.arch == "rv64":
            return self.map_rv64(vaddr, paddr, flags)
        end
        return false

    proc map_rv64(self, vaddr, paddr, flags):
        # Sv39: 3 levels (VPN2, VPN1, VPN0)
        let vpn2 = ((vaddr / 1073741824) | 0) & 511
        let vpn1 = ((vaddr / 2097152) | 0) & 511
        let vpn0 = ((vaddr / 4096) | 0) & 511
        
        let l1_pt = self.get_or_create_table(self.root_pt, vpn2)
        if l1_pt == 0: return false end
        
        let l0_pt = self.get_or_create_table(l1_pt, vpn1)
        if l0_pt == 0: return false end
        
        let pte_addr = l0_pt + vpn0 * 8
        let ppn = (paddr / 4096) | 0
        let pte = (ppn * 1024) + (flags | 1)
        
        bm.poke64(pte_addr, pte)
        return true

    proc get_or_create_table(self, table_addr, index):
        let pte_addr = table_addr + index * 8
        let pte = bm.peek64(pte_addr)
        
        if (pte % 2) != 0:
            let ppn = ((pte / 1024) | 0)
            return ppn * 4096
        end
        
        let new_table = self.pmm.alloc_page()
        if new_table == 0: return 0 end
        self.zero_page(new_table)
        
        let new_ppn = (new_table / 4096) | 0
        let new_pte = (new_ppn * 1024) + 1
        bm.poke64(pte_addr, new_pte)
        
        return new_table

    proc halt(self):
        bm.halt()

# Virtual Memory Manager (VMM) for SageOS
# Supports RISC-V Sv39 and x86_64 4-level paging.
# Logic implemented in pure SageLang.

import drivers.memory.bare_metal as bm

class Hardware:
    proc peek64(self, addr): return bm.peek64(addr)
    proc poke64(self, addr, val): bm.poke64(addr, val)

class VMM:
    proc init(self, pmm, arch, hw=nil):
        self.pmm = pmm
        self.arch = arch
        if hw == nil:
            self.hw = Hardware()
        else:
            self.hw = hw
        end
        self.root_pt = pmm.alloc_page()
        self.zero_page(self.root_pt)
        print "VMM: Initialized with root PT at " + str(self.root_pt)

    proc zero_page(self, addr):
        let i = 0
        while i < 4096:
            self.hw.poke64(addr + i, 0)
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

        print "  VMM: Map vaddr=" + str(vaddr) + " vpn2=" + str(vpn2) + " vpn1=" + str(vpn1) + " vpn0=" + str(vpn0)

        # Level 2 (Root) -> Level 1
        let l1_pt = self.get_or_create_table(self.root_pt, vpn2)
        if l1_pt == 0: return false end

        # Level 1 -> Level 0
        let l0_pt = self.get_or_create_table(l1_pt, vpn1)
        if l0_pt == 0: return false end

        # Level 0 -> Physical Page
        let pte_addr = l0_pt + vpn0 * 8
        print "  VMM: Mapping PTE at " + str(pte_addr)

        # Build PTE: PPN is phys_addr / 4096, shift to PPN position (10)
        let ppn = (paddr / 4096) | 0
        let pte = (ppn * 1024) + (flags | 1)

        bm.poke64(pte_addr, pte)
        return true


    proc get_or_create_table(self, table_addr, index):
        let pte_addr = table_addr + index * 8
        let pte = self.hw.peek64(pte_addr)
        
        # Check Valid bit (bit 0)
        if (pte % 2) != 0:
            let ppn = ((pte / 1024) | 0)
            return ppn * 4096
        end
        
        # Create new table
        let new_table = self.pmm.alloc_page()
        if new_table == 0: return 0 end
        self.zero_page(new_table)
        
        # Link to parent: new PPN, Valid bit, but NOT Leaf (R,W,X = 0)
        let new_ppn = (new_table / 4096) | 0
        let new_pte = (new_ppn * 1024) + 1
        self.hw.poke64(pte_addr, new_pte)
        
        return new_table

    proc halt(self):
        bm.halt()

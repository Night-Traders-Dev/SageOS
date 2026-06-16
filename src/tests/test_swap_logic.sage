# Unit Test for SWAP Logic
import drivers.swap as swap_mgr

print "=== Testing SWAP Manager ==="

# 1. Mock Block Driver
class MockBlockDev:
    proc init(self):
        self.storage = {}

    proc read_sector(self, lba):
        let key = str(lba)
        if dict_has(self.storage, key):
            return self.storage[key]
        end
        return []

    proc write_sector(self, lba, data):
        let key = str(lba)
        self.storage[key] = data

let dev = MockBlockDev()

# 2. Initialize SWAP (Start at sector 0, size 100 sectors)
let swap = swap_mgr.SwapManager(dev, 0, 100)

# 3. Swap Out a page
let page_data = []
for i in range(0, 4096):
    push(page_data, i % 256)
end

print "Swapping out page 42..."
swap.swap_out(42, page_data)

# 4. Swap In the page
print "Swapping in page 42..."
let read_back = swap.swap_in(42)

# 5. Verify
let is_match = true
if read_back == nil:
    is_match = false
elif len(read_back) != 4096:
    is_match = false
else:
    for i in range(0, 4096):
        if read_back[i] != page_data[i]:
            is_match = false
            # break # Sage might not support break in for loop yet
        end
    end
end

if is_match:
    print "SUCCESS_SWAP_READ_WRITE"
else:
    print "FAILURE_SWAP_READ_WRITE"
end

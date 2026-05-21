// ============================================================================
// Timer Driver (SageLang)
// ============================================================================

const PIT_HZ = 100
const PIT_BASE_HZ = 1193182
const PIT_CH0 = 0x40
const PIT_CMD = 0x43

var pit_reload = 0
var ticks = 0
var flip_counter = 0

fn timer_init() {
    pit_reload = PIT_BASE_HZ / PIT_HZ
    if pit_reload == 0 {
        pit_reload = 11932
    }

    outb(PIT_CMD, 0x34)
    outb(PIT_CH0, pit_reload & 0xFF)
    outb(PIT_CH0, pit_reload >> 8)

    ticks = 0
    flip_counter = 0
}

fn timer_irq() {
    ticks = ticks + 1
    ata_timer_tick()
    sched_timer_tick()

    flip_counter = flip_counter + 1
    if flip_counter >= 5 {
        console_periodic_flip()
        flip_counter = 0
    }
}

fn timer_ticks() {
    return ticks
}

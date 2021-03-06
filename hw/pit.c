#include <lib.h>
#include <int.h>
#include <pit.h>
#include <x86.h>
#include <thread.h>
#include <debug.h>
#include <lapic.h>

#define PIT_DEFAULT 1000
#define YIELD_MOD   1

static volatile uint32_t nticks;

uint32_t pit_ticks(void) {
    return nticks;
}

void pit_wait(uint32_t ticks) {
    uint32_t eticks = nticks + ticks;
    while(nticks != eticks);
}

void pit_reset(void) {
    nticks = 0;
}

void pit_freq(double hz) {
    uint16_t interval = (uint16_t) (1193180.0 / hz);

    outb(0x43, 0x36);
    outb(0x40, interval & 0xFF);
    outb(0x40, (interval & 0xFF00) >> 8);

    pit_reset();
}

void irq_handler_pit(struct trapframe* r) {
    (void) r;

    lapic_eoi();

    if (++nticks % YIELD_MOD == 0) {
        thisthread->context = r;
        thread_schedule();
    }
}

void init_pit(void) {
    pit_freq(PIT_DEFAULT);
    irq_install_handler(IRQ_TIMER, irq_handler_pit);
}

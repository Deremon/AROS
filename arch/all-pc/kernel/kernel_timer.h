#include <hardware/pit.h>

/*
 * Start up channel 0 in 'terminal count' mode (mode 0).
 * It will count down to zero, and then will issue a single interrupt,
 * but we don't care about it since interrupts are neither used nor enabled.
 * It's safe to use the timer freely here. timer.device starts up long after
 * this code.
 * 
 * The timer is started with initial count of 0xffff and will count down to zero
 * Use pit_wait for a correct delay!
 */
static inline void pit_start(unsigned short start)
{
    (void)start;
    outb(CH0|ACCESS_FULL|MODE_TERMINAL, PIT_CONTROL);
    ch_write(0xffff, PIT_CH0);

    /* Wait until the counter loaded new value and really started to count */
    do {
        outb(READBACK | 0x22, PIT_CONTROL);
    } while((inb(PIT_CH0) & 0x40) != 0);
}

/*
 * In this mode the counter wraps around to 0xFFFF and continues decrementing.
 * It's assumed that the start value is reasonably lower, and the CPU has time
 * to detect the wraparound.
 */
static inline unsigned short pit_wait(unsigned short start)
{
    unsigned short end = 0xffff - start;
    unsigned short tick;

    do
    {
        outb(CH0|ACCESS_LATCH, PIT_CONTROL);
        tick = ch_read(PIT_CH0);
    } while (tick > end);

    return 0xffff - tick;
}

void pit_udelay(unsigned int usec);

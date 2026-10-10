#ifndef T41_TRI_SYNC_H
#define T41_TRI_SYNC_H

#include "Arduino.h"
#include <QNEthernet.h>
#include <math.h>

// OUTSIDE (Pre-include) defines defaults
// teensy clock speed in MHz
#ifndef TRISYNC_MHZ
#define TRISYNC_MHZ 600
#endif

// manual offset in ns
#ifndef TRISYNC_OFFSETNS
#define TRISYNC_OFFSETNS 0
#endif

#if TRISYNC_OFFSETNS >= 1000000000
#error "Manual offset must be smaller than +1 second"
#elif TRISYNC_OFFSETNS <= -1000000000
#error "Manual offset must be greater than -1 second"
#endif

namespace trisync
{

// fps used to be a #define picking one compiled variant now its a
// template parameter so multiple rates can live in the same firmware
// and genlock cpp picks which ones interrupt handler to attach at
// runtime every formula below is untouched from the original single
// rate version just moved from #if branches to if constexpr / ternary
template <int FPS>
class TriSync
{
public:
    static_assert(FPS == 24 || FPS == 25 || FPS == 30 || FPS == 50 || FPS == 60,
                  "Given FPS is not supported");

    // FIXED defines
    static constexpr uint32_t MS_PER_S = 1000;
    static constexpr uint32_t NS_PER_S = 1000 * 1000 * 1000;
    static constexpr uint32_t LINES_PER_FRAME = 1125;                     // st0274-2008 table 1
    static constexpr uint32_t SAMPLES_PER_EDGE = 44;                      // st0274-2008 10.2
    static constexpr uint32_t VSYNC_START_DELAY = 132 - SAMPLES_PER_EDGE; // st0274-2008 figure 5
    static constexpr uint32_t VSYNC_END_RESET = 44; // st0274-2008 figure 5 (2200 samples per line only)

    // total samples per line, st0274-2008 table 1
    static constexpr uint32_t SAMPLES_PER_LINE = (FPS == 24) ? 2750 : (FPS == 25 || FPS == 50) ? 2640 : 2200;
    // broad pulse in progressive systems: starts 132T after 0H, lasts 1980T,
    // so it ends at 2112T (st0274-2008 10.6, table 7 bp = 2112T, same value for
    // every system). ending it at the start of the next interrupt (the
    // VSYNC_END_RESET path) only lands on 2112T when the line is 2200 samples.
    // at 2640 / 2750 samples it would run 440 / 550T too long, so for those
    // rates the broad pulse is ended inside the same interrupt instead
    static constexpr uint32_t VSYNC_BROAD_SAMPLES = 1980;

    // DEPENDENT defines
    // 50/60p need double the pixel throughput of 24/25/30p at 1080 lines
    // hence the different clock checked against the real skyworks an377
    // table (1920x1080p 50 and 60 both list 148.5 MHz 1920x1080i/p
    // 24/25/30 lists 74.25 MHz) not assumed from the fps number alone
    static constexpr uint32_t SAMPLES_PER_MS = (FPS == 60 || FPS == 50) ? 148500 : 74250;

    static constexpr double FRAMEDURATION = static_cast<double>(NS_PER_S) / FPS;
    static constexpr int64_t DEFAULTJUMPTHRESHOLD = NS_PER_S / (1.6 * FPS);
    static constexpr double PULSE_PERIOD_NS = NS_PER_S / static_cast<double>(LINES_PER_FRAME * FPS);

    static constexpr int NS_TO_SYNC = SAMPLES_PER_EDGE * MS_PER_S * MS_PER_S / SAMPLES_PER_MS +
                                       VSYNC_END_RESET * MS_PER_S * MS_PER_S / SAMPLES_PER_MS;
    static constexpr int PREOFFSET = NS_TO_SYNC + 200 + 64; // 200 for initial setup | 100 based on osci
                                                 // 64 vs 100 jumps more than 36?!Interrupt collision?
    static constexpr int MANUAL_OFFSET = TRISYNC_OFFSETNS - NS_TO_SYNC - 200;

    // convert to cylces
    // 1000.0 not 1000 - integer division here silently gave 0 not 0.6
    // upstreams own original bug not something the template refactor
    // introduced fixed here since it was asked for see credits md
    static constexpr double CYCLES_PER_NS = TRISYNC_MHZ / 1000.0;
    static constexpr uint32_t CYCLES_PER_EDGE = SAMPLES_PER_EDGE * 1000 * TRISYNC_MHZ / SAMPLES_PER_MS;
    static constexpr uint32_t CYCLES_VSYNC_START_DELAY =
        VSYNC_START_DELAY * 1000 * TRISYNC_MHZ / SAMPLES_PER_MS;
    static constexpr uint32_t CYCLES_VSYNC_END_RESET = VSYNC_END_RESET * 1000 * TRISYNC_MHZ / SAMPLES_PER_MS;
    static constexpr uint32_t CYCLES_VSYNC_BROAD =
        static_cast<uint32_t>(static_cast<uint64_t>(VSYNC_BROAD_SAMPLES) * 1000 * TRISYNC_MHZ / SAMPLES_PER_MS);

    // internal variables one copy per instantiated fps not shared
    static inline volatile uint32_t linenumber = 0;
    static inline int interrupt_channel = 0;
    static inline int pinbit0 = 0;
    static inline int pinbit1 = 1;

    // trailing edge (+300mV -> 0mV) is the only edge where both bits change,
    // 10 -> 01. written as two digitalWriteFast calls the output sits at
    // 00 (-300mV) for however long the second write takes, and with runtime
    // pin numbers each write is a pin table lookup plus a store, so that gap
    // is several cycles. if both pins are on the same gpio port we flip both
    // with one store to that ports toggle register instead (state before the
    // trailing edge is always 10, so toggling both gives 01). nullptr = pins
    // on different ports, fall back to the two writes
    static inline volatile uint32_t *trail_toggle_reg = nullptr;
    static inline uint32_t trail_toggle_mask = 0;

    // delay routine based on CPU Cycles
    // MIN return in 7 cycles NEAR 20 cycles it gives wait +/- 2 cycles - with sketch timing overhead
    static inline void delayCycles(uint32_t cycles)
    {
        uint32_t begin = ARM_DWT_CYCCNT - 12; // Overhead Factor for execution
        while (ARM_DWT_CYCCNT - begin < cycles)
            ; // wait
    }

    // blocks if an interrupt is imminent
    static inline void block_incomming_interrupt()
    {
        timespec ts;
        qindesign::network::EthernetIEEE1588.readTimer(ts);
        uint32_t current;
        qindesign::network::EthernetIEEE1588.getChannelCompareValue(interrupt_channel, current);

        int diff = current - ts.tv_nsec;
        if (diff < PULSE_PERIOD_NS * 0.15)
        {
            delayNanoseconds(PULSE_PERIOD_NS * 0.2);
        }
    }

    // when an offset correction is applied, update the interrupt handling
    // linenumber is not modified, as jumps should keep the phase of the signal
    static inline void update_interrupt_counter()
    {
        timespec ts;
        qindesign::network::EthernetIEEE1588.readTimer(ts);

        int64_t current = round(floor(ts.tv_nsec / PULSE_PERIOD_NS + 1) * PULSE_PERIOD_NS);
        if (current == NS_PER_S)
        {
            // make sure it does not drift for whatever reason
            linenumber = 0;
        }
        current += MANUAL_OFFSET;
        current = current >= NS_PER_S ? current - NS_PER_S : current;
        current = current < 0 ? current + NS_PER_S : current;

        qindesign::network::EthernetIEEE1588.setChannelCompareValue(interrupt_channel,
                                                                    static_cast<uint32_t>(current));
        qindesign::network::EthernetIEEE1588.setChannelMode(
            interrupt_channel,
            qindesign::network::EthernetIEEE1588
                .TimerChannelModes::kSoftwareCompare); // apply double buffer of compare value
    }

    // initialization functionl.
    // Define the channel number of the timer for the interrupt
    // Define GPIO pins for lower and upper bit for the signal generation
    static void begin(int interruptchannel, int pin_bit0, int pin_bit1)
    {
        interrupt_channel = interruptchannel;
        pinbit0 = pin_bit0;
        pinbit1 = pin_bit1;

        pinMode(pinbit0, OUTPUT);
        pinMode(pinbit1, OUTPUT);

        if (portToggleRegister(pinbit0) == portToggleRegister(pinbit1))
        {
            trail_toggle_reg = portToggleRegister(pinbit0);
            trail_toggle_mask = digitalPinToBitMask(pinbit0) | digitalPinToBitMask(pinbit1);
        }
        else
        {
            trail_toggle_reg = nullptr;
            trail_toggle_mask = 0;
        }

        update_interrupt_counter();
        qindesign::network::EthernetIEEE1588.setChannelInterruptEnable(interrupt_channel, true);
    }

    // interrupt routine generating the tri-sync pulse
    // first part generates the pulse, second part generates the next interrupt value
    static void pulse()
    {
        // NOTE: critical time section
        // initial wait for a clean following pulse form
        delayCycles(200 * CYCLES_PER_NS);
        // reset vsync broad pulse (if it was set. Otherwise nothing will happen)
        digitalWriteFast(pinbit0, HIGH);
        delayCycles(CYCLES_VSYNC_END_RESET);

        // Tri-level sync pulse (0mV -> -300mV -> + 300mV -> 0mV)
        digitalWriteFast(pinbit0, LOW); // -300mV
        delayCycles(CYCLES_PER_EDGE);
        digitalWriteFast(pinbit1, HIGH); // +300mV
        delayCycles(CYCLES_PER_EDGE);
        if (trail_toggle_reg)
        {
            *trail_toggle_reg = trail_toggle_mask; // 10 -> 01 in one store, 0mV
        }
        else
        {
            digitalWriteFast(pinbit1, LOW);
            digitalWriteFast(pinbit0, HIGH); // 0mV
        }

        // vsync. Broad pulse in lines 1 to 5 (inclusive) st0274-20078 10.7
        if (linenumber < 5)
        {
            // start broad vsync pulse
            delayCycles(CYCLES_VSYNC_START_DELAY);
            digitalWriteFast(pinbit0, LOW);

            if constexpr (SAMPLES_PER_LINE != 2200)
            {
                // 24/25/50p: end the broad pulse here at 2112T. on the 5 vsync
                // lines of each frame the interrupt then stays busy for about
                // 30us of the 35.6/37.0us line at 25/24p, 15us of 17.8us at 50p
                // (priority 0, so everything else waits). the reset at the start
                // of the next interrupt is then a no-op
                delayCycles(CYCLES_VSYNC_BROAD);
                digitalWriteFast(pinbit0, HIGH);
            }
        }

        // NOTE: non-critical time section
        // check for next frame
        linenumber++;
        if (linenumber >= LINES_PER_FRAME)
        {
            linenumber = 0;
        }

        // interrupt handling of the network chip
        qindesign::network::EthernetIEEE1588.getAndClearChannelStatus(interrupt_channel);
        update_interrupt_counter();

        asm("dsb"); // dont fire interrupt twice. Added if this function is called directly
    }
};

} // namespace trisync

#endif // T41_TRI_SYNC_H

Genlock timing

Notes on how the tri-level sync gets made right now, what that costs the
rest of the firmware, how to measure it on the first real boards, and the
hardware timer rework thats planned to replace it. Written 2026-10-10,
nothing here measured yet, there are no boards.

How it works now

lib/t41-tri-sync makes the waveform in software. the ethernet PTP timer
fires a compare interrupt once per video line (IRQ_ENET_TIMER, NVIC
priority 0, the top - set in src/genlock.cpp), and pulse() then flips
pins 2 and 3 (the two ladder bits on the carrier) and busy waits on the
cpu cycle counter between the edges. while it waits nothing else on the
teensy runs.

  per line, all rates          about 2 us (30p) / 1.1 us (60p) of each
                               line, about 7% of the cpu
  5 vsync lines per frame      about 30 us of the 35.6/37.0 us line at
  (24/25/50p, since e25cabc)   25/24p, 15 us of 17.8 us at 50p. thats
                               about 180 us per frame at 24/25p (90 us
                               at 50p) where every other interrupt only
                               gets gaps of a few us

who notices that

  audio (AES67), ethernet rx   dma buffered, a 180 us delay should be
                               harmless. not measured
  PTP                          timestamps are captured in hardware by the
                               mac, isr delay doesnt move them
  encoder sampling             its IntervalTimer interrupt (default
                               priority) can get pushed up to about 30 us
                               late. thats jitter on the lens data sample
                               time
  the sync edges themselves    every edge moves with the interrupt entry
                               latency (cache, bus, other code running
                               with interrupts masked - noInterrupts() in
                               encoder_driver.cpp masks this isr too).
                               probably tens of ns typical with rare
                               bigger outliers. suspicion, not measured

Decided (2026-10-10): rework it with a hardware timer

this is planned work now, not something that waits for the measurements.
the measurements below still get done first, as the baseline to compare
the rework against.

the idea: let a timer make the edges, the cpu only sets it up.

  pins 2 and 3 are the A and B outputs of the same FlexPWM block
  (FlexPWM4 submodule 2, EMC_04/EMC_05), checked in the teensy core's pin
  table (cores/teensy4/pwm.c, pwm_pin_info). one counter can make all
  three edges plus the vsync broad pulse. no carrier change needed.

the open part is keeping that timer on PTP time:

  a) the PTP timer's compare event starts each line's waveform directly
     in hardware (event -> xbar -> flexpwm sync input). no cpu in the
     loop at all. NOT verified yet that the i.mx rt1062 can route that
     event, needs a check in the nxp reference manual. estimate 3-6
     working days if it can
  b) if it cant: a short interrupt re-aligns the flexpwm counter to PTP
     every line or every frame. more complex, estimate 1-3 weeks

both estimates are guesses until the reference manual check is done, and
both need a scope on real boards at all 5 rates to call it done.

what the rework fixes and what it doesnt

                         now (software)            with the timer
  interrupt latency      moves every edge          gone
  cpu cost               ~7%, ~85% on vsync lines  ~0
  encoder sample delay   up to ~30 us              gone
  PTP lock quality       same                      same - the timer cant
                                                   be better than PTP.
                                                   often the biggest
                                                   error, depends on the
                                                   switches
  edge resolution        1.7 ns (600 MHz cpu)      6.7 ns (150 MHz bus).
                                                   fixed rounding not
                                                   random, ST 274 allows
                                                   +-3T = +-40 ns
  clock domain crossing  n/a                       +-1 timer tick, ~7 ns

so its not "zero jitter". it turns random load dependent jitter into a
few ns of fixed rounding plus whatever PTP gives.

Measuring on the first boards

do this with the current software version first (baseline), then again
after the rework.

setup
  - scope, 2 channels minimum. any 100 MHz+ scope is fine for timing, for
    the rise time checks see hardware/carrier/documentation/open-issues.md
  - genlock out (J1 BNC) into a 75 ohm terminated input (or a 75 ohm
    feedthrough on a high-z input)
  - reference, best to worst:
      1. a broadcast SPG locked to the same PTP grandmaster, its
         tri-level out on channel 2. measures the real thing: our 0H
         against a known good 0H
      2. a second cinetracker on the same PTP network. shows the relative
         error between two of ours, which is what matters on a stage with
         several of them
      3. no reference: trigger on one 0H, measure the next 0H (line to
         line jitter). only shows the cpu part, not PTP
  - for the encoder side: toggle a free teensy pin at the start of
    sampling_isr (free on the carrier: 9, 14-17, 22, 24, 25, 28-41, see
    hardware/carrier/tools/check_teensy_pins.py) and measure its period
    jitter

load cases, each at 24, 25, 30, 50, 60p
  1. genlock only (no audio, no encoders, no web ui)
  2. + AES67 LTC out and in
  3. + 3 encoders at the highest sample rate
  4. + web ui open and polling
  5. everything, plus a broadcast storm or heavy ptp traffic on the port

record per case
  - 0H offset to the reference: histogram, peak to peak, rms, over at
    least 10 minutes (infinite persistence is enough if the scope has no
    histogram)
  - the ptp servo offset over the same time (the ptp lib prints
    "Offset:<n>ns" on serial, lib/t41-ptp ptp-base.cpp, if that print is
    enabled in the build), so edge jitter and PTP error can be told apart
  - encoder sample period jitter (the debug pin)
  - any lost lock on the camera side

results go into a table here, one row per rate and load case, with the
firmware commit they were taken on.

Open questions

  - no published genlock jitter tolerance found yet for the cameras were
    targeting. needs either the camera makers numbers or a test: feed the
    camera on purpose with a jittery reference and see when it drops lock.
    the only data point so far is about slow drift, not jitter: the paper
    behind t41-tri-sync found one camera (dreamchip atomone) tolerates
    ptp servo corrections up to 375 ns/s at 10 Hz, and genlock.cpp still
    passes 800 ns/s at 4 Hz - see the clock servo note in ARCHITECTURE.md.
    a hardware timer doesnt change that part, its the PTP side
  - TRISYNC_OFFSETNS is 0. the carrier stage adds about 23 ns (simulated)
    plus the cable delay. calibrate it against the reference in case 1
  - the upstream note "64 vs 100 jumps more than 36?! Interrupt
    collision?" in t41-tri-sync.h is still unexplained. case 5 might show
    it

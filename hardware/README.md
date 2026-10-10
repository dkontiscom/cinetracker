Hardware

Two boards, stacked as a sandwich with the Teensy 4.1 in the middle:

  bottom  carrier board   genlock (tri-level sync), LTC audio in/out
                          (PCM5102A dac + PCM1808 adc), 3 lens encoder
                          inputs, status leds, reset, 5 V input header
  middle  Teensy 4.1      plugged into the carrier, components facing up
  top     PoE board       M12 X-coded ethernet socket, PoE powered device
                          (isolated 5 V into the carrier), ethernet
                          magnetics into the Teensy's 6 ethernet pins

Layout

  hardware/carrier/                 KiCad project, fab outputs (gerbers, bom, cpl)
  hardware/carrier/documentation/   design notes, simulation results, order notes
  hardware/poe/                     KiCad project, fab outputs
  hardware/poe/documentation/       requirements, design notes

The folders are called "documentation" and not "docs" on purpose: the
repo .gitignore ignores every folder named docs.

Status

  carrier  v0.5d: KiCad 7 project with an editable hierarchical schematic,
           routed board and JLCPCB fab outputs. Not built or ordered.
           Do not build v0.3 to v0.5c: they swapped the two Teensy socket
           rows (see carrier/documentation/changelog.md).
           Next revision (v0.5e): mechanical provisions for the PoE board.
  poe      v0.1: KiCad 7 project (schematic, routed 2-layer board, isolation
           DRC rule) and JLCPCB fab outputs. Not built or ordered.
           Open: the 5 V link to carrier J7 (long-pin header or cable),
           see poe/documentation/open-issues.md.

Prototype orders are planned at carrier v0.6, together with the PoE board.

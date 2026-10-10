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

  carrier  v0.5c designed (generated, not yet in KiCad form here), not
           ordered. KiCad project with an editable schematic: in progress.
  poe      v0.1 requirements settled, see poe/documentation/requirements-v0.1.md.
           schematic not started.

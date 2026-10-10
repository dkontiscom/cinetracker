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

  carrier  v0.60: KiCad 7 project with an editable hierarchical schematic,
           routed board and JLCPCB fab outputs. Not built or ordered.
           v0.60: 0.1 % genlock level resistors, MISO pull-ups,
           connector courtyards (see carrier/documentation/changelog.md).
           Do not build v0.3 to v0.5c: they swapped the two Teensy socket
           rows (see carrier/documentation/changelog.md).
  poe      v0.60 (v0.1 renumbered, same design): KiCad 7 project
           (schematic, routed 2-layer board, isolation DRC rule) and
           JLCPCB fab outputs. Not built or ordered.
           The 5 V link to carrier J7 is a long-pin 1x4 header, cut to
           length once the real stack is measured.

Revisions go v0.60, v0.61, v0.62, ... Before anything is ordered: owner
review, then (with funding) a review by a professional PCB designer.

Licence

  Everything under hardware/ (KiCad sources, libraries, fab outputs,
  documentation) is licensed CERN-OHL-S-2.0, strongly reciprocal, see
  hardware/LICENSE. Copyright CineTracker contributors.
  Source location: https://github.com/dkontiscom/cinetracker
  Boards made from these files carry that source location on their
  bottom silkscreen; keep it on derived boards.
  The firmware (everything outside hardware/) is AGPL-3.0-or-later, see
  the LICENSE at the repository root.

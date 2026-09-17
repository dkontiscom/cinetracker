CineTracker builds on work from other people, credit where its due:

Genlock / tri level sync generation, PTP locked:
based on t41-tri-sync-ptp by IMS-AS-LUH (Leibniz Universitat Hannover,
IMS/AS group)
https://github.com/IMS-AS-LUH/t41-tri-sync-ptp
which itself is based on:
- t41-ptp, https://github.com/IMS-AS-LUH/t41-ptp
- a QNEthernet PTP fork, https://github.com/HedgeHawk/QNEthernet/tree/ieee1588-2-fix
- the paper "PTP-Synchronized Tri-Level Sync Generation for Networked
  Multi-Sensor Systems" by Riggers, Schleusner, Renke and Blume (2024)

AES67 audio / LTC timecode over the network:
based on AES67Teensy by SavilleBelcher
https://github.com/SavilleBelcher/AES67Teensy
which itself is based on:
- a modified QNEthernet, https://github.com/ssilverman/QNEthernet/tree/ieee1588-2
- t41-ptp, https://github.com/IMS-AS-LUH/t41-ptp
- optionally teensy-4-usbAudio, https://github.com/alex6679/teensy-4-usbAudio

FreeD protocol: the FreeD camera tracking protocol spec, and the virtual
production community around it.

24bit I2S on the audio shield: the stock teensy audio library caps at
16bit, so for real 24bit audio out to the sgtl5000 we use
OpenAudio_ArduinoLibrary by Chip Audette
https://github.com/chipaudette/OpenAudio_ArduinoLibrary
which itself extends the original PJRC Teensy Audio Library by
Paul Stoffregen. both MIT (the license text is right in the source file
headers even though the repo has no standalone LICENSE file, which threw
off automatic license detection for a bit until we actually checked the
files directly).

QNEthernet itself (used by both the genlock and AES67 paths above) is
AGPL-3.0-or-later, which is why all of CineTracker ends up AGPL too,
rather than the MIT license t41-tri-sync-ptp and AES67Teensy's own code
uses.

all of the above are used under their own licenses. if youre one of the
people/projects above and something here is wrong or missing, open an
issue.

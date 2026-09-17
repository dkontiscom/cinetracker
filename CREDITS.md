CineTracker builds on work from other people, credit where its due:

PTP time sync + genlock (tri level sync generation), used directly, both
vendored straight into this repo at lib/t41-ptp and lib/t41-tri-sync
rather than pulled in as a remote dependency:
t41-tri-sync-ptp by IMS-AS-LUH (Leibniz Universitat Hannover, IMS/AS
group), commit 4a121127f63f826a1db01534c0b7c4614c448026
https://github.com/IMS-AS-LUH/t41-tri-sync-ptp
MIT license, checked directly in the source files. one shared PTP client
feeds both genlock and aes67 timing. the standalone IMS-AS-LUH/t41-ptp
repo turned out to be an older, stale copy missing the extended
PTPBase::begin() genlock actually needs (see ARCHITECTURE.md for why),
so we vendor the newer copy bundled inside t41-tri-sync-ptp instead.
also where we got the reference tri-level sync output circuit (the
actual pcb build was outsourced separately). also based on the paper
"PTP-Synchronized Tri-Level Sync Generation for Networked Multi-Sensor
Systems" by Riggers, Schleusner, Renke and Blume (2024). needs the
HedgeHawk QNEthernet fork, https://github.com/HedgeHawk/QNEthernet/tree/ieee1588-2-fix

AES67 audio / LTC timecode over the network:
based on AES67Teensy by SavilleBelcher
https://github.com/SavilleBelcher/AES67Teensy
note: their vendored copy of t41-ptp has unresolved merge conflicts
committed into it and wouldnt compile, so we use the clean t41-ptp
above instead of anything from their lib/ folder. not using
teensy-4-usbAudio either, https://github.com/alex6679/teensy-4-usbAudio,
that was one of their optional paths and we want analog i2s not usb

FreeD protocol: the FreeD camera tracking protocol spec, and the virtual
production community around it.

24bit I2S audio: using OpenAudio_ArduinoLibrary by Chip Audette
https://github.com/chipaudette/OpenAudio_ArduinoLibrary
for its 32bit float i2s streaming classes (AudioInputI2S_F32/
AudioOutputI2S_F32/etc), which drive the raw i2s peripheral and dont
care what chip is listening. which itself extends the original PJRC
Teensy Audio Library by Paul Stoffregen. both MIT (the license text is
right in the source file headers even though the repo has no standalone
LICENSE file, which threw off automatic license detection for a bit
until we actually checked the files directly).

audio hardware: dropped the PJRC sgtl5000 audio shield (16bit unless you
override its registers, and its onboard sd card conflicted with the
Focus encoders spi pins). went with a PCM5102A dac + PCM1808 adc pair
instead, bare chips no i2c no register hack needed, 24bit by how the
hardware itself is wired.

QNEthernet itself (the HedgeHawk fork our PTP dependency needs) is
AGPL-3.0-or-later, which is why all of CineTracker ends up AGPL too,
rather than the MIT license t41-ptp, t41-tri-sync-ptp and AES67Teensy's
own code uses.

all of the above are used under their own licenses. if youre one of the
people/projects above and something here is wrong or missing, open an
issue.

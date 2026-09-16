CineTracker builds on work from other people. Credit where it's due:

Genlock / tri-level sync generation, PTP-locked:
based on t41-tri-sync-ptp by IMS-AS-LUH (Leibniz Universitat Hannover, IMS/AS group)
https://github.com/IMS-AS-LUH/t41-tri-sync-ptp
which itself is based on:
- t41-ptp, https://github.com/IMS-AS-LUH/t41-ptp
- a QNEthernet PTP fork, https://github.com/HedgeHawk/QNEthernet/tree/ieee1588-2-fix
- the paper "PTP-Synchronized Tri-Level Sync Generation for Networked Multi-Sensor Systems"
  by Riggers, Schleusner, Renke and Blume (2024)

AES67 audio / LTC timecode over the network:
based on AES67Teensy by SavilleBelcher
https://github.com/SavilleBelcher/AES67Teensy
which itself is based on:
- a modified QNEthernet, https://github.com/ssilverman/QNEthernet/tree/ieee1588-2
- t41-ptp, https://github.com/IMS-AS-LUH/t41-ptp
- optionally teensy-4-usbAudio, https://github.com/alex6679/teensy-4-usbAudio

FreeD protocol: the FreeD camera tracking protocol specification, and the virtual
production community around it.

All of the above are used under their own licenses (MIT where checked so far).
If you're one of the people/projects above and something here is wrong or
missing, open an issue.

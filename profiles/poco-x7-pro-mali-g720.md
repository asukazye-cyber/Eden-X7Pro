# POCO X7 Pro baseline profile

Hardware target: Dimensity 8400-Ultra (`MT6899`) with Mali-G720 MC7 and 12 GB
RAM.

Start comparison runs in Pokémon Sword at the same saved location, after the
device has reached a stable temperature. Record one result per configuration:

| Variant | GPU async emulation | Pipeline workers | Purpose |
| --- | --- | --- | --- |
| A | On | 2 | Lower contention and heat |
| B | On | 3 | Baseline balance |
| C | On | 4 | Higher parallelism test |

Use the platform's stock Mali driver. Do not apply settings intended for
Adreno/Turnip, such as Qualcomm-specific vertex-buffer or bloom workarounds.

V0 does not force these settings into the APK: settings remain user-controlled
until each one has been measured for stability.

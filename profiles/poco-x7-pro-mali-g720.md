# POCO X7 Pro / Mali-G720 profile

Hardware target: Dimensity 8400-Ultra (`MT6899`) with Mali-G720 MC7 and 12 GB
RAM.

The global preset starts a fresh installation with four pipeline workers,
asynchronous GPU emulation and asynchronous shaders enabled. Those settings are user-editable:
if a game has visual issues or crashes, turn off asynchronous GPU emulation
first, then asynchronous shaders, before changing other accuracy settings.

Compare configurations in Pokémon Sword at the same saved location, after the
device has reached a stable temperature. Record one result per configuration:

| Variant | GPU async emulation | Pipeline workers | Purpose |
| --- | --- | --- | --- |
| A | On | 2 | Lower contention and heat |
| B | On | 4 | Global balanced preset |
| C | On | 5 | Higher parallelism test |

Use the platform's stock Mali driver. Do not apply settings intended for
Adreno/Turnip, such as Qualcomm-specific vertex-buffer or bloom workarounds.

V0 did not alter any defaults. This is a starting profile only; it does not
replace the stock Mali driver or apply Qualcomm/Turnip workarounds.

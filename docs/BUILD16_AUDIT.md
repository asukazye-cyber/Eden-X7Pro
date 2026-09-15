# Build #16 audit — starting point

This checklist describes the shipped Build #15 code, not intended behavior. “Wired” means an Android preference reaches executable runtime code.

| Requested feature | Build #15 status | Evidence |
| --- | --- | --- |
| Hardware detection | PARTIALLY IMPLEMENTED | The Vulkan model matcher accepts `Mali-G720 MC7`, but profile activation also requires narrow Android property strings, so the physical target can be rejected. |
| Dimensity 8400 optimization profile | PARTIALLY IMPLEMENTED | Four pipeline workers and affinity hooks exist, but they are gated by the failed profile. |
| Mali-G720 optimization profile | PARTIALLY IMPLEMENTED | Descriptor-ring and native compute paths exist; their main profile gate can reject the target. |
| CPU scheduler | IMPLEMENTED BACKEND ONLY | CPU, present, and worker affinity calls exist but there is no user-controlled policy. |
| ADPF integration | NOT IMPLEMENTED | No Android Performance Hint / ADPF calls exist. |
| Performance Governor | NOT IMPLEMENTED | No setting or runtime governor exists. |
| Vulkan Mali fast paths | PARTIALLY IMPLEMENTED | The descriptor-ring budget and native compute path are present; capability and identity state are not separately reported. |
| Shader/pipeline cache improvements | PARTIALLY IMPLEMENTED | Async shaders and a four-worker default are wired; there is no adaptive cache governor. |
| Frame pacing | PARTIALLY IMPLEMENTED | A display-rate hint exists for native FG; there is no measured 30 FPS pacing policy. |
| Detailed performance telemetry | PARTIALLY IMPLEMENTED | A render-interval timestamp exists, but the displayed `real` counter is overwritten by FG status. |
| Performance overlay | PARTIALLY IMPLEMENTED | A single Mali-FG diagnostic line can be shown; it is not the requested measured overlay. |
| Frame-generation integration | IMPLEMENTED AND WIRED | Native Vulkan compute FG is compiled for this flavor and exposed in the frame-generation page, subject to profile/capability gates. |
| FG Governor | PARTIALLY IMPLEMENTED | A hard-coded late-frame guard exists; there is no UI setting or explicit governor mode. |
| Thermal awareness | NOT IMPLEMENTED | No reliable thermal-status integration exists. |
| Dynamic Resolution | NOT IMPLEMENTED | No runtime resolution controller exists. |
| Pokémon Sword preset | NOT IMPLEMENTED | No title-ID preset exists. |
| Android settings UI | PARTIALLY IMPLEMENTED | Native FG settings are present; the requested Device Optimization controls do not exist. |

Build #16 will only expose settings that have a persisted configuration key and runtime behavior. Dynamic resolution and thermal control remain absent until they can be implemented safely rather than as decorative toggles.

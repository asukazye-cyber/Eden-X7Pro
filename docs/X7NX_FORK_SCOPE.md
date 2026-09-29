# X7NX fork scope

X7NX is a GPLv3 fork derived from Eden. Its target is deliberately narrow:

```text
Android ARM64 -> POCO X7 Pro -> Dimensity 8400-Ultra -> Mali-G720 MC7 -> Vulkan
```

## Reused, not reimplemented

The guest CPU/NCE, HLE, loader, services, filesystem, shader decoding, renderer core, Vulkan
scheduler, caches, Android UI and JNI bridge are mature Eden-derived components. Retaining them is
intentional: Phase 1 changes the ownership and specialization boundary, not emulation semantics.

## Owned by X7NX

* the Gradle root and the single `x7nx` Android product (`org.x7nx.emulator`);
* an Android ARM64 build guard and disabled frame-generation variants;
* runtime Dimensity/Mali capability and all-big-core topology detection;
* X7NX backend initialization, Mali policy, shader compatibility policy, UMA policy and telemetry;
* target-specific game-policy data and replay/benchmark capture contracts.

## Current limitation

The fork is an architectural extraction, not proof of a faster renderer. The active feedback path
remains correctness-first copy/clone until replay measurements validate a Mali tile-local path per
driver. Internal Java package names and filesystem helpers remain inherited implementation details;
the installable Android package, project identity and build target are X7NX.

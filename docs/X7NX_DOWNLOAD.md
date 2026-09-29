# Downloading an X7NX build

The `x7nx-phase1` branch has a GitHub Actions workflow named **Build X7NX Experimental APK**.
Each successful run verifies the independent package ID (`org.x7nx.emulator`) and uploads
`X7NX-Experimental.apk` as an Actions artifact retained for 90 days.

Open the workflow run, then use **Artifacts → X7NX-Experimental-<run number>** to download it.
The APK is experimental, targets Android ARM64, and contains no game content, keys, firmware,
or Frame Generation implementation.

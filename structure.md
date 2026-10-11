# NLSI Exclusive Logbook v1.5.5-beta structure

This document reflects the implemented repository layout. It complements, but
does not replace, the more detailed [development](./docs/DEVELOPMENT.md),
[installation](./docs/INSTALLATION.md), and [backend](./db/README.md) guides.

```text
nlsi_telemetry_poc/
├── db/
│   ├── api/                  # Authenticated PHP API endpoints
│   ├── dashboard/            # Browser dashboard assets
│   ├── lib/                  # PDO, validation, and ingestion services
│   ├── migrations/           # Versioned, additive MySQL migrations
│   ├── logs/                 # Protected server-side diagnostics
│   ├── tests/                # PHP backend tests
│   ├── config.example.php    # Safe configuration template
│   └── run-migrations.php    # Versioned migration runner
├── native/
│   ├── app/                  # Native application entry and resources
│   ├── gui/                  # Qt desktop interface
│   ├── logging/              # Durable local records and sync queue
│   ├── providers/            # TruckSim and SCS provider interfaces
│   ├── resources/            # Windows application resources
│   ├── session/              # Session lifecycle and state
│   ├── sync/                 # HTTPS API client and retry scheduling
│   ├── telemetry/            # Telemetry models and core
│   ├── tests/                # Native unit and integration tests
│   └── updater/              # Update/version support
├── scs_position_plugin/      # SCS telemetry plugin source
├── includes/                 # SDK and packaged third-party plugin inputs
├── installer/                # Inno Setup, packaging, and plugin installers
├── docs/                     # User, developer, schema, and release docs
├── test/                     # Python packaging and installer tests
├── assets/, img/, overlays/  # Existing application UI/media assets
├── build/                    # Generated, version-scoped build artifacts
├── CMakeLists.txt
├── version.json
├── README.md
└── RELEASE_NOTES.md
```

## Synchronization boundaries

- The desktop application keeps the existing `.nlsi` records and durable local
  queue. Synchronization never deletes or rewrites those records.
- The native client sends records over authenticated HTTPS to the PHP API.
  PHP validates and stores full payloads through PDO; browser JavaScript never
  connects directly to MySQL.
- SQL is preferred for records already stored there. Local-only records remain
  pending in the desktop app until a committed API acknowledgement is received.
- Automatic synchronization runs every 15 minutes while the desktop app is
  open. The desktop also provides **Sync Now**; no background service is added.
- XAMPP hosts the PHP API/dashboard. Remote testing requires an active HTTPS
  tunnel such as ngrok and application authentication; tunnel availability is
  not evidence that the database is reachable.

## Plugin installation boundaries

The native installer packages the SCS position plugin from the release-tagged
`build/plugins/<release-tag>/win_x64|win_x86/nlsi.dll` outputs and the
hash-verified TruckSim GPS plugin from
`includes/trucksim-gps-plugin/win_x64|win_x86/plugins/`. Post-install setup
invokes both plugin installers on fresh installation and update. They validate
source architecture, replace outdated game-directory copies, verify installed
hashes, and report failures instead of silently claiming success.

# NLSI Exclusive Logbook 1.7.4 Beta

## What's new

- Add a Settings page with Preference and Profile tabs. Preference contains
  grouped speed, volume, currency, and distance selections with one save action;
  Profile is a styled Coming Soon page.
- Store preferences together in the per-user `profile.nlsi` file. On Windows,
  the file payload is protected with the current user's DPAPI and replaced
  atomically. Existing encrypted profile fields are preserved when updated.
- Add centralized km/h/mph, km/mile, liter/US-gallon/Imperial-gallon formatting.
  Canonical telemetry and stored job values remain unchanged.
- Preserve the source currency denomination for each income or expense. The
  application does not exchange currencies or label game currency as EUR/USD.
- Apply saved measurement preferences to dashboard and completed-job details.
  Add PDF export from completed-job details using the same presentation values.
- Update application, installer, and release output metadata to 1.7.4 Beta.

## Compatibility and verification notes

The current source had no `profile.nlsi` storage or encryption implementation.
The new profile format is versioned and DPAPI-protected; it preserves JSON
fields inside a valid file, but it is not a migration reader for another
undocumented profile format. An unreadable existing file is not overwritten.

Installer upgrades leave per-user application data untouched. Clean-install and
upgrade behavior is covered by the release script's packaging checks and by the
manual scenarios in the installation guide; real-machine install and live-game
testing are not implied unless separately completed.

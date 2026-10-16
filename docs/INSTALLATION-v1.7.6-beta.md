# Installing or updating NLSI Exclusive Logbook 1.7.6 Beta

Run `NLSI-Exclusive-Logbook-v1.7.6-beta-Setup.exe`. Setup keeps the existing
product identity and install location so it can upgrade an existing
installation. Close NLSI Exclusive Logbook and both games before setup.

The installer replaces managed application files, runtime dependencies, and
plugin payloads while preserving the per-user encrypted `profile.nlsi`, local
delivery and job history, logs, and preferences. Existing game plugins are
replaced only when their NLSI ownership marker and file hash are valid.

After installation, open Settings > Config. The default API link is already
filled in. Enter the driver key issued by the NLSI VTC driver portal and save
the configuration. **Test Connection** verifies API availability and, when a
key is configured, verifies that the key is still valid. Open Settings >
Profile to refresh account information from the server.

The key is stored in the encrypted profile and is masked in the interface.
If a key is invalid or revoked, issue or rotate it through the driver portal
and replace it in Settings > Config. An API outage does not disable local
telemetry or saved records.

The installer finish-page option **Follow Nabski Plays on TikTok** is optional
and unchecked by default. Selecting it opens the profile in the system browser
after setup completes.

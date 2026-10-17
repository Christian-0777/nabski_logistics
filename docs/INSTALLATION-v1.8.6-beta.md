# NLSI Exclusive Logbook v1.8.6 Beta Installation

Run `NLSI-Exclusive-Logbook-v1.8.6-beta-Setup.exe` and follow the installer
prompts. The installer supports Windows x64 and includes the x64 dashboard plus
x64 and x86 SCS telemetry plugins.

The installer upgrades existing NLSI Beta installations while leaving local
account configuration, encrypted profile settings, job history, preferences,
and unrelated files intact. Existing game plugins are replaced only when they
are recognized as NLSI-managed and unchanged since installation.

Configure the HTTPS API base URL and driver key under **Settings > Config**.
Select **Test API Connection** to validate the server-issued profile. Username,
display name, driver ID, account status, role, and rank are shown only after a
successful profile response. Role and driver rank are independent server values.

If the API is unavailable, the application retains local telemetry and job
history. Job records remain pending or failed until a valid committed server
acknowledgement is received. An API reachability or job-sync error does not
itself erase a previously verified profile; rejected credentials do.

The product remains a Beta release.

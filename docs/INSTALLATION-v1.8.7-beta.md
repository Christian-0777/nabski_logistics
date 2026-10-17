# NLSI Exclusive Logbook v1.8.7 Beta Installation

Run `NLSI-Exclusive-Logbook-v1.8.7-beta-Setup.exe` and follow the installer
prompts. The installer supports Windows x64 and includes the x64 dashboard plus
x64 and x86 SCS telemetry plugins.

The installer upgrades existing NLSI Beta installations while leaving local
account configuration, encrypted profile settings, job history, preferences,
and unrelated files intact. Existing game plugins are replaced only when they
are recognized as NLSI-managed and unchanged since installation.

Configure the HTTPS API base URL and driver key under **Settings > Config**.
The Profile tab displays the TASK section before RANK. Monthly task progress
currently reports as unavailable because the existing desktop API contract
does not provide monthly task records. Rank continues to use server-confirmed
qualifying distance.

DRIVER, MANAGER, CEO, and linked ADMIN accounts can synchronize eligible local
deliveries owned by their authenticated driver identity. Jobs remain pending
until a committed response confirms the driver and each local/server job ID
pair; transient failures can be retried. Local `.nlsi` records remain available
offline and are preserved during upgrades.

The product remains a Beta release.

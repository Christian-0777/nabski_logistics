# Installing or updating NLSI Exclusive Logbook 1.8.2 Beta

Run `NLSI-Exclusive-Logbook-v1.8.2-beta-Setup.exe` and approve the Windows
administrator prompt. The installer updates the existing NLSI application
directory using the stable Inno Setup application ID and leaves per-user
settings, local jobs, preferences, and event history in place.

The package contains the x64 dashboard, the x64 and x86 SCS plugins, Qt runtime
files, and the required app-local Microsoft runtime. It deploys a game plugin
only into an existing matching ETS2 or ATS architecture directory. Close the
dashboard and games before upgrading; modified or unmarked game plugin files
are preserved.

After installation, start the dashboard and configure the HTTPS API URL and
driver connection key in **Settings > Config**. `LOCAL MODE` is expected until
the server confirms the account identity and role. Local jobs remain available
without a server connection.

## Updating the separate driver portal

The desktop installer does not install website or database files. Deploy the
updated `nlsi_vtc` PHP files separately. Back up the existing database and
apply `nlsi_vtc/database/migrations/009_driver_login_password.sql` after
migrations 005–008. Do not import the fresh-install schema over an existing
database. Drivers activate with their existing single-use invitation and set a
reusable password; active legacy drivers can use a one-time emailed sign-in
code to establish or reset that password.

Do not package or publish `.env` files, database credentials, API keys, private
keys, development build files, or debug artifacts.

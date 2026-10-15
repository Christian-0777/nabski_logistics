# Installing or updating NLSI Exclusive Logbook 1.7.4 Beta

Run `NLSI-Exclusive-Logbook-v1.7.4-beta-Setup.exe`. Setup retains the stable
product identity and detected previous application directory, so install it
over an existing version without uninstalling. Close NLSI Exclusive Logbook
and both games before setup.

The installer deploys the dashboard, Qt runtime, resources, and architecture-
matched telemetry plugins using the existing plugin ownership checks. It does
not install, replace, or delete per-user application data. Settings are stored
together in the user's encrypted `profile.nlsi`; Windows DPAPI protects the
file for the current Windows user. Job history remains in the existing
application-data location. In-place upgrades therefore retain both settings
and historical records.

On first launch with no profile, default preferences (km/h, liters, EUR, and
km) are active in memory. Change all preferences on the Settings > Preference
tab and select **Save Preferences** to save them together in the encrypted
profile. The
Settings > Profile tab is informational and does not change authentication.
Currency display follows the denomination supplied by telemetry or the job
record; preferences do not trigger currency exchange.

## Manual install and upgrade verification plan

Use a disposable Windows VM and test Steam-library folders; do not experiment
with production game plugins or saved data.

1. For a clean install, install with no existing NLSI data and confirm the
   application starts with default units; after selecting **Save Preferences**,
   confirm that it creates an encrypted `profile.nlsi`.
2. Save non-default units, close the application, relaunch it, and confirm the
   selections and dashboard/history presentation remain consistent.
3. For an upgrade, install over a prior NLSI installation with an existing
   encrypted profile and job history. Confirm both remain unchanged through
   setup, then launch and verify saved preferences load.
4. Exercise the existing managed-plugin checks for matching x64/x86 ETS2 and
   ATS plugin installation. Confirm unmarked or modified third-party files
   remain untouched.
5. Export a completed job to PDF and compare its values and units with the
   completed-job details page.

This is a manual verification plan, not a claim that these install scenarios
or live ETS2/ATS sessions have been executed.

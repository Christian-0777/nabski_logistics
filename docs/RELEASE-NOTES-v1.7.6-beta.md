# NLSI Exclusive Logbook 1.7.6 Beta — Getting Online

## What's new

- Add Settings > Config for the HTTPS API link and driver key. Both are stored
  together in the existing Windows DPAPI-protected `profile.nlsi`.
- Add API connection testing and server-refreshed driver profile information
  using the existing NLSI driver-key validation route.
- Preserve unknown encrypted profile fields when saving preferences or API
  settings.
- Update the application, telemetry plugins, and installer to 1.7.6 Beta.
- Add an optional, unchecked TikTok follow checkbox to the installer finish
  page.

The profile view only displays basic account data returned for the authenticated
driver. Invalid credentials and unavailable APIs do not stop local telemetry,
job history, or delivery records from operating.

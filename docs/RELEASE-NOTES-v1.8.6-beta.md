# NLSI Exclusive Logbook v1.8.6 Beta

## Country lookup and dashboard guardrails

- Harden route formatting so origin and destination display values do not
  produce malformed output when the country value is missing, duplicated, or
  unavailable.
- Preserve the visible company and city text when the country cannot be
  resolved, while avoiding strings such as `City, —` or `, Country`.
- Keep the current implementation tied to SCSSDK telemetry only and do not add
  TruckSim GPS or any external geolocation dependency.
- Validate that the application never invents country values; missing data
  remains missing instead of being fabricated.

## Release contents

- Dashboard: Windows x64, file version `1.8.6.0`, product version `1.8.6 Beta`.
- SCS telemetry plugin: Windows x64 and x86, file version `1.8.6.0`, product
  version `1.8.6 Beta`.
- Installer: fresh version-specific staging under `releases\1.8.6-beta`; user
  profile and local job history are not packaged or modified by the installer.

The release build continues to validate the existing automated tests and the
release packaging flow before the final installer is published. This remains a
Beta release.

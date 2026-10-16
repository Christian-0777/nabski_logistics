# NLSI Exclusive Logbook v1.8.4 Beta

## Account and connectivity

- The shared status badge reports AUTHENTICATING while checking the API, ONLINE
  only after a successful driver-key authentication and validated server
  profile, OFFLINE when the API cannot be reached or is unavailable, and LOCAL
  MODE when no verified server account is active.
- The badge tooltip separates transport reachability from account
  authentication. HTTP success alone does not establish an authenticated
  session.
- Username, display name, canonical account role, and server-issued driver
  rank are kept as distinct profile values. Refreshing the profile uses the
  current server response; logout, account failure, or account switch clears
  stale verified identity values.

## Local history and synchronization

- Jobs and local history remain available in LOCAL MODE and OFFLINE.
- A local job is marked synced only after the authenticated server confirms a
  committed write for the same account and acknowledges the matching local job
  ID with its server job ID.
- Invalid JSON, mismatched account IDs, unsuccessful HTTP responses, and
  incomplete acknowledgements do not delete local jobs or claim success.
- The MySQL database remains behind the existing PHP API.

## Release artifacts

- Application and plugin file versions: `1.8.4.0`.
- Application, plugin, and installer product display version: `1.8.4 Beta`
  wherever the resource format supports a string product version.
- The x64 dashboard and x64/x86 plugin DLLs are freshly built and version
  checked before entering the clean installer payload.
- Release artifacts are built by `scripts\build-release.ps1` and staged under
  `releases\1.8.4-beta`. The installer compiler receives the exact staged
  payload path; no generic or prior-version staging directory is used.

This is a Beta release and is not represented as stable or production-ready.

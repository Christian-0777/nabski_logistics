# NLSI Exclusive Logbook v1.8.5 Beta

## Account and synchronization corrections

- Keep profile authentication separate from API transport and job synchronization
  availability. A failed job-list response no longer clears a profile that was
  already verified by the server.
- Clear profile identity when credentials are rejected, the account is inactive,
  the user removes the local API configuration, or credentials are switched.
- Replace a stale "API CONNECTION VERIFIED" footer with the current profile or
  synchronization state.
- Record redacted API failure diagnostics containing configured/request/response
  URLs, method, HTTP status, redirect location, content type, expected versus
  actual response shape, and a safely redacted body excerpt. Request credentials
  and private job/profile fields are not logged.
- Forward the website's `/api/job-sync.php` route to the central `job-sync.php`
  endpoint, matching the website's existing profile-validation proxy.
- Send ngrok's skip-browser-warning request header to ngrok-hosted API URLs so
  API GETs receive JSON instead of the tunnel's HTTP 200 plain-text warning.
- Continue to require strict JSON schemas and committed server acknowledgements
  before marking local jobs as synchronized.

## Release contents

- Dashboard: Windows x64, file version `1.8.5.0`, product version `1.8.5 Beta`.
- SCS telemetry plugin: Windows x64 and x86, file version `1.8.5.0`, product
  version `1.8.5 Beta`.
- Installer: fresh version-specific staging under `releases\1.8.5-beta`; user
  profile and local job history are not packaged or modified by the installer.

The live configured API was probed during this release investigation. Profile
validation returned HTTP 200 `application/json` with the expected authenticated
driver profile schema. The read-only job-list request returned HTTP 200
`application/json` with the expected `driver_id`, `records`, and `pagination`
schema. A website alternate-route probe reproduced an HTML 404 for
`/api/job-sync.php` before the route was added; this release forwards that route.
The initial desktop GET to the configured ngrok host returned HTTP 200
`text/plain` with `ERR_NGROK_6024`; retrying with the ngrok warning-bypass
header returned the expected JSON response.

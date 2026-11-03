# NLSI Exclusive Logbook v1.9.5 Beta Release Notes

## Freight Market trailer information

- Captures trailer identity, brand/name, type, and plate from SCS SDK 1.16 trailer configuration attributes.
- Uses the documented indexed `trailer.connected` telemetry channels to track attachment and detachment.
- Transfers trailer state and job-market information through the existing telemetry IPC snapshot.
- Allows later valid trailer updates to replace the initial job snapshot and retains values during transient unavailable updates.
- Clears active-job trailer details on an explicit detached state and prevents detached values from leaking into the next job.
- Keeps existing `.nlsi` serialization compatible and preserves truck-job telemetry behavior.
- Current Job, completed-job details, and PDF delivery reports continue to use the same trailer fields.

## Validation

- Dashboard and telemetry plugin build with the repository's configured MSVC/Ninja toolchain.
- All seven configured automated tests pass.
- ETS2/ATS Freight Market in-game verification was not performed.

Trailer dimensions are not exposed as direct dimension attributes by SDK 1.16. The SDK does expose trailer wheel count/offsets and hook position in configuration, and cargo/body/chassis/wheel damage channels; these measurements are not presented as trailer dimensions or added to the current job UI.

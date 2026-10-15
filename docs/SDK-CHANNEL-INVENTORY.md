# SDK channel inventory

This inventory was derived directly from the official headers under `scs_sdk_1_15/include/` and the documented channel comments in those files.

The plugin registers fuel, AdBlue (ETS2 only; the ATS header documents it as
unsupported), engine RPM, gear, effective throttle/brake/clutch/steering,
cruise-control speed, retarder level, parking brake, navigation distance, and
navigation ETA. It also registers the SDK configuration and gameplay events,
consumes the documented job, truck, and trailer configuration values listed
below, and displays only the most recent in-memory gameplay event ID.

## Verified game / SDK metadata

- `SCS_TELEMETRY_VERSION_CURRENT` in `scssdk_telemetry.h`
- `SCS_TELEMETRY_EUT2_GAME_VERSION_CURRENT` in `eurotrucks2/scssdk_telemetry_eut2.h`
- `SCS_TELEMETRY_EUT2_GAME_VERSION_1_20` in the same file
- `SCS_TELEMETRY_ATS_GAME_VERSION_CURRENT` in `amtrucks/scssdk_telemetry_ats.h`
- `scs_sdk_init_params_v100_t::game_name`, `game_id`, and `game_version` in `scssdk.h`; `game_version` is the game's telemetry API version, not its retail patch version.
- `SCS_TELEMETRY_CONFIG_truck` in `common/scssdk_telemetry_common_configs.h`
- `SCS_TELEMETRY_CONFIG_trailer` in the same file
- `SCS_TELEMETRY_CONFIG_job` in the same file
- `SCS_TELEMETRY_CONFIG_ATTRIBUTE_*` values such as `license.plate`, `brand`, `name`, `cargo`, `income`, `planned_distance.km`

## Verified telemetry channels for the dashboard

| Channel | Meaning | Value type | Unit | Notes |
| --- | --- | --- | --- | --- |
| `truck.fuel.amount` | Fuel amount | float | liters | Directly declared; ETS2 and ATS |
| `truck.adblue` | AdBlue amount | float | liters | Directly declared; ETS2, explicitly unsupported in ATS header |
| `truck.engine.rpm` | Engine RPM | float | RPM | Directly declared; ETS2 and ATS |
| `truck.engine.gear` | Current gear | s32 | gear count | Directly declared; ETS2 and ATS |
| `truck.displayed.gear` | Dashboard gear | s32 | gear count | Directly declared |
| `truck.speed` | Truck speed | float | m/s | Directly declared |
| `truck.input.steering` | Steering input | float | normalized [-1,1] | Directly declared |
| `truck.input.throttle` | Throttle input | float | normalized [0,1] | Directly declared |
| `truck.input.brake` | Brake input | float | normalized [0,1] | Directly declared |
| `truck.input.clutch` | Clutch input | float | normalized [0,1] | Directly declared |
| `truck.effective.steering` | Effective steering | float | normalized [-1,1] | Directly declared |
| `truck.effective.throttle` | Effective throttle | float | normalized [0,1] | Directly declared |
| `truck.effective.brake` | Effective brake | float | normalized [0,1] | Directly declared |
| `truck.effective.clutch` | Effective clutch | float | normalized [0,1] | Directly declared |
| `truck.cruise_control` | Cruise control target speed | float | m/s | Directly declared |
| `truck.brake.parking` | Parking brake | bool | enabled/disabled | Directly declared |
| `truck.brake.motor` | Engine brake | bool | enabled/disabled | Directly declared |
| `truck.brake.retarder` | Retarder level | u32 | retarder units | Directly declared |
| `truck.engine.enabled` | Engine enabled | bool | enabled/disabled | Directly declared |
| `truck.world.placement` | World position and orientation | dplacement | game-world coordinates | Directly declared |
| `truck.local.velocity.linear` | Positional velocity | fvector | m/s | Directly declared |
| `truck.local.velocity.angular` | Angular velocity | fvector | rotations/s | Directly declared |
| `truck.navigation.distance` | Navigation distance | float | meters | SDK advisor value; direct channel; ETS2 and ATS |
| `truck.navigation.time` | Navigation ETA | float | seconds | SDK advisor ETA; direct channel; ETS2 and ATS. The SDK header does not identify its time-domain as game or real time. The dashboard follows the requested 15:1 rule and treats the advisor ETA seconds as remaining game-time seconds, converting once for display. |
| `truck.navigation.speed.limit` | Speed limit | float | m/s | Directly declared |
| `trailer.connected` | Trailer connection state | bool | connected/disconnected | Directly declared |
| `trailer.wear.body` | Trailer body wear | float | 0..1 | Directly declared |
| `trailer.wear.chassis` | Trailer chassis wear | float | 0..1 | Directly declared |
| `trailer.wear.wheels` | Trailer wheel wear | float | 0..1 | Directly declared |
| `truck.wear.engine` | Engine wear | float | 0..1 | Directly declared |
| `truck.wear.transmission` | Transmission wear | float | 0..1 | Directly declared |
| `truck.wear.cabin` | Cabin wear | float | 0..1 | Directly declared |
| `truck.light.beam.low` | Low beam | bool | enabled/disabled | Directly declared |
| `truck.light.beam.high` | High beam | bool | enabled/disabled | Directly declared |
| `truck.light.brake` | Brake lights | bool | enabled/disabled | Directly declared |
| `truck.light.reverse` | Reverse lights | bool | enabled/disabled | Directly declared |
| `truck.lblinker` | Left blinker | bool | enabled/disabled | Directly declared |
| `truck.rblinker` | Right blinker | bool | enabled/disabled | Directly declared |
| `truck.hazard.warning` | Hazard lights | bool | enabled/disabled | Directly declared |

## Verified configuration attributes

| Name | Meaning | Type | Source |
| --- | --- | --- | --- |
| `brand` | Brand name | string | `SCS_TELEMETRY_CONFIG_ATTRIBUTE_brand` |
| `name` | Vehicle or trailer name | string | `SCS_TELEMETRY_CONFIG_ATTRIBUTE_name` |
| `license.plate` | License plate | string | `SCS_TELEMETRY_CONFIG_ATTRIBUTE_license_plate` |
| `license.plate.country` | Plate country | string | `SCS_TELEMETRY_CONFIG_ATTRIBUTE_license_plate_country` |
| `fuel.capacity` | Fuel capacity | float | `SCS_TELEMETRY_CONFIG_ATTRIBUTE_fuel_capacity` |
| `adblue.capacity` | AdBlue capacity | float | `SCS_TELEMETRY_CONFIG_ATTRIBUTE_adblue_capacity` |
| `income` | Job income | float | `SCS_TELEMETRY_CONFIG_ATTRIBUTE_income` |
| `planned_distance.km` | Planned distance | float | `SCS_TELEMETRY_CONFIG_ATTRIBUTE_planned_distance_km` |
| `cargo` | Cargo name | string | `SCS_TELEMETRY_CONFIG_ATTRIBUTE_cargo` |
| `source_city` | Origin city | string | `SCS_TELEMETRY_CONFIG_ATTRIBUTE_source_city` |
| `destination_city` | Destination city | string | `SCS_TELEMETRY_CONFIG_ATTRIBUTE_destination_city` |

## Values intentionally kept unavailable

The following dashboard fields are intentionally left as unavailable because
the SDK does not provide them through the currently consumed configuration or
channel callbacks. No derived values are displayed:

- Job identifier and job status
- Job progress
- Fuel range
- Average fuel consumption
- GPS coordinates converted to real-world map coordinates
- Inferred delivery status

Job income is the SDK configuration's expected income before penalties and is
displayed with a generic game-currency unit because the game-specific currency
may differ. Planned distance is the raw `planned_distance.km` configuration
value. Navigation distance (meters) and navigation ETA (seconds) are direct
SDK advisor channels; ETA is not recalculated from distance or speed. The SDK
header does not explicitly state whether ETA seconds are game-time or
real-time. The v1.6.9 display applies the user-requested game-time conversion
once; this interpretation is recorded here rather than presented as an
independent guarantee from the SDK.

The configuration callback signature and event delivery ordering were checked
against `scssdk_telemetry_event.h`: configuration registrations must occur
during `scs_telemetry_init` to receive the initial configuration.

## Source files consulted

- `scs_sdk_1_15/include/scssdk_telemetry.h`
- `scs_sdk_1_15/include/scssdk_telemetry_event.h`
- `scs_sdk_1_15/include/common/scssdk_telemetry_common_gameplay_events.h`
- `scs_sdk_1_15/include/common/scssdk_telemetry_common_configs.h`
- `scs_sdk_1_15/include/common/scssdk_telemetry_truck_common_channels.h`
- `scs_sdk_1_15/include/common/scssdk_telemetry_trailer_common_channels.h`
- `scs_sdk_1_15/include/eurotrucks2/scssdk_telemetry_eut2.h`
- `scs_sdk_1_15/include/amtrucks/scssdk_telemetry_ats.h`

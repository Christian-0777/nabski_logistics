# Dashboard Layout --- Truck Simulator Telemetry

A Markdown wireframe based on the hand-drawn dashboard layout.

``` text
┌──────────────────────────────────────────────────────────────────────────┐
│ DASHBOARD                                                                │
├──────────────────────────────────────────────────────────────────────────┤
│ CURRENT JOB: [JOB ID] — [STATUS]                                         │
│                                                                          │
│  CARGO: __________________________       INCOME: ______________________   │
│  FROM:  __________________________       TO:     ______________________   │
│                                                                          │
│  PLANNED DISTANCE: _______________       REMAINING: __________________   │
│  PROGRESS: 00.00%                       ARRIVAL: ______________________   │
├──────────────────────────────────────────────────────────────────────────┤
│ DRIVING                                                                  │
│                                                                          │
│  FUEL: __________________________       RPM / GEAR: __________________   │
│  THROTTLE: ______________________       BRAKE: ________________________   │
│  CRUISE CONTROL: ________________       RETARDER: _____________________   │
├──────────────────────────────────────────────────────────────────────────┤
│ GAME CONFIG | EURO TRUCK SIMULATOR 2                                      │
│                                                                          │
│  GAME API VERSION: ______________                                          │
│  VEHICLE: _______________________       VEHICLE PLATE: ________________   │
│  TRAILER: _______________________       TRAILER PLATE: ________________   │
├──────────────────────────────────────────────────────────────────────────┤
│ SUMMARY                                                                  │
│                                                                          │
│  [Job / trip summary, events, or telemetry notes appear here.]           │
│                                                                          │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

## Section and field reference

### 1. Current Job

  Field              Description
  ------------------ ---------------------------------------------------------
  Job ID             Identifier for the active delivery
  Status             Current job state, such as active, paused, or completed
  Cargo              Freight being transported
  From / To          Origin and destination
  Income             Job payment or estimated reward
  Planned Distance   Total planned route distance
  Remaining          Distance left to the destination
  Progress           Percentage text
  Arrival            Remaining time - ETA

### 2. Driving

  Field            Description
  ---------------- -------------------------------------------
  Fuel             Current fuel amount / capacity
  RPM / Gear       Engine RPM and selected transmission gear
  Throttle         %
  Brake            %
  Cruise Control   Cruise-control state or target speed
  Retarder         Retarder state or level

### 3. Game Config

  Field           Description
  --------------- ----------------------------
  Game API Version SDK API version for the active game
  Vehicle         Current truck or vehicle
  Vehicle Plate   Vehicle registration plate
  Trailers        Attached trailer(s)
  Trailer Plate   Trailer registration plate

### 4. Summary

Use this area for a short trip recap, notable telemetry events,
warnings, or a compact log summary.

## Suggested UI behavior

-   Keep the four sections in the same vertical order as the sketch.
-   Use two columns for field/value pairs inside **Current Job**,
    **Driving**, and **Game Config**.
-   Display **Progress** as the existing percentage text.
-   Allow **Summary** to expand or scroll when the recorded text becomes
    long.
-   Show unavailable telemetry values as `—` rather than leaving
    confusing blank fields.

## Completed Jobs

The sidebar's **COMPLETED JOBS** navigation item opens a scrollable list of
confirmed deliveries and cancellations. Cards are sorted newest first, show
the application-generated job ID prominently, and distinguish `DELIVERED`
from `CANCELLED`. Selecting a card opens its full detail view:

1. Job details: cargo/income; origin and destination city/country; origin and
   destination companies; planned and driven distance; XP and damage; real and
   in-game duration.
2. Vehicle and trip details: truck and plate; trailer and plate; fuel cost and
   refueled amount; average and total fuel consumption; maximum and average
   speed.
3. Special-cargo information, only when present in telemetry, and the final
   job status.

Each value uses the actual SCS telemetry/configuration/event value or an em
dash when unavailable. Finalized `.nlsi` records are loaded on application
startup and newly confirmed terminal events refresh the list immediately.
Active and interrupted jobs are retained separately and never shown as
completed.

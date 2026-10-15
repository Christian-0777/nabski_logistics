# NLSI Exclusive Logbook 1.7.1 Beta

## What's new

- Refined the existing sidebar navigation with full-width active, hover,
  pressed, and keyboard-focus states while preserving the current pages and
  navigation behavior.
- Restyled the existing main dashboard vertical scrollbar with a slim
  branded track and rounded normal, hover, and dragging states. Horizontal
  scrolling remains disabled.
- Updated application and installer version metadata to 1.7.1 Beta.

## Compatibility and data

The dashboard content, telemetry behavior, completed-job history, and user
data locations are unchanged. The installer retains update compatibility with
recognized NLSI-owned plugins from previous releases, preserves user settings
and job records, and does not overwrite or remove third-party game plugins.

## Validation

The release build compiles the x64 dashboard and plugin, separately compiles
the x86 plugin, runs the automated test suites, checks plugin architectures
and required SCS exports, stages and validates runtime dependencies, and
builds the Windows installer.

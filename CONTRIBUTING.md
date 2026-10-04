# Contributing

These conventions apply to application code, tests, and supporting scripts.

## Formatting and naming

Use C89 and follow OpenBSD C conventions. Indent with tabs displayed eight
columns wide. Keep lines within 80 columns. Prefer early returns, group
declarations by type, and initialize pointers. Use `scripts/format` through
the Makefile targets to apply the project's C formatting rules.

Choose names that describe a function's action or a module's responsibility.
Follow the surrounding code's naming conventions. Comments should explain
constraints, ownership, or reasons that are not clear from the code.

## Structure and ownership

Give each module a clear responsibility. Keep state and cleanup with their
owner, keep implementation details private, and document resource ownership
beside the relevant APIs. Put reusable helpers in `src/shared`. Keep helpers
specific to one feature with that feature.

Preserve the window manager's boundaries between client lifecycle, metadata,
focus and stacking, and geometry and visibility. Only `window-state.c`
changes live placement fields. Callers use its snapshot and transition APIs.
Frame drag state, tracking of double clicks, and move/resize cursors remain
private to `window-frame.c`.

Keep X11 event dispatch in `events.c` and protocol requests and shared
property publishing in `protocol.c`. `main.c` owns startup, shutdown, and
the event loop, including motion coalescing and timers.

Keep parsing and persistence separate from live behavior. Configuration and
action parsing must work without a running WM. Session file parsing and
serialization operate on owned records. Session restoration coordinates
their use. Desktop icon placement files remain separate from icon rendering
and input handling. Settings readers own and release their line buffers.

Load shared UI settings before rendering. Images own their pixmaps, masks,
and allocated colors. Palettes own their colors, while borrowed drawing
contexts leave those colors with their owner. The run dialog owns its input
method and clipboard resources in `run/x11-input.c`. Taskbar teardown owns
its widgets and tooltip. Callers use the tray coordinator's public API.
Keep Unicode character boundary navigation in the shared `utf8.c` helpers.

## Tests

Run `make test` to build and run the unit and integration tests.

Apply the same care to tests as to application code. Name scenarios clearly,
share repetitive setup, and keep assertions beside the actions they check.
Extract helpers around meaningful operations or checks, keeping each
scenario's behavior easy to follow.

Group integration tests by feature in `tests/integration/`. Keep process and
file handling in `harness.c`, X11 queries and input helpers in `x11.c`, and
test client creation in `fixture.c`.

Each integration case runs in its own process with fresh configuration,
session, and output directories. Keep cases independent of execution order.

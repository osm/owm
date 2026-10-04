# Repository instructions

Read and follow [CONTRIBUTING.md](CONTRIBUTING.md) before changing code,
tests, or scripts.

## Build and validation

Run commands from the repository root. `make` builds the applications.
The root `Makefile` defines application builds, installation, formatting,
and tests.

For code changes, run `make format`, `make format-check`, and `make test`
before finishing. Formatting requires Python 3 and `clang-format`. Integration
tests use Xvfb, libXtst, and xterm. Coverage for st and SDL2 is optional.
For changes limited to documentation, check links and `git diff --check`.

After building the tests, use these commands for focused validation:

* `./tests/run.sh --list` lists integration cases.
* `./tests/run.sh --case run-unicode` runs one case.
* `./tests/run.sh --reverse` checks independence from execution order.
* `./tests/run.sh --keep-output` retains case logs and screenshots.

Options can be combined, for example:
`./tests/run.sh --case run-unicode --keep-output`.
Failed runs always retain their logs and screenshots.

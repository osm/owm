#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

# Exercise the applications built in this checkout.
export PATH="$PWD:$PATH"

usage() {
	cat <<EOF
Usage: $0 [--case NAME] [--reverse] [--keep-output]
       $0 --list

  --case NAME    Run one integration case.
  --reverse      Run cases in reverse order.
  --keep-output  Retain logs and screenshots.
  --list         List cases without starting an X server.
  --help         Show this help.
EOF
}

test_case=${OWM_TEST_CASE:-}
keep_output=${KEEP_TEST_OUTPUT:-0}
reverse=0
list=0
while [ "$#" -gt 0 ]; do
	case $1 in
		--case)
			if [ "$#" -lt 2 ] || [ -z "$2" ]; then
				echo '--case requires a name' >&2
				exit 1
			fi
			case $2 in
				--*) echo '--case requires a name' >&2; exit 1 ;;
			esac
			test_case=$2
			shift
			;;
		--reverse) reverse=1 ;;
		--keep-output) keep_output=1 ;;
		--list) list=1 ;;
		--help) usage; exit 0 ;;
		*) echo "Unknown option: $1" >&2; usage >&2; exit 1 ;;
	esac
	shift
done

run_cases() {
	set --
	if [ -n "$test_case" ]; then
		set -- "$@" --case "$test_case"
	fi
	if [ "$reverse" = 1 ]; then
		set -- "$@" --reverse
	fi
	if [ "$list" = 1 ]; then
		set -- "$@" --list
	fi
	./build/tests/integration "$@"
}

if [ "$list" = 1 ]; then
	run_cases
	exit 0
fi

case ${XSERVER:-xvfb} in
	xvfb) server=${XVFB:-Xvfb}; set -- -screen 0 1280x800x24 ;;
	xephyr) server=${XEPHYR:-Xephyr}; set -- -screen 1280x800 ;;
	*) echo 'XSERVER must be xvfb or xephyr' >&2; exit 1 ;;
esac
if ! command -v "$server" >/dev/null 2>&1; then
	echo "Tests require $server." >&2
	echo "Debian/Ubuntu: apt install xvfb xserver-xephyr libxtst6 xterm" >&2
	exit 1
fi
test_dir=$(mktemp -d /tmp/owm-check.XXXXXX)
server_pid=
cleanup() {
	status=$?
	trap - EXIT HUP INT TERM
	if [ -n "$server_pid" ]; then
		kill "$server_pid" 2>/dev/null || true
		wait "$server_pid" 2>/dev/null || true
	fi
	if [ "$keep_output" = 1 ] || [ "$status" -ne 0 ]; then
		echo "Test output: $test_dir"
	else
		rm -rf "$test_dir"
	fi
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM HUP
"$server" -displayfd 3 "$@" -nolisten tcp \
	3>"$test_dir/display" >"$test_dir/xserver.log" 2>&1 &
server_pid=$!
i=0
while [ ! -s "$test_dir/display" ]; do
	if ! kill -0 "$server_pid" 2>/dev/null || [ "$i" -ge 100 ]; then
		cat "$test_dir/xserver.log" >&2
		exit 1
	fi
	sleep 0.05
	i=$((i + 1))
done
DISPLAY=:$(cat "$test_dir/display")
export DISPLAY
export XDG_CONFIG_HOME="$test_dir/config"
export OWM_TEST_DISPLAY=1
export OWM_TEST_OUTPUT="$test_dir"
export OWM_POWER_SUPPLY_PATH="$test_dir/power_supply"
mkdir -p "$OWM_POWER_SUPPLY_PATH/LaptopCell" "$XDG_CONFIG_HOME/owm"
printf 'Battery\n' >"$OWM_POWER_SUPPLY_PATH/LaptopCell/type"
printf '83\n' >"$OWM_POWER_SUPPLY_PATH/LaptopCell/capacity"
printf 'Charging\n' >"$OWM_POWER_SUPPLY_PATH/LaptopCell/status"
run_cases

# Rendering errors can otherwise leave functional tests green.
if grep -RE 'owm: X11 error:|X Error of failed request:' \
	"$test_dir" --include=wm.log; then
	echo 'FAIL: unexpected X11 rendering errors in wm.log' >&2
	exit 1
fi

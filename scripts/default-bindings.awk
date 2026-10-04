# Config strings use the same escapes as C strings.
/^[[:space:]]*bind[[:space:]]*=/ {
	sub(/^[^=]*=[[:space:]]*/, "")
	if (!match($0, /^"([^"\\]|\\.)*"/)) {
		print FILENAME ":" FNR ": expected a quoted binding" > "/dev/stderr"
		exit 1
	}
	print substr($0, 1, RLENGTH) ","
}

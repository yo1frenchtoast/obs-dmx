#!/usr/bin/env bash
# Checks that every declared Qt slot is actually connected to a signal.
#
# A slot nothing connects compiles without the slightest warning: the button
# exists, it is visible, and it does nothing. That is exactly the kind of fault
# that only shows up in use.
set -uo pipefail
cd "$(dirname "$0")/.."

status=0
for header in $(ls src/*/*.h | sort -u); do
  source_file="${header%.h}.cpp"
  [ -f "$source_file" ] || continue

  # Names declared after "private slots:" or "public slots:", up to the next
  # access label.
  slots=$(awk '
    /(private|public|protected) slots:/ { in_slots = 1; next }
    /^[[:space:]]*(private|public|protected):/ { in_slots = 0 }
    in_slots && match($0, /[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\(/) {
      name = substr($0, RSTART, RLENGTH - 1)
      gsub(/[[:space:]]/, "", name)
      print name
    }' "$header")

  for slot in $slots; do
    if ! grep -q "connect(.*&[A-Za-z]*::$slot\b" "$source_file" &&
       ! grep -qE "connect\(.*\{[^}]*$slot\(" "$source_file"; then
      echo "$source_file: slot '$slot' is connected to no signal"
      status=1
    fi
  done
done

# The other half of the same problem: a signal nobody listens to.
for header in $(ls src/*/*.h | sort -u); do
  signals=$(awk '
    /^[[:space:]]*signals:/ { in_signals = 1; next }
    /^[[:space:]]*(private|public|protected)( slots)?:/ { in_signals = 0 }
    in_signals && match($0, /[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\(/) {
      name = substr($0, RSTART, RLENGTH - 1)
      gsub(/[[:space:]]/, "", name)
      print name
    }' "$header")

  for signal in $signals; do
    if ! grep -rq "connect(.*::$signal\b" src/; then
      echo "$header: signal '$signal' has no receiver"
      status=1
    fi
  done
done

[ "$status" -eq 0 ] && echo "Every Qt slot and signal is connected."
exit "$status"

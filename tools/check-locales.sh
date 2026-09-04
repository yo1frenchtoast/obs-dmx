#!/usr/bin/env bash
# Checks that every interface string has a translation, in both languages.
#
# A missing key shows up verbatim in OBS: the fault is visible but silent, hence
# this check.
set -euo pipefail
cd "$(dirname "$0")/.."

used=$(mktemp) en=$(mktemp) fr=$(mktemp)
trap 'rm -f "$used" "$en" "$fr"' EXIT

grep -rhoE 'obs_module_text\("[^"]+"\)|tr_\("[^"]+"\)' src/ \
  | grep -oE '"[^"]+"' | tr -d '"' | sort -u > "$used"
grep -oE '^[A-Za-z0-9_.]+=' data/locale/en-US.ini | tr -d '=' | sort -u > "$en"
grep -oE '^[A-Za-z0-9_.]+=' data/locale/fr-FR.ini | tr -d '=' | sort -u > "$fr"

status=0
if missing=$(comm -23 "$used" "$en") && [ -n "$missing" ]; then
  echo "Keys used in the code but missing from en-US.ini:"
  echo "$missing" | sed 's/^/  /'
  status=1
fi

# A key may be referenced indirectly, through a struct field rather than a
# literal call, so look for it as a plain string before calling it unused.
orphans=""
for key in $(comm -13 "$used" "$en"); do
  grep -rqF "\"$key\"" src/ || orphans="$orphans $key"
done
if [ -n "$orphans" ]; then
  echo "Translated keys found nowhere in the code:"
  for key in $orphans; do echo "  $key"; done
  status=1
fi

if ! diff -q "$en" "$fr" >/dev/null; then
  echo "en-US.ini and fr-FR.ini do not carry the same keys:"
  diff "$en" "$fr" | sed 's/^/  /'
  status=1
fi

[ "$status" -eq 0 ] && echo "Translations complete: $(wc -l < "$en") keys."
exit "$status"

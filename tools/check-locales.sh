#!/usr/bin/env bash
# Verifie que chaque chaine d'interface a sa traduction, dans les deux langues.
# Une clef manquante s'affiche telle quelle dans OBS : le defaut est visible
# mais silencieux, d'ou ce controle.
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
  echo "Clefs utilisees mais absentes de en-US.ini :"; echo "$missing" | sed 's/^/  /'; status=1
fi
# Une clef peut etre citee indirectement, par un champ de structure plutot que
# dans un appel litteral : on la cherche alors comme simple chaine avant de la
# declarer inutilisee.
orphans=""
for key in $(comm -13 "$used" "$en"); do
  grep -rqF "\"$key\"" src/ || orphans="$orphans $key"
done
if [ -n "$orphans" ]; then
  echo "Clefs traduites mais introuvables dans le code :"
  for key in $orphans; do echo "  $key"; done
  status=1
fi
if ! diff -q "$en" "$fr" >/dev/null; then
  echo "en-US.ini et fr-FR.ini n'ont pas les memes clefs :"; diff "$en" "$fr" | sed 's/^/  /'; status=1
fi

[ "$status" -eq 0 ] && echo "Traductions completes : $(wc -l < "$en") clefs."
exit "$status"

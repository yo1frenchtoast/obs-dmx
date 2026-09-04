#!/usr/bin/env bash
# Verifie que chaque slot Qt declare est effectivement relie a un signal.
#
# Un slot que rien ne connecte compile sans le moindre avertissement : le
# bouton existe, il est visible, et il ne fait rien. C'est exactement le
# genre de defaut qui n'apparait qu'a l'usage.
set -uo pipefail
cd "$(dirname "$0")/.."

status=0
for header in $(ls src/*/*.h | sort -u); do
  source_file="${header%.h}.cpp"
  [ -f "$source_file" ] || continue

  # Les noms declares apres "private slots:" ou "public slots:", jusqu'a la
  # prochaine etiquette d'acces.
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
      echo "$source_file : le slot '$slot' n'est relie a aucun signal"
      status=1
    fi
  done
done

# L'autre moitie du meme probleme : un signal que personne n'ecoute.
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
      echo "$header : le signal '$signal' n'a aucun destinataire"
      status=1
    fi
  done
done

[ "$status" -eq 0 ] && echo "Tous les slots et signaux Qt sont relies."
exit "$status"

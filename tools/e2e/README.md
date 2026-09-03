# Tests de bout en bout

Ces scripts verifient ce que les tests unitaires ne peuvent pas atteindre : que
changer de scene dans un vrai OBS fait vraiment sortir les bonnes valeurs DMX
sur le reseau.

Le principe : on installe une collection de scenes jetable contenant deux
scenes et deux programmes, on lance OBS, on lui fait changer de scene par
obs-websocket, et on decode les trames Art-Net recues sur la loopback.

## Lancer

    ./run_e2e.sh

Le script exige qu'obs-websocket soit active et connait le mot de passe en le
lisant dans la configuration d'OBS.

## Attention

`run_e2e.sh` modifie la configuration d'OBS : il active obs-websocket, ecrit une
collection de scenes nommee `obs-dmx-test`, et pointe la sortie DMX vers
127.0.0.1. Sauvegardez `user.ini`, `global.ini` et
`plugin_config/obs-websocket/config.json` avant de l'utiliser sur une machine
dont la configuration compte.

Il efface aussi `.sentinel/run_*` avant chaque lancement : sans cela, l'arret
force d'OBS a la fin du test precedent fait apparaitre le dialogue de mode
securise, qui bloque le demarrage suivant.

## Les autres scripts

- `artnet_listener.py <secondes>` : decode les trames ArtDMX sur 127.0.0.1:6454
  et verifie la cadence et le sequencement.
- `sacn_listener.py <univers> <secondes>` : idem pour sACN, en multicast.

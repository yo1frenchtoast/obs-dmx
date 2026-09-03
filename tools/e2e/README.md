# Tests de bout en bout

Ces scripts verifient ce que les tests unitaires ne peuvent pas atteindre : que
changer de scene dans un vrai OBS fait vraiment sortir les bonnes valeurs DMX
sur le reseau.

Le principe : on installe une collection de scenes jetable, on lance OBS, on lui
fait changer de scene par obs-websocket, et on decode les trames Art-Net recues
sur la loopback.

Sept scenes couvrent les cinq etapes du projet :

| Scene         | Ce qu'elle verifie                                        |
|---------------|-----------------------------------------------------------|
| Plateau       | une ambiance blanche, temperature de couleur et intensite  |
| Interview     | une ambiance coloree, teinte et saturation                 |
| Chaser        | le motif se decale bien le long des projecteurs            |
| Strobe        | le canal de strobe materiel, sur un fond colore preserve   |
| EffetIntegre  | le mode FX du T4c : selection de l'effet et vitesse        |
| Musique       | une source sonore allume la lumiere                        |
| Silence       | le meme programme sans son la laisse eteinte               |

Les deux dernieres forment un temoin : sans la scene Silence, rien ne
prouverait que c'est bien le son qui allume la lumiere.

## Lancer

    ./run_e2e.sh

## Attention

Le script modifie la configuration d'OBS : il active obs-websocket, ecrit une
collection de scenes nommee `obs-dmx-test`, depose un son de test dans le
dossier de configuration, et pointe la sortie DMX vers 127.0.0.1. Sauvegardez
`user.ini`, `global.ini` et `plugin_config/obs-websocket/config.json` avant de
l'utiliser sur une machine dont la configuration compte.

Deux pieges rencontres en chemin, qui expliquent des lignes du script :

- il efface `.sentinel/run_*` avant chaque lancement, sinon l'arret force
  d'OBS a la fin du test precedent fait apparaitre le dialogue de mode securise,
  qui bloque le demarrage ;
- le son de test est ecrit sous `$HOME` et non dans `/tmp` : le bac a sable
  d'OBS a son propre `/tmp` et n'y trouverait rien.

## Les autres scripts

- `artnet_listener.py <secondes>` : decode les trames ArtDMX sur 127.0.0.1:6454
  et verifie la cadence et le sequencement.
- `sacn_listener.py <univers> <secondes>` : idem pour sACN, en multicast.
- `make_tone.py <fichier.wav>` : genere la basse de test.

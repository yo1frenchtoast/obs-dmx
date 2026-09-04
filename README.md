# Éclairage DMX pour OBS

Un plugin OBS qui associe un programme lumineux à chaque scène : changer de
scène change la lumière. Ambiances fixes, chasers, strobes, effets qui suivent
le son.

Tout se règle à la souris dans un panneau intégré à OBS. Il n'y a pas de
fichier de configuration à écrire.

## Ce dont vous avez besoin

- **OBS Studio 32** ou plus récent.
- **Une interface DMX** entre l'ordinateur et vos projecteurs. Trois familles
  sont gérées :
  - un boîtier **Art-Net** ou **sACN** sur le réseau, qui ressort du DMX ;
  - une interface **Enttec DMX USB Pro** (ou un clone parlant le même
    protocole) branchée en USB.

  Sous Linux, l'accès à une interface USB demande d'appartenir au groupe
  `dialout` :

      sudo usermod -aG dialout $USER

  puis de se déconnecter et se reconnecter.

- **Si vos projecteurs sont des Aputure amaran T2c ou T4c** : l'adaptateur
  *amaran USB-C vers DMX*, vendu séparément, un par lampe. Ces tubes n'ont pas
  d'entrée DMX directe.

## Construire et installer

Le plugin se construit comme une extension Flatpak d'OBS. Il n'y a rien à
installer sur le système hôte.

    flatpak install --user flathub org.flatpak.Builder
    flatpak run org.flatpak.Builder --force-clean --user --install \
        build-dir flatpak/com.obsproject.Studio.Plugin.ObsDmx.yaml

Relancez OBS. Le panneau apparaît dans le menu **Docks → Éclairage DMX**.

Le plugin doit être reconstruit à chaque version majeure d'OBS : un plugin natif
est lié à la version avec laquelle il a été compilé.

## Prise en main

### 1. Déclarez vos projecteurs

Onglet **Projecteurs → Ajouter un projecteur**. Choisissez le modèle, indiquez
son **mode DMX**, donnez l'adresse de départ. Le plugin propose la première
adresse libre.

> **Le mode DMX se choisit sur l'écran du projecteur**, pas depuis le logiciel.
> Le mode indiqué ici doit correspondre à celui réglé sur l'appareil. Rien ne
> permet de le vérifier : si les deux ne correspondent pas, le projecteur se
> comportera bizarrement sans aucun message d'erreur. C'est de loin la cause la
> plus fréquente de « ça ne marche pas ».

Le tableau signale les chevauchements d'adresses, en nommant les deux appareils
concernés.

### 2. Vérifiez le câblage

Onglet **Sortie**. Choisissez le protocole, indiquez l'adresse du boîtier ou le
port série, cochez **Émettre le DMX**.

Le **banc d'essai** en bas de l'onglet envoie une valeur sur un canal de votre
choix. C'est la façon la plus rapide de répondre à la vraie question : est-ce
que mon projecteur est bien à l'adresse que je crois ?

### 3. Construisez vos programmes

Onglet **Programmes → Nouveau**. Cochez les projecteurs concernés, sélectionnez-
les, réglez la lumière. Le réglage s'applique à toute la sélection d'un coup.

Tant que cet onglet est ouvert, le programme en cours d'édition prend la main
sur la sortie : vous voyez ce que vous réglez. **En quittant l'onglet, c'est la
scène OBS active qui reprend la main** — si elle n'est associée à aucun
programme, la lumière s'éteint. Un avertissement en haut de l'onglet le signale,
avec un bouton pour associer le programme à la scène courante en un clic.

### 4. Associez les scènes OBS

En bas de l'onglet Programmes, un tableau liste vos scènes OBS avec, en face de
chacune, le programme à déclencher et la durée du fondu. C'est l'écran central.

Une scène laissée sans programme éteint la lumière.

## Les effets

Un programme peut porter des effets, qui se superposent à sa lumière de base.
Décocher un effet l'éteint sans le perdre.

- **Chaser** — une suite de couleurs qui défile le long des projecteurs, dans
  l'ordre où ils sont listés. Durée par pas ou tempo, fondu entre les pas, sens
  avant, arrière, aller-retour ou aléatoire.
- **Strobe** — fréquence, durée de l'éclat. Les projecteurs dotés d'un canal de
  strobe s'en chargent eux-mêmes, ce qui est bien plus net. Les autres sont
  clignotés par le plugin, ce qui devient irrégulier au-delà d'une dizaine
  d'éclats par seconde : c'est une limite du DMX, pas de votre matériel.
- **Réagit au son** — l'intensité suit le niveau, la couleur suit les
  fréquences, ou un éclat tombe sur chaque temps. L'analyse écoute le mix audio
  d'OBS, donc exactement ce que le public entend. Un afficheur de niveaux montre
  en direct ce que le plugin entend, pour régler la sensibilité en voyant.

  Attention : le mix comprend vos périphériques audio globaux, micro compris, et
  ce quelle que soit la scène affichée. Si la lumière frémit dans le silence,
  c'est le bruit de la salle qui passe par le micro : montez le **seuil**.
- **Effet intégré au projecteur** — pour les appareils qui en proposent, comme
  le mode FX des amaran T4c : orage, feu, téléviseur, gyrophare, feu
  d'artifice… Ils sont joués par la lampe elle-même, donc bien plus fins que ce
  qui pourrait être envoyé sur le réseau. En contrepartie, la lampe doit être en
  mode effets sur son écran, et ne peut alors plus afficher de couleur simple.

  **Si le plugin ne connaît pas les effets de votre projecteur**, cochez
  *Saisir les canaux moi-même* : vous recopiez alors la table de canaux de sa
  notice, ligne par ligne. Le canal 1 est le premier canal du projecteur, tel
  que numéroté dans sa documentation — ce n'est pas l'adresse DMX, et les
  valeurs suivent l'appareil si vous le réadressez. Un canal qui dépasse le
  nombre de canaux de l'appareil n'est pas émis, et l'interface le signale :
  y écrire piloterait le projecteur suivant.

  Cette saisie directe ne sert pas qu'aux effets. C'est aussi le moyen de
  piloter n'importe quel canal qu'aucun réglage de l'interface n'expose : une
  roue de gobos, un moteur de rotation, un réglage propre à votre modèle.

Par défaut, quand plusieurs choses pilotent le même projecteur, la plus
lumineuse l'emporte. C'est ce qui permet à un strobe d'éclater sur un fond
coloré sans l'effacer entre deux éclats. Le repli **Avancé** permet de choisir
le remplacement à la place.

## Raccourci clavier

Un **blackout** est proposé dans les raccourcis d'OBS
(*Paramètres → Raccourcis clavier*). Il coupe et rallume tout d'un même geste.

## Où sont rangés les réglages

- Le montage lumière (projecteurs, programmes, associations) vit dans la
  **collection de scènes** d'OBS. Il suit donc votre projet, et se sauvegarde
  avec lui.
- Les réglages de sortie (protocole, adresse du boîtier, port série) dépendent
  de la machine et vivent à part, dans la configuration du plugin.

## Développement

Le moteur (`src/core`) ne dépend ni de libobs ni de Qt, ce qui le rend testable
hors d'OBS :

    flatpak run --devel --filesystem="$PWD" --command=bash com.obsproject.Studio -c '
      g++ -std=c++20 -O2 -I src -I tests -I/app/include -pthread \
        -DOBS_DMX_FIXTURES_DIR=\"$PWD/data/fixtures\" -o /tmp/tests \
        tests/*.cpp src/core/*.cpp src/output/*.cpp && /tmp/tests'

`tools/check-locales.sh` vérifie que chaque texte d'interface a sa traduction
dans les deux langues. `tools/e2e/` contient un test de bout en bout qui pilote
un vrai OBS et décode le DMX qui en sort ; lisez son README avant de le lancer,
il modifie la configuration d'OBS.

### Ajouter un modèle de projecteur

Déposez un fichier JSON dans `data/fixtures/`. Chaque mode y décrit ses canaux
par leur rôle — `dimmer`, `red`, `hue`, `cct`, `strobe`… — plutôt que par un
numéro. C'est ce typage qui permet à l'interface de proposer une roue de couleur
et au moteur de traduire une couleur voulue vers ce dont l'appareil dispose.

Les plages ne sont pas toujours 0-255 et se décrivent dans le profil :
`range_min`, `range_max`, `off`, `neutral`, `physical_min`, `physical_max`.
Voyez `aputure-amaran-t4c.json`, dont le canal de strobe est éteint de 0 à 19
puis couvre 1 à 25 Hz de 20 à 255, et dont le neutre vert/magenta est à 132.

## Licence

GPL-2.0-or-later, comme l'exige toute liaison à libobs.

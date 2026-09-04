#pragma once

#include "core/color.h"
#include "core/fixture-profile.h"

#include <cstdint>
#include <string>
#include <vector>

namespace obsdmx {

enum class EffectType : uint8_t {
	Chaser,
	Strobe,
	Sound,
	BuiltinFx,
};

/// Comment un effet se combine a ce qui est deja en place.
enum class BlendMode : uint8_t {
	/// L'effet remplace la base sur les projecteurs qu'il vise.
	Replace,
	/// Le plus fort l'emporte, comme sur une console. Un strobe se superpose
	/// ainsi a un fond colore sans l'effacer entre deux eclats.
	Htp,
};

enum class ChaserDirection : uint8_t {
	Forward,
	Backward,
	PingPong,
	Random,
};

/// Le chaser fait tourner une suite de couleurs sur les projecteurs vises,
/// dans l'ordre ou ils sont listes. C'est le modele des jeux de lumiere
/// classiques, et il se regle avec deux nombres au lieu d'un tableau.
struct ChaserSettings {
	std::vector<LightState> steps;
	/// Duree d'un pas. Ignoree quand la synchro BPM est active.
	int stepMs = 500;
	bool useBpm = false;
	float bpm = 120.0f;
	/// Part du pas consacree au fondu : 0 pour une coupure franche, 1 pour un
	/// fondu permanent.
	float fadeRatio = 0.0f;
	ChaserDirection direction = ChaserDirection::Forward;
};

struct StrobeSettings {
	float hz = 8.0f;
	/// Part du cycle pendant laquelle la lampe est allumee.
	float dutyCycle = 0.5f;
	/// Reprendre la couleur du programme au lieu d'imposer la sienne.
	bool useBaseColor = true;
	LightState color;
	/// Passer par le canal strobe de l'appareil quand il en a un : a 40 Hz de
	/// rafraichissement, un strobe logiciel crenelle au-dela d'une dizaine de
	/// hertz.
	bool preferHardware = true;
};

/// De quoi le sound-reactive fait varier la lumiere.
enum class SoundTarget : uint8_t {
	Intensity,  ///< l'intensite suit le volume
	Hue,        ///< la teinte suit le contenu frequentiel
	StepOnBeat, ///< le chaser avance d'un pas a chaque temps
	FlashOnBeat,///< un eclat a chaque temps
};

struct SoundSettings {
	SoundTarget target = SoundTarget::Intensity;
	/// Bande ecoutee : 0 grave, 1 medium, 2 aigu.
	int band = 0;
	float sensitivity = 1.0f;
	float threshold = 0.05f;
	/// Constante de lissage, en millisecondes, pour la descente.
	float smoothingMs = 120.0f;

	/// Reprendre la couleur du programme au lieu d'imposer la sienne.
	bool useBaseColor = true;
	LightState color;
};

/// Un canal force a la main.
///
/// Le numero est celui du document du constructeur : 1 designe le premier
/// canal du projecteur, pas l'adresse DMX absolue. C'est ainsi que les tables
/// de canaux sont ecrites, et cela suit l'appareil si on le readresse.
struct ManualChannel {
	int channel = 1;
	uint8_t value = 0;
};

/// Un effet embarque dans l'appareil, comme le mode FX du T4c.
struct BuiltinFxSettings {
	std::string effectId;
	/// 1 a 10, ou 0 pour la valeur aleatoire quand l'effet l'accepte.
	int frequency = 5;
	/// Variante de l'effet : combinaison de couleurs, plage de temperature.
	int variant = 0;

	/// Saisie directe des canaux, pour les appareils dont le profil ne decrit
	/// pas les effets. L'utilisateur recopie alors la table du constructeur.
	bool useManual = false;
	std::vector<ManualChannel> manual;
};

struct Effect {
	std::string id;
	std::string name;
	EffectType type = EffectType::Chaser;
	bool enabled = true;
	BlendMode blend = BlendMode::Htp;

	/// Projecteurs vises, dans l'ordre : c'est cet ordre qui donne son sens
	/// au deplacement d'un chaser.
	std::vector<std::string> fixtureIds;

	ChaserSettings chaser;
	StrobeSettings strobe;
	SoundSettings sound;
	BuiltinFxSettings builtin;
};

/// Ce que le moteur sait de l'audio a un instant donne. Rempli par le thread
/// audio a travers des atomiques, lu par le thread de rendu.
struct AudioSnapshot {
	/// Enveloppes par bande, 0 a 1 : grave, medium, aigu.
	float bands[3] = {0.0f, 0.0f, 0.0f};
	/// Nombre de temps detectes depuis le demarrage. Un compteur plutot qu'un
	/// booleen : le rendu ne peut pas manquer un temps entre deux trames.
	uint64_t beatCount = 0;
};

/// Combine deux etats selon le mode de fusion.
LightState blend(const LightState &base, const LightState &overlay, BlendMode mode);

} // namespace obsdmx

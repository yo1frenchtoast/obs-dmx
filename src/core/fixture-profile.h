#pragma once

#include "core/channel-role.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace obsdmx {

/// Un canal dans un mode donne.
///
/// Les bornes ne sont pas toujours 0-255 : le canal strobe du T4c est eteint
/// de 0 a 19 puis couvre 1 a 25 Hz de 20 a 255, et son canal vert/magenta a
/// une bande neutre au milieu. Decrire ces plages dans le profil evite de
/// coder en dur les particularites d'un constructeur dans le moteur.
struct ChannelSpec {
	ChannelRole role = ChannelRole::Unused;
	/// Libelle affiche a l'utilisateur, tire du document constructeur.
	std::string label;
	/// Valeur emise quand le moteur ne pilote pas ce canal.
	uint8_t defaultValue = 0;

	/// Bornes utiles du canal.
	uint8_t rangeMin = 0;
	uint8_t rangeMax = 255;
	/// Valeur signifiant "aucun effet", pour les canaux comme le strobe.
	uint8_t offValue = 0;
	/// Milieu, pour les canaux bipolaires comme le vert/magenta.
	uint8_t neutralValue = 128;

	/// Grandeur physique correspondant a rangeMin et rangeMax : des kelvins
	/// pour la temperature de couleur, des hertz pour le strobe.
	float physicalMin = 0.0f;
	float physicalMax = 0.0f;
};

/// Un effet embarque dans l'appareil, tel que le mode FX du T4c.
struct BuiltinEffect {
	std::string id;
	std::string label;
	/// Valeur a ecrire dans le canal FxSelect.
	uint8_t selectValue = 0;
	/// L'effet accepte une frequence reglable de 1 a 10.
	bool hasFrequency = false;
	/// La frequence accepte en plus une valeur aleatoire.
	bool hasRandomFrequency = false;
	/// Indice, dans le mode, du canal portant la frequence.
	int frequencyChannel = -1;
};

/// Un mode DMX : c'est le reglage choisi sur l'ecran de l'appareil, pas
/// quelque chose que l'on peut imposer par le DMX.
struct FixtureMode {
	std::string id;
	std::string label;
	std::vector<ChannelSpec> channels;

	/// Effets embarques disponibles dans ce mode, s'il s'agit d'un mode FX.
	std::vector<BuiltinEffect> effects;

	size_t channelCount() const { return channels.size(); }

	/// Indice du premier canal portant ce role, ou -1.
	int findRole(ChannelRole role) const;
	bool hasRole(ChannelRole role) const { return findRole(role) >= 0; }
};

/// Un modele de projecteur, avec tous ses modes.
struct FixtureProfile {
	std::string id;
	std::string manufacturer;
	std::string model;
	/// Mode propose par defaut a l'ajout.
	std::string defaultMode;
	std::vector<FixtureMode> modes;

	std::string displayName() const
	{
		return manufacturer.empty() ? model : manufacturer + " " + model;
	}

	const FixtureMode *findMode(const std::string &id) const;
	const FixtureMode *preferredMode() const;
};

} // namespace obsdmx

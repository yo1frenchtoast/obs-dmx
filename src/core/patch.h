#pragma once

#include "core/color.h"
#include "core/fixture-library.h"
#include "core/universe.h"

#include <cstdint>
#include <string>
#include <vector>

namespace obsdmx {

/// Un projecteur declare par l'utilisateur.
struct Fixture {
	/// Identifiant stable, independant du nom : les programmes s'y referent.
	std::string id;
	std::string name;

	std::string profileId;
	/// Mode DMX choisi. Il doit correspondre au reglage fait sur l'appareil,
	/// ce que rien ne permet de verifier depuis le plugin.
	std::string modeId;

	uint16_t universe = 0;
	/// Adresse de depart, 1 a 512.
	int address = 1;

	/// Rang dans la grille de selection, pour retrouver ses projecteurs dans
	/// l'ordre ou ils sont poses dans la salle.
	int order = 0;
};

/// Un chevauchement d'adresses entre deux projecteurs.
struct AddressConflict {
	std::string firstFixtureId;
	std::string secondFixtureId;
	uint16_t universe = 0;
	int firstAddress = 0;
	int secondAddress = 0;
};

/// L'ensemble des projecteurs declares, et ce qu'on peut en deduire.
class Patch {
public:
	explicit Patch(const FixtureLibrary &library) : library_(&library) {}

	const std::vector<Fixture> &fixtures() const { return fixtures_; }

	/// Ajoute un projecteur. L'identifiant est genere s'il est vide.
	const Fixture &add(Fixture fixture);
	bool remove(const std::string &fixtureId);
	void clear() { fixtures_.clear(); }

	Fixture *find(const std::string &fixtureId);
	const Fixture *find(const std::string &fixtureId) const;

	/// Mode DMX effectif d'un projecteur, ou nullptr si le profil a disparu.
	const FixtureMode *modeOf(const Fixture &fixture) const;

	/// Nombre de canaux occupes, 0 si le profil est introuvable.
	size_t footprintOf(const Fixture &fixture) const;

	/// Premiere adresse libre pouvant accueillir un appareil de cette taille.
	/// Renvoie 0 si l'univers est plein.
	int suggestAddress(uint16_t universe, size_t channelCount) const;

	/// Chevauchements d'adresses. Vide quand tout va bien.
	std::vector<AddressConflict> conflicts() const;

	/// Ecrit l'etat lumineux d'un projecteur dans l'univers correspondant.
	/// Les projecteurs dont le profil est introuvable sont ignores.
	void renderFixture(const Fixture &fixture, const LightState &state, std::vector<Universe> &universes) const;

private:
	const FixtureLibrary *library_;
	std::vector<Fixture> fixtures_;
	int nextId_ = 1;
};

} // namespace obsdmx

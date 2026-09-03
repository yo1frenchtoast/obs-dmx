#pragma once

#include "core/fixture-profile.h"

#include <string>
#include <vector>

namespace obsdmx {

/// Les modeles de projecteurs connus, charges depuis data/fixtures.
///
/// Un profil illisible est ignore avec un avertissement plutot que de faire
/// echouer le chargement : une bibliotheque partiellement valide reste utile.
class FixtureLibrary {
public:
	/// Charge tous les fichiers .json du dossier. Renvoie le nombre de
	/// profils lus ; les erreurs sont deposees dans warnings.
	size_t loadDirectory(const std::string &directory, std::vector<std::string> &warnings);

	/// Charge un profil depuis du texte JSON. Renvoie false et remplit error
	/// si le document est inutilisable.
	bool loadJson(const std::string &json, std::string &error);

	const std::vector<FixtureProfile> &profiles() const { return profiles_; }
	const FixtureProfile *find(const std::string &id) const;

	/// Profils dont le nom contient le texte donne, insensible a la casse.
	std::vector<const FixtureProfile *> search(const std::string &text) const;

	void clear() { profiles_.clear(); }
	bool empty() const { return profiles_.empty(); }

private:
	std::vector<FixtureProfile> profiles_;
};

/// Fabrique un profil pour un appareil absent de la bibliotheque : l'utilisateur
/// donne un nom et decrit ses canaux un par un.
FixtureProfile makeManualProfile(const std::string &name, const std::vector<ChannelRole> &roles);

} // namespace obsdmx

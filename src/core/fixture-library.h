#pragma once

#include "core/fixture-profile.h"

#include <string>
#include <vector>

namespace obsdmx {

/// The known fixture models, loaded from data/fixtures.
///
/// An unreadable profile is skipped with a warning rather than failing the whole
/// load: a partly valid library is still useful.
class FixtureLibrary {
public:
	/// Label language, in OBS's format ("fr-FR", "en-US").
	///
	/// Profiles carry their own labels, so they cannot go through the module's
	/// translation file, which only knows fixed keys. A profile may therefore
	/// give its label in several languages, and this picks which one. Set it
	/// before loading.
	void setLanguage(std::string language) { language_ = std::move(language); }
	const std::string &language() const { return language_; }

	/// Loads every .json file in the directory. Returns how many profiles were
	/// read; problems are appended to warnings.
	size_t loadDirectory(const std::string &directory, std::vector<std::string> &warnings);

	/// Loads one profile from JSON text. Returns false and fills error if the
	/// document is unusable.
	bool loadJson(const std::string &json, std::string &error);

	const std::vector<FixtureProfile> &profiles() const { return profiles_; }
	const FixtureProfile *find(const std::string &id) const;

	/// Profiles whose name contains the given text, case-insensitively.
	std::vector<const FixtureProfile *> search(const std::string &text) const;

	void clear() { profiles_.clear(); }
	bool empty() const { return profiles_.empty(); }

private:
	std::vector<FixtureProfile> profiles_;
	std::string language_ = "en-US";
};

/// Builds a profile for a fixture the library does not know: the user gives a
/// name and describes the channels one by one.
FixtureProfile makeManualProfile(const std::string &name, const std::vector<ChannelRole> &roles);

} // namespace obsdmx

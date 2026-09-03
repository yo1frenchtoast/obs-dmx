#include "core/fixture-library.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace obsdmx {

namespace {

using json = nlohmann::json;

std::string toLower(std::string text)
{
	std::transform(text.begin(), text.end(), text.begin(),
		       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

uint8_t readByte(const json &node, const char *key, uint8_t fallback)
{
	if (!node.contains(key) || !node[key].is_number())
		return fallback;
	const int value = node[key].get<int>();
	return static_cast<uint8_t>(std::clamp(value, 0, 255));
}

float readFloat(const json &node, const char *key, float fallback)
{
	if (!node.contains(key) || !node[key].is_number())
		return fallback;
	return node[key].get<float>();
}

ChannelSpec parseChannel(const json &node)
{
	ChannelSpec spec;
	spec.role = roleFromString(node.value("role", "unused"));
	spec.label = node.value("label", "");

	spec.rangeMin = readByte(node, "range_min", 0);
	spec.rangeMax = readByte(node, "range_max", 255);
	spec.offValue = readByte(node, "off", 0);

	// Sans valeur neutre explicite, on prend le milieu de la plage utile :
	// c'est ce qu'attend un canal bipolaire ordinaire.
	spec.neutralValue = readByte(node, "neutral", static_cast<uint8_t>((spec.rangeMin + spec.rangeMax) / 2));
	spec.defaultValue = readByte(node, "default", 0);

	spec.physicalMin = readFloat(node, "physical_min", 0.0f);
	spec.physicalMax = readFloat(node, "physical_max", 0.0f);
	return spec;
}

BuiltinEffect parseEffect(const json &node)
{
	BuiltinEffect effect;
	effect.id = node.value("id", "");
	effect.label = node.value("label", effect.id);
	effect.selectValue = readByte(node, "select", 0);
	effect.frequencyChannel = node.value("frequency_channel", -1);
	effect.hasFrequency = effect.frequencyChannel >= 0;
	effect.hasRandomFrequency = node.value("random", false);
	return effect;
}

} // namespace

bool FixtureLibrary::loadJson(const std::string &text, std::string &error)
{
	json document = json::parse(text, nullptr, false);
	if (document.is_discarded()) {
		error = "document JSON illisible";
		return false;
	}
	if (!document.is_object()) {
		error = "le document doit etre un objet";
		return false;
	}

	FixtureProfile profile;
	profile.id = document.value("id", "");
	profile.manufacturer = document.value("manufacturer", "");
	profile.model = document.value("model", "");
	profile.defaultMode = document.value("default_mode", "");

	if (profile.id.empty()) {
		error = "champ 'id' manquant";
		return false;
	}
	if (!document.contains("modes") || !document["modes"].is_array() || document["modes"].empty()) {
		error = "aucun mode declare";
		return false;
	}

	for (const auto &modeNode : document["modes"]) {
		FixtureMode mode;
		mode.id = modeNode.value("id", "");
		mode.label = modeNode.value("label", mode.id);

		if (modeNode.contains("channels") && modeNode["channels"].is_array())
			for (const auto &channelNode : modeNode["channels"])
				mode.channels.push_back(parseChannel(channelNode));

		if (mode.channels.empty()) {
			error = "le mode '" + mode.id + "' n'a aucun canal";
			return false;
		}

		if (modeNode.contains("effects") && modeNode["effects"].is_array())
			for (const auto &effectNode : modeNode["effects"])
				mode.effects.push_back(parseEffect(effectNode));

		profile.modes.push_back(std::move(mode));
	}

	// Un rechargement remplace le profil existant plutot que de le doubler.
	const auto existing = std::find_if(profiles_.begin(), profiles_.end(),
					   [&profile](const FixtureProfile &p) { return p.id == profile.id; });
	if (existing != profiles_.end())
		*existing = std::move(profile);
	else
		profiles_.push_back(std::move(profile));

	return true;
}

size_t FixtureLibrary::loadDirectory(const std::string &directory, std::vector<std::string> &warnings)
{
	namespace fs = std::filesystem;

	std::error_code ec;
	if (!fs::is_directory(directory, ec)) {
		warnings.push_back("dossier de profils introuvable : " + directory);
		return 0;
	}

	// Ordre alphabetique : la bibliotheque doit se presenter de la meme
	// facon d'une machine a l'autre.
	std::vector<fs::path> files;
	for (const auto &entry : fs::directory_iterator(directory, ec))
		if (entry.is_regular_file() && entry.path().extension() == ".json")
			files.push_back(entry.path());
	std::sort(files.begin(), files.end());

	size_t loaded = 0;
	for (const auto &file : files) {
		std::ifstream stream(file);
		if (!stream) {
			warnings.push_back("lecture impossible : " + file.string());
			continue;
		}

		std::ostringstream buffer;
		buffer << stream.rdbuf();

		std::string error;
		if (loadJson(buffer.str(), error))
			++loaded;
		else
			warnings.push_back(file.filename().string() + " : " + error);
	}

	return loaded;
}

const FixtureProfile *FixtureLibrary::find(const std::string &id) const
{
	const auto it = std::find_if(profiles_.begin(), profiles_.end(),
				     [&id](const FixtureProfile &p) { return p.id == id; });
	return it != profiles_.end() ? &*it : nullptr;
}

std::vector<const FixtureProfile *> FixtureLibrary::search(const std::string &text) const
{
	const std::string needle = toLower(text);

	std::vector<const FixtureProfile *> found;
	for (const auto &profile : profiles_)
		if (needle.empty() || toLower(profile.displayName()).find(needle) != std::string::npos)
			found.push_back(&profile);
	return found;
}

FixtureProfile makeManualProfile(const std::string &name, const std::vector<ChannelRole> &roles)
{
	FixtureProfile profile;
	profile.id = "manual:" + name;
	profile.model = name;
	profile.defaultMode = "manual";

	FixtureMode mode;
	mode.id = "manual";
	mode.label = name;
	for (ChannelRole role : roles) {
		ChannelSpec spec;
		spec.role = role;
		spec.label = std::string(toString(role));
		mode.channels.push_back(spec);
	}

	profile.modes.push_back(std::move(mode));
	return profile;
}

} // namespace obsdmx

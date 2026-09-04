#include "test-harness.h"

#include "core/effect-runner.h"
#include "core/fixture-library.h"
#include "core/patch.h"

using namespace obsdmx;

#ifndef OBS_DMX_FIXTURES_DIR
#define OBS_DMX_FIXTURES_DIR "data/fixtures"
#endif

namespace {

FixtureLibrary loadShipped(std::vector<std::string> &warnings)
{
	FixtureLibrary library;
	library.loadDirectory(OBS_DMX_FIXTURES_DIR, warnings);
	return library;
}

} // namespace

TEST(every_shipped_profile_loads_without_a_warning)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);

	for (const auto &warning : warnings)
		std::fprintf(stderr, "    warning: %s\n", warning.c_str());

	CHECK(warnings.empty());
	CHECK(library.profiles().size() >= 10);
}

TEST(the_t4c_profile_covers_the_manufacturers_seven_modes)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);

	const auto *t4c = library.find("aputure-amaran-t4c");
	CHECK(t4c != nullptr);
	if (!t4c)
		return;

	CHECK_EQ(t4c->modes.size(), size_t(7));

	// Channel counts taken from the Aputure document, firmware V1.4.
	CHECK_EQ(t4c->findMode("mode1")->channelCount(), size_t(10));
	CHECK_EQ(t4c->findMode("mode2")->channelCount(), size_t(6));
	CHECK_EQ(t4c->findMode("mode3")->channelCount(), size_t(9));
	CHECK_EQ(t4c->findMode("mode4")->channelCount(), size_t(7));
	CHECK_EQ(t4c->findMode("mode5")->channelCount(), size_t(6));
	CHECK_EQ(t4c->findMode("mode6")->channelCount(), size_t(8));
	CHECK_EQ(t4c->findMode("mode7")->channelCount(), size_t(9));

	// Mode 3 is the one offered by default: it is the only one giving both hue
	// and colour temperature.
	CHECK(t4c->preferredMode()->id == "mode3");
}

TEST(the_t4c_mode_3_has_the_expected_roles)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *mode = library.find("aputure-amaran-t4c")->findMode("mode3");

	CHECK_EQ(mode->findRole(ChannelRole::Dimmer), 0);
	CHECK_EQ(mode->findRole(ChannelRole::Cct), 1);
	CHECK_EQ(mode->findRole(ChannelRole::GreenMagenta), 2);
	CHECK_EQ(mode->findRole(ChannelRole::ColorMix), 3);
	CHECK_EQ(mode->findRole(ChannelRole::Hue), 4);
	CHECK_EQ(mode->findRole(ChannelRole::Saturation), 5);
	CHECK_EQ(mode->findRole(ChannelRole::Strobe), 8);

	// The manufacturer's peculiar ranges must have survived the JSON.
	CHECK_EQ(mode->channels[2].neutralValue, 132);
	CHECK_EQ(mode->channels[2].rangeMin, 21);
	CHECK_EQ(mode->channels[8].rangeMin, 20);
	CHECK_EQ(mode->channels[1].physicalMin, 2500.0f);
	CHECK_EQ(mode->channels[1].physicalMax, 7500.0f);
}

TEST(the_t4c_profile_exposes_the_nine_built_in_effects)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *fx = library.find("aputure-amaran-t4c")->findMode("mode7");

	CHECK_EQ(fx->effects.size(), size_t(9));
	CHECK_EQ(fx->findRole(ChannelRole::FxSelect), 2);

	// Every effect must point at a rate channel that exists.
	for (const auto &effect : fx->effects) {
		CHECK(!effect.id.empty());
		CHECK(effect.hasFrequency);
		CHECK(effect.frequencyChannel >= 0);
		CHECK(effect.frequencyChannel < static_cast<int>(fx->channelCount()));
	}

	// Selection values must be distinct, otherwise two effects would collide.
	for (size_t i = 0; i < fx->effects.size(); ++i)
		for (size_t j = i + 1; j < fx->effects.size(); ++j)
			CHECK(fx->effects[i].selectValue != fx->effects[j].selectValue);
}

TEST(no_shipped_profile_exceeds_one_universe)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);

	for (const auto &profile : library.profiles())
		for (const auto &mode : profile.modes) {
			CHECK(mode.channelCount() >= 1);
			CHECK(mode.channelCount() <= size_t(kSlotsPerUniverse));
		}
}

TEST(every_shipped_profile_has_a_valid_default_mode)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);

	for (const auto &profile : library.profiles()) {
		CHECK(profile.preferredMode() != nullptr);
		// A default_mode naming a mode that does not exist is a silent typo:
		// surface it.
		if (!profile.defaultMode.empty())
			CHECK(profile.findMode(profile.defaultMode) != nullptr);
	}
}

TEST(the_t4c_built_in_effects_produce_the_right_channels)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *fx = library.find("aputure-amaran-t4c")->findMode("mode7");

	BuiltinFxSettings settings;
	settings.effectId = "lightning";
	settings.frequency = 3;

	const auto channels = builtinFxChannels(*fx, settings);

	// Channel 3 of the FX mode: effect selection. Lightning occupies 10 to 19.
	bool foundSelect = false, foundFrequency = false, foundControl = false;
	for (const auto &[index, value] : channels) {
		if (index == 2) { foundSelect = true; CHECK_EQ(value, 15); }
		if (index == 1) { foundControl = true; CHECK(value < 10); } // en boucle, pas a l'arret
		// Rate 3 falls in the 20-29 slice.
		if (index == 5) { foundFrequency = true; CHECK(value >= 20 && value <= 29); }
	}
	CHECK(foundSelect);
	CHECK(foundControl);
	CHECK(foundFrequency);
}

TEST(the_random_rate_is_offered_only_by_effects_that_accept_it)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *fx = library.find("aputure-amaran-t4c")->findMode("mode7");

	// Lightning accepts random: it occupies the 100-109 slice.
	BuiltinFxSettings orage;
	orage.effectId = "lightning";
	orage.frequency = 0;
	for (const auto &[index, value] : builtinFxChannels(*fx, orage))
		if (index == 5)
			CHECK(value >= 100 && value <= 109);

	// Cop car does not, so we fall back on a valid rate rather than writing a
	// reserved value.
	BuiltinFxSettings gyrophare;
	gyrophare.effectId = "cop_car";
	gyrophare.frequency = 0;
	for (const auto &[index, value] : builtinFxChannels(*fx, gyrophare))
		if (index == 4)
			CHECK(value < 100);
}

TEST(an_unknown_built_in_effect_produces_no_channel)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *fx = library.find("aputure-amaran-t4c")->findMode("mode7");

	BuiltinFxSettings settings;
	settings.effectId = "effet-inexistant";
	CHECK(builtinFxChannels(*fx, settings).empty());

	// Nor does a mode without effects, even given a valid identifier.
	const auto *mode3 = library.find("aputure-amaran-t4c")->findMode("mode3");
	settings.effectId = "lightning";
	CHECK(builtinFxChannels(*mode3, settings).empty());
}

TEST(manual_entry_forces_the_channels_asked_for)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	// Mode 3 declares no effects, which is exactly the case manual entry is
	// there for.
	const auto *mode3 = library.find("aputure-amaran-t4c")->findMode("mode3");

	BuiltinFxSettings settings;
	settings.useManual = true;
	settings.manual = {{1, 255}, {5, 128}, {9, 42}};

	const auto channels = builtinFxChannels(*mode3, settings);
	CHECK_EQ(channels.size(), size_t(3));

	// Manufacturer numbering starts at 1, internal indices at 0.
	CHECK_EQ(channels[0].first, 0);
	CHECK_EQ(channels[0].second, 255);
	CHECK_EQ(channels[1].first, 4);
	CHECK_EQ(channels[1].second, 128);
	CHECK_EQ(channels[2].first, 8);
	CHECK_EQ(channels[2].second, 42);
}

TEST(manual_entry_refuses_to_spill_onto_the_neighbouring_fixture)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *mode3 = library.find("aputure-amaran-t4c")->findMode("mode3");
	CHECK_EQ(mode3->channelCount(), size_t(9));

	BuiltinFxSettings settings;
	settings.useManual = true;
	// 10 is past the fixture's 9 channels: writing it would drive its neighbour.
	settings.manual = {{9, 10}, {10, 20}, {100, 30}, {0, 40}, {-1, 50}};

	const auto channels = builtinFxChannels(*mode3, settings);
	CHECK_EQ(channels.size(), size_t(1));
	CHECK_EQ(channels[0].first, 8);
	CHECK_EQ(channels[0].second, 10);
}

TEST(manual_entry_overrides_the_effect_library)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *fx = library.find("aputure-amaran-t4c")->findMode("mode7");

	// Even on a mode that knows effects, manual entry takes over: it is the way
	// out when the library gets something wrong.
	BuiltinFxSettings settings;
	settings.useManual = true;
	settings.effectId = "lightning";
	settings.manual = {{3, 77}};

	const auto channels = builtinFxChannels(*fx, settings);
	CHECK_EQ(channels.size(), size_t(1));
	CHECK_EQ(channels[0].first, 2);
	CHECK_EQ(channels[0].second, 77);
}

TEST(empty_manual_entry_writes_nothing)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *mode3 = library.find("aputure-amaran-t4c")->findMode("mode3");

	BuiltinFxSettings settings;
	settings.useManual = true;
	CHECK(builtinFxChannels(*mode3, settings).empty());
}

TEST(the_wild_wash_rgb_profile_matches_the_manual)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);

	const auto *ww = library.find("stairville-wild-wash-rgb");
	CHECK(ww != nullptr);
	if (!ww)
		return;

	// Eight modes, as listed in the fixture's own menu.
	CHECK_EQ(ww->modes.size(), size_t(8));
	CHECK_EQ(ww->findMode("1ch")->channelCount(), size_t(1));
	CHECK_EQ(ww->findMode("2ch1")->channelCount(), size_t(2));
	CHECK_EQ(ww->findMode("2ch2")->channelCount(), size_t(2));
	CHECK_EQ(ww->findMode("3ch1")->channelCount(), size_t(3));
	CHECK_EQ(ww->findMode("3ch2")->channelCount(), size_t(3));
	CHECK_EQ(ww->findMode("3ch3")->channelCount(), size_t(3));
	CHECK_EQ(ww->findMode("4ch")->channelCount(), size_t(4));
	CHECK_EQ(ww->findMode("6ch")->channelCount(), size_t(6));

	// 6Ch is the only one offering dimmer, strobe and RGB together.
	CHECK(ww->preferredMode()->id == "6ch");
}

TEST(wild_wash_6ch_drives_red_green_and_blue)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *mode = library.find("stairville-wild-wash-rgb")->findMode("6ch");

	LightState state;
	state.intensity = 1.0f;
	state.colorMix = 1.0f;
	state.hue = 120.0f; // vert
	state.saturation = 1.0f;

	const auto values = renderState(*mode, state);
	CHECK_EQ(values.size(), size_t(6));
	CHECK_EQ(values[0], 255); // intensite
	CHECK_EQ(values[2], 0);   // rouge
	CHECK_EQ(values[3], 255); // vert
	CHECK_EQ(values[4], 0);   // bleu
	CHECK_EQ(values[5], 0);   // commande sonore de l'appareil : laissee coupee
}

TEST(wild_wash_strobe_starts_at_a_different_place_per_mode)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *ww = library.find("stairville-wild-wash-rgb");

	LightState state;
	state.intensity = 1.0f;

	// With no strobe, both modes leave the LEDs lit (0-5), not in the blackout
	// zone (6-10).
	CHECK_EQ(renderState(*ww->findMode("6ch"), state)[1], 0);
	CHECK_EQ(renderState(*ww->findMode("3ch2"), state)[1], 0);

	state.strobeHz = 0.1f;

	// In 3Ch2 the strobe range starts at 11.
	const int simple = renderState(*ww->findMode("3ch2"), state)[1];
	CHECK(simple >= 11 && simple <= 14);

	// In 6Ch, values 11 to 127 are taken by the fixture's random effects: the
	// steady strobe only starts at 128. Confusing the two would fire a random
	// effect instead of a strobe.
	const int etendu = renderState(*ww->findMode("6ch"), state)[1];
	CHECK(etendu >= 128 && etendu <= 131);

	// At full speed both cap at 250, not 255: beyond that the fixture returns to
	// steady light.
	state.strobeHz = 30.0f;
	CHECK_EQ(renderState(*ww->findMode("3ch2"), state)[1], 250);
	CHECK_EQ(renderState(*ww->findMode("6ch"), state)[1], 250);
}

TEST(the_wild_wash_default_colour_macro_does_not_leave_it_dark)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *ww = library.find("stairville-wild-wash-rgb");

	LightState state;
	state.intensity = 1.0f;

	// The macro channel is not driven by a lighting intent. At zero it means
	// blackout, so the fixture would stay dark whatever the dimmer did. The
	// profile therefore leaves it on white.
	for (const char *modeId : {"2ch1", "3ch2", "4ch"}) {
		const auto *mode = ww->findMode(modeId);
		const int macro = mode->findRole(ChannelRole::ColorWheel);
		CHECK(macro >= 0);
		if (macro >= 0) {
			const int value = renderState(*mode, state)[static_cast<size_t>(macro)];
			CHECK(value >= 110 && value <= 117); // plage « White » de la notice
		}
	}
}

TEST(the_wild_wash_white_profile_exposes_no_colour_or_temperature)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);

	const auto *ww = library.find("stairville-wild-wash-132-white");
	CHECK(ww != nullptr);
	if (!ww)
		return;

	CHECK_EQ(ww->modes.size(), size_t(3));
	for (const auto &mode : ww->modes) {
		// The fixture is fixed cold white: nothing to drive on that side.
		CHECK(!mode.hasRole(ChannelRole::Red));
		CHECK(!mode.hasRole(ChannelRole::Hue));
		CHECK(!mode.hasRole(ChannelRole::Cct));
	}

	CHECK(ww->findMode("2ch")->hasRole(ChannelRole::Dimmer));
	CHECK(ww->findMode("2ch")->hasRole(ChannelRole::Strobe));
}

TEST(labels_follow_the_requested_language)
{
	// Profiles carry their own labels: they cannot go through the module's
	// translation file, which only knows fixed keys.
	FixtureLibrary anglais;
	anglais.setLanguage("en-US");
	std::vector<std::string> warnings;
	anglais.loadDirectory(OBS_DMX_FIXTURES_DIR, warnings);

	FixtureLibrary francais;
	francais.setLanguage("fr-FR");
	francais.loadDirectory(OBS_DMX_FIXTURES_DIR, warnings);

	const auto *en = anglais.find("aputure-amaran-t4c")->findMode("mode3");
	const auto *fr = francais.find("aputure-amaran-t4c")->findMode("mode3");

	CHECK(en->channels[0].label == "Intensity");
	CHECK(fr->channels[0].label == "Intensite");
	CHECK(en->label != fr->label);

	// The technical values, by contrast, do not depend on the language.
	CHECK_EQ(en->channelCount(), fr->channelCount());
	CHECK_EQ(en->channels[2].neutralValue, fr->channels[2].neutralValue);
}

TEST(an_unknown_language_falls_back_to_english)
{
	FixtureLibrary library;
	library.setLanguage("ja-JP");
	std::vector<std::string> warnings;
	library.loadDirectory(OBS_DMX_FIXTURES_DIR, warnings);

	// A label in the wrong language is still more useful than an empty field.
	const auto *mode = library.find("aputure-amaran-t4c")->findMode("mode3");
	CHECK(mode->channels[0].label == "Intensity");
	CHECK(!mode->label.empty());
}

TEST(a_plain_string_label_is_still_accepted)
{
	// A hand-written profile can make do with a plain string.
	FixtureLibrary library;
	std::string error;
	CHECK(library.loadJson(R"({"id":"x","model":"X","modes":[
		{"id":"m","label":"Simple","channels":[{"role":"dimmer","label":"Dimmer"}]}]})", error));

	const auto *mode = library.find("x")->findMode("m");
	CHECK(mode->label == "Simple");
	CHECK(mode->channels[0].label == "Dimmer");
}

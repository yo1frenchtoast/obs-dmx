#include "test-harness.h"

#include "core/color.h"
#include "core/fixture-library.h"

#include <cmath>

using namespace obsdmx;

namespace {

/// Reproduit le mode 3 du T4c, celui retenu par defaut.
FixtureMode t4cMode3()
{
	FixtureMode mode;
	mode.id = "mode3";
	mode.channels = {
		{ChannelRole::Dimmer, "Intensite", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Cct, "CCT", 0, 0, 255, 0, 128, 2500, 7500},
		{ChannelRole::GreenMagenta, "G/M", 132, 21, 244, 0, 132, 0, 0},
		{ChannelRole::ColorMix, "Fondu", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Hue, "Teinte", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Saturation, "Saturation", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Unused, "Reserve", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Unused, "Reserve", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Strobe, "Strobe", 0, 20, 255, 0, 128, 1, 25},
	};
	return mode;
}

FixtureMode rgbwMode()
{
	FixtureMode mode;
	mode.channels = {
		{ChannelRole::Dimmer, "Intensite", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Red, "R", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Green, "V", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Blue, "B", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::White, "Blanc", 0, 0, 255, 0, 128, 0, 0},
	};
	return mode;
}

bool near(int actual, int expected, int tolerance = 2)
{
	return std::abs(actual - expected) <= tolerance;
}

} // namespace

TEST(t4c_une_couleur_saturee_ouvre_le_fondu_vers_la_couleur)
{
	LightState state;
	state.intensity = 1.0f;
	state.mode = ColorMode::Tint;
	state.hue = 240.0f; // bleu
	state.saturation = 1.0f;

	const auto values = renderState(t4cMode3(), state);

	CHECK_EQ(values[0], 255);              // intensite
	CHECK(near(values[3], 255));           // fondu ouvert : sans lui la lampe reste blanche
	CHECK(near(values[4], 170));           // teinte : 240/360 * 255
	CHECK_EQ(values[5], 255);              // saturation
	CHECK_EQ(values[8], 0);                // strobe eteint
}

TEST(t4c_en_mode_blanc_referme_le_fondu)
{
	LightState state;
	state.intensity = 0.5f;
	state.mode = ColorMode::White;
	state.cct = 5000.0f;

	const auto values = renderState(t4cMode3(), state);

	CHECK(near(values[0], 128));  // intensite a 50 %
	CHECK_EQ(values[3], 0);       // fondu referme : moteur blanc
	CHECK_EQ(values[5], 0);       // saturation nulle
	// 5000 K sur la plage 2500-7500 : la moitie.
	CHECK(near(values[1], 128));
}

TEST(t4c_temperature_de_couleur_aux_bornes)
{
	FixtureMode mode = t4cMode3();
	LightState state;
	state.mode = ColorMode::White;

	state.cct = 2500.0f;
	CHECK_EQ(renderState(mode, state)[1], 0);

	state.cct = 7500.0f;
	CHECK_EQ(renderState(mode, state)[1], 255);

	// Hors bornes : on borne au lieu de deborder.
	state.cct = 1000.0f;
	CHECK_EQ(renderState(mode, state)[1], 0);
	state.cct = 20000.0f;
	CHECK_EQ(renderState(mode, state)[1], 255);
}

TEST(t4c_vert_magenta_respecte_la_bande_neutre)
{
	FixtureMode mode = t4cMode3();
	LightState state;

	// Le T4c place son neutre a 132, pas au milieu de 0-255.
	state.greenMagenta = 0.0f;
	CHECK_EQ(renderState(mode, state)[2], 132);

	state.greenMagenta = -1.0f;
	CHECK_EQ(renderState(mode, state)[2], 21);

	state.greenMagenta = 1.0f;
	CHECK_EQ(renderState(mode, state)[2], 244);

	// La pente differe de part et d'autre du neutre, qui n'est pas centre.
	state.greenMagenta = -0.5f;
	CHECK(near(renderState(mode, state)[2], 77));
	state.greenMagenta = 0.5f;
	CHECK(near(renderState(mode, state)[2], 188));
}

TEST(t4c_strobe_saute_la_zone_eteinte)
{
	FixtureMode mode = t4cMode3();
	LightState state;

	// 0 Hz doit tomber dans la zone eteinte, pas a la valeur minimale utile.
	state.strobeHz = 0.0f;
	CHECK_EQ(renderState(mode, state)[8], 0);

	// 1 Hz est la premiere frequence valide : elle commence a 20.
	state.strobeHz = 1.0f;
	CHECK_EQ(renderState(mode, state)[8], 20);

	state.strobeHz = 25.0f;
	CHECK_EQ(renderState(mode, state)[8], 255);

	state.strobeHz = 13.0f;
	CHECK(near(renderState(mode, state)[8], 138));
}

TEST(rgbw_sort_le_blanc_du_melange)
{
	LightState state;
	state.intensity = 1.0f;
	state.mode = ColorMode::Tint;
	state.hue = 0.0f;

	// Pleinement sature : tout le rouge, pas de blanc.
	state.saturation = 1.0f;
	auto values = renderState(rgbwMode(), state);
	CHECK_EQ(values[1], 255); // rouge
	CHECK_EQ(values[2], 0);
	CHECK_EQ(values[3], 0);
	CHECK_EQ(values[4], 0);   // blanc

	// Desature : le blanc prend le relais plutot que d'etre fabrique avec
	// les trois couleurs, ce qui serait moins lumineux.
	state.saturation = 0.0f;
	values = renderState(rgbwMode(), state);
	CHECK_EQ(values[1], 0);
	CHECK_EQ(values[2], 0);
	CHECK_EQ(values[3], 0);
	CHECK_EQ(values[4], 255);
}

TEST(sans_gradateur_l_intensite_se_reporte_sur_les_couleurs)
{
	FixtureMode mode;
	mode.channels = {
		{ChannelRole::Red, "R", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Green, "V", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Blue, "B", 0, 0, 255, 0, 128, 0, 0},
	};

	LightState state;
	state.intensity = 0.5f;
	state.hue = 0.0f;
	state.saturation = 1.0f;

	const auto values = renderState(mode, state);
	// Sans ce report, l'appareil resterait a pleine puissance.
	CHECK(near(values[0], 128));
}

TEST(teinte_hors_bornes_est_ramenee_dans_le_cercle)
{
	// Un effet qui fait tourner la teinte deborde naturellement de 360.
	const auto a = hsToRgb(30.0f, 1.0f);
	const auto b = hsToRgb(390.0f, 1.0f);
	const auto c = hsToRgb(-330.0f, 1.0f);

	CHECK(std::abs(a.r - b.r) < 0.001f);
	CHECK(std::abs(a.g - b.g) < 0.001f);
	CHECK(std::abs(a.r - c.r) < 0.001f);
	CHECK(std::abs(a.g - c.g) < 0.001f);
}

TEST(temperature_vers_rvb_va_du_chaud_au_froid)
{
	const auto chaud = cctToRgb(2500.0f);
	const auto froid = cctToRgb(7500.0f);

	// Une lumiere chaude a plus de rouge que de bleu, une froide l'inverse.
	CHECK(chaud.r > chaud.b);
	CHECK(froid.b > chaud.b);
	CHECK(froid.b >= froid.r - 0.01f);
}

TEST(canaux_inconnus_gardent_leur_valeur_par_defaut)
{
	FixtureMode mode;
	mode.channels = {
		{ChannelRole::Dimmer, "Intensite", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Gobo, "Gobos", 17, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Unused, "Reset", 42, 0, 255, 0, 128, 0, 0},
	};

	LightState state;
	state.intensity = 1.0f;

	const auto values = renderState(mode, state);
	CHECK_EQ(values[0], 255);
	CHECK_EQ(values[1], 17); // le profil decide, pas le moteur
	CHECK_EQ(values[2], 42);
}

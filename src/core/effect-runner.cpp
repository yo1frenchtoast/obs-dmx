#include "core/effect-runner.h"

#include "core/patch.h"

#include <algorithm>
#include <cmath>

namespace obsdmx {

namespace {

float clamp01(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}

/// Generateur simple et reproductible : on ne veut pas d'un chaser aleatoire
/// qui depende de l'etat global du programme.
uint32_t nextRandom(uint32_t &state)
{
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return state;
}

double stepDurationMs(const ChaserSettings &chaser)
{
	if (!chaser.useBpm)
		return std::max(1, chaser.stepMs);
	// Un pas par temps : 60000 ms divisees par le tempo.
	return 60000.0 / std::max(1.0f, chaser.bpm);
}

} // namespace

LightState blend(const LightState &base, const LightState &overlay, BlendMode mode)
{
	if (mode == BlendMode::Replace)
		return overlay;

	// Le plus fort l'emporte. On prend l'etat entier du gagnant plutot que de
	// melanger canal par canal : melanger deux teintes donnerait une troisieme
	// couleur que personne n'a demandee.
	return overlay.intensity >= base.intensity ? overlay : base;
}

void EffectRunner::reset()
{
	runtimes_.clear();
}

EffectRunner::Runtime &EffectRunner::runtimeFor(const Effect &effect, Clock::time_point now)
{
	auto it = runtimes_.find(effect.id);
	if (it == runtimes_.end()) {
		Runtime runtime;
		runtime.started = now;
		it = runtimes_.emplace(effect.id, runtime).first;
	}
	return it->second;
}

void EffectRunner::apply(const std::vector<Effect> &effects, const Patch &patch, const AudioSnapshot &audio,
			 Clock::time_point now, std::unordered_map<std::string, LightState> &states)
{
	for (const auto &effect : effects) {
		if (!effect.enabled || effect.fixtureIds.empty())
			continue;

		Runtime &runtime = runtimeFor(effect, now);

		switch (effect.type) {
		case EffectType::Chaser:
			applyChaser(effect, runtime, audio, now, states);
			break;
		case EffectType::Strobe:
			applyStrobe(effect, patch, now, states);
			break;
		case EffectType::Sound:
			applySound(effect, runtime, audio, states);
			break;
		case EffectType::BuiltinFx:
			// Les effets embarques ne passent pas par un etat lumineux :
			// ils forcent directement des canaux, plus tard dans le rendu.
			break;
		}
	}
}

void EffectRunner::applyChaser(const Effect &effect, Runtime &runtime, const AudioSnapshot &audio,
			       Clock::time_point now, std::unordered_map<std::string, LightState> &states)
{
	const auto &chaser = effect.chaser;
	if (chaser.steps.empty())
		return;

	const int stepCount = static_cast<int>(chaser.steps.size());
	const double durationMs = stepDurationMs(chaser);

	const double elapsedMs =
		std::chrono::duration<double, std::milli>(now - runtime.started).count();
	const int elapsedSteps = static_cast<int>(elapsedMs / durationMs);
	const double phase = std::fmod(elapsedMs, durationMs) / durationMs;

	// Position du motif. Pour le sens aleatoire on ne recalcule que lorsque le
	// pas change, sinon le motif sauterait a chaque trame.
	int offset = 0;
	switch (chaser.direction) {
	case ChaserDirection::Forward:
		offset = elapsedSteps;
		break;
	case ChaserDirection::Backward:
		offset = -elapsedSteps;
		break;
	case ChaserDirection::PingPong: {
		// Aller-retour sans repeter les extremites.
		const int span = std::max(1, stepCount - 1);
		const int position = elapsedSteps % (2 * span);
		offset = position <= span ? position : 2 * span - position;
		break;
	}
	case ChaserDirection::Random:
		if (elapsedSteps != runtime.lastStepIndex) {
			runtime.lastStepIndex = elapsedSteps;
			runtime.step = static_cast<int>(nextRandom(runtime.randomState) % stepCount);
		}
		offset = runtime.step;
		break;
	}

	// Le fondu ne couvre qu'une part du pas : au-dela, la couleur est stable.
	const float fade = clamp01(chaser.fadeRatio);
	const float mix = fade <= 0.0f ? 1.0f : clamp01(static_cast<float>(phase) / fade);

	(void)audio;

	for (size_t i = 0; i < effect.fixtureIds.size(); ++i) {
		const std::string &fixtureId = effect.fixtureIds[i];

		// Chaque projecteur est decale d'un cran : c'est ce decalage qui fait
		// courir la lumiere le long de la rampe.
		const int index = static_cast<int>(i) + offset;
		const int current = ((index % stepCount) + stepCount) % stepCount;
		const int previous = ((current - 1) % stepCount + stepCount) % stepCount;

		const LightState value =
			fade > 0.0f ? lerp(chaser.steps[previous], chaser.steps[current], mix) : chaser.steps[current];

		auto it = states.find(fixtureId);
		states[fixtureId] = it != states.end() ? blend(it->second, value, effect.blend) : value;
	}
}

void EffectRunner::applyStrobe(const Effect &effect, const Patch &patch, Clock::time_point now,
			       std::unordered_map<std::string, LightState> &states)
{
	const auto &strobe = effect.strobe;
	const float hz = std::clamp(strobe.hz, 0.1f, 25.0f);

	const double elapsedMs = std::chrono::duration<double, std::milli>(now.time_since_epoch()).count();
	const double periodMs = 1000.0 / hz;
	const bool lit = std::fmod(elapsedMs, periodMs) < periodMs * clamp01(strobe.dutyCycle);

	for (const std::string &fixtureId : effect.fixtureIds) {
		const Fixture *fixture = patch.find(fixtureId);
		if (!fixture)
			continue;

		auto it = states.find(fixtureId);
		const LightState base = it != states.end() ? it->second : LightState::black();

		LightState value = strobe.useBaseColor ? base : strobe.color;

		const FixtureMode *mode = patch.modeOf(*fixture);
		const bool hardware = strobe.preferHardware && mode && mode->hasRole(ChannelRole::Strobe);

		if (hardware) {
			// L'appareil genere lui-meme le clignotement : bien plus net
			// qu'une modulation a 40 Hz, et sans crenelage.
			value.strobeHz = hz;
			if (strobe.useBaseColor)
				value.intensity = base.intensity;
		} else {
			value.strobeHz = 0.0f;
			value.intensity = lit ? (strobe.useBaseColor ? base.intensity : strobe.color.intensity) : 0.0f;
		}

		states[fixtureId] = it != states.end() ? blend(base, value, effect.blend) : value;
	}
}

void EffectRunner::applySound(const Effect &effect, Runtime &runtime, const AudioSnapshot &audio,
			      std::unordered_map<std::string, LightState> &states)
{
	const auto &sound = effect.sound;
	const int band = std::clamp(sound.band, 0, 2);

	const float level = clamp01((audio.bands[band] - sound.threshold) * sound.sensitivity);
	const bool beat = audio.beatCount != runtime.lastBeat;
	if (beat)
		runtime.lastBeat = audio.beatCount;

	for (const std::string &fixtureId : effect.fixtureIds) {
		auto it = states.find(fixtureId);
		const LightState base = it != states.end() ? it->second : LightState::black();

		LightState value = sound.color;

		switch (sound.target) {
		case SoundTarget::Intensity:
			value = base;
			value.intensity = level;
			break;

		case SoundTarget::Hue:
			// Les trois bandes deviennent une position sur le cercle
			// chromatique : les graves au rouge, les aigus au bleu.
			value = base;
			value.colorMix = 1.0f;
			value.hue = std::fmod(audio.bands[0] * 60.0f + audio.bands[1] * 180.0f +
						      audio.bands[2] * 300.0f,
					      360.0f);
			value.saturation = 1.0f;
			break;

		case SoundTarget::FlashOnBeat:
			value = sound.color;
			// L'eclat dure une trame ; c'est court, mais a 40 Hz l'oeil le
			// voit, et cela evite de tenir un minuteur ici.
			value.intensity = beat ? 1.0f : 0.0f;
			break;

		case SoundTarget::StepOnBeat:
			// Traite par le chaser associe : rien a faire sur l'etat.
			value = base;
			break;
		}

		states[fixtureId] = blend(base, value, effect.blend);
	}
}

std::vector<std::pair<int, uint8_t>> builtinFxChannels(const FixtureMode &mode, const BuiltinFxSettings &settings)
{
	std::vector<std::pair<int, uint8_t>> values;

	const auto effect = std::find_if(mode.effects.begin(), mode.effects.end(),
					 [&settings](const BuiltinEffect &e) { return e.id == settings.effectId; });
	if (effect == mode.effects.end())
		return values;

	const int selectChannel = mode.findRole(ChannelRole::FxSelect);
	if (selectChannel < 0)
		return values;
	values.emplace_back(selectChannel, effect->selectValue);

	// Le canal de commande demarre la boucle. La valeur haute vaut "arret" :
	// on reste donc dans la plage basse.
	if (const int control = mode.findRole(ChannelRole::FxControl); control >= 0)
		values.emplace_back(control, 0);

	if (effect->hasFrequency && effect->frequencyChannel >= 0 &&
	    effect->frequencyChannel < static_cast<int>(mode.channelCount())) {
		// Les frequences 1 a 10 occupent des tranches de dix valeurs ; la
		// valeur aleatoire, quand elle existe, occupe la tranche suivante.
		const int slot = settings.frequency == 0 && effect->hasRandomFrequency
					 ? 10
					 : std::clamp(settings.frequency, 1, 10) - 1;
		values.emplace_back(effect->frequencyChannel, static_cast<uint8_t>(slot * 10 + 5));
	}

	return values;
}

} // namespace obsdmx

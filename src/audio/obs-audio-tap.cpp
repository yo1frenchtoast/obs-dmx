#include "audio/obs-audio-tap.h"

#include <obs-module.h>
#include <util/platform.h>

namespace obsdmx {

namespace {

/// Kept apart from output.json, which is about wires and addresses.
constexpr const char *kConfigFile = "audio.json";

/// The main mix. OBS can have several; this is the one streaming uses by
/// default.
constexpr size_t kMainMix = 0;

audio_convert_info requestedFormat(uint32_t sampleRate)
{
	audio_convert_info info = {};
	info.samples_per_sec = sampleRate;
	// A single floating-point channel: OBS does the downmix and conversion
	// itself, which saves doing it here, inside a real-time callback.
	info.format = AUDIO_FORMAT_FLOAT_PLANAR;
	info.speakers = SPEAKERS_MONO;
	info.allow_clipping = false;
	return info;
}

} // namespace

ObsAudioTap::~ObsAudioTap()
{
	stop();
}

void ObsAudioTap::start()
{
	if (active_)
		return;

	audio_t *audio = obs_get_audio();
	if (!audio) {
		blog(LOG_WARNING, "[obs-dmx] OBS audio unavailable: the sound-reactive effects will stay inert");
		return;
	}

	const uint32_t sampleRate = audio_output_get_sample_rate(audio);
	analyzer_.prepare(static_cast<float>(sampleRate));

	const audio_convert_info format = requestedFormat(sampleRate);
	obs_add_raw_audio_callback(kMainMix, &format, &ObsAudioTap::onAudio, this);
	active_ = true;

	blog(LOG_INFO, "[obs-dmx] listening to the audio mix at %u Hz", sampleRate);
}

void ObsAudioTap::stop()
{
	if (!active_)
		return;
	obs_remove_raw_audio_callback(kMainMix, &ObsAudioTap::onAudio, this);
	active_ = false;
}

void ObsAudioTap::onAudio(void *param, size_t, audio_data *data)
{
	// Audio thread, real time: no allocation, no lock, no logging. The analyser
	// writes only atomics.
	if (!data || !data->data[0] || data->frames == 0)
		return;

	auto *self = static_cast<ObsAudioTap *>(param);
	self->analyzer_.process(reinterpret_cast<const float *>(data->data[0]), data->frames);
}

void ObsAudioTap::loadSettings()
{
	char *path = obs_module_config_path(kConfigFile);
	obs_data_t *data = path ? obs_data_create_from_json_file(path) : nullptr;
	bfree(path);
	if (!data)
		return;

	if (obs_data_has_user_value(data, "beat_sensitivity"))
		analyzer_.setBeatSensitivity(
			static_cast<float>(obs_data_get_double(data, "beat_sensitivity")));

	obs_data_release(data);
}

void ObsAudioTap::saveSettings() const
{
	obs_data_t *data = obs_data_create();
	obs_data_set_double(data, "beat_sensitivity", analyzer_.beatSensitivity());

	// The configuration directory does not exist yet on a first launch.
	char *dir = obs_module_config_path("");
	if (dir) {
		os_mkdirs(dir);
		bfree(dir);
	}

	char *path = obs_module_config_path(kConfigFile);
	if (path) {
		if (!obs_data_save_json_safe(data, path, "tmp", "bak"))
			blog(LOG_WARNING, "[obs-dmx] could not save %s", path);
		bfree(path);
	}
	obs_data_release(data);
}

} // namespace obsdmx

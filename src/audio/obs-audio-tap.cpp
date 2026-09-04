#include "audio/obs-audio-tap.h"

#include <obs-module.h>

namespace obsdmx {

namespace {

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

} // namespace obsdmx

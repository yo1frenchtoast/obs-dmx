#pragma once

#include "core/audio-analysis.h"

// Without this header, "struct audio_data" would declare a type in our own
// namespace instead of naming libobs's.
#include <media-io/audio-io.h>

namespace obsdmx {

/// Takes the audio from OBS's final mix and hands it to the analyser.
///
/// We tap the mix rather than a single source: that is what the audience hears,
/// and therefore what the light should react to.
class ObsAudioTap {
public:
	~ObsAudioTap();

	ObsAudioTap(const ObsAudioTap &) = delete;
	ObsAudioTap &operator=(const ObsAudioTap &) = delete;
	ObsAudioTap() = default;

	void start();
	void stop();
	bool active() const { return active_; }

	AudioSnapshot snapshot() const { return analyzer_.snapshot(); }
	AudioAnalyzer &analyzer() { return analyzer_; }

	/// Beat sensitivity is a matter of the room and the music played there,
	/// not of the show being built, so it lives beside the machine's output
	/// settings rather than in the scene collection.
	void loadSettings();
	void saveSettings() const;

private:
	static void onAudio(void *param, size_t mixIndex, audio_data *data);

	AudioAnalyzer analyzer_;
	bool active_ = false;
};

} // namespace obsdmx

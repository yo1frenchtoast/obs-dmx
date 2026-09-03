#pragma once

#include "core/audio-analysis.h"

// Sans cet en-tete, "struct audio_data" declare un type dans notre propre
// namespace au lieu de designer celui de libobs.
#include <media-io/audio-io.h>

namespace obsdmx {

/// Prend l'audio du mix final d'OBS et le donne a l'analyseur.
///
/// On se branche sur le mix plutot que sur une source : c'est ce que le public
/// entend, donc ce sur quoi la lumiere doit reagir.
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

private:
	static void onAudio(void *param, size_t mixIndex, audio_data *data);

	AudioAnalyzer analyzer_;
	bool active_ = false;
};

} // namespace obsdmx

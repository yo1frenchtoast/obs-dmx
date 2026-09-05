#pragma once

#include "core/effect.h"

#include <functional>

namespace obsdmx {

/// What the interface is allowed to do with the audio analyser.
///
/// The pages that show levels sit several constructors away from the analyser,
/// and passing three callbacks down that chain one by one would be noise. They
/// travel together because they are used together: nothing reads the levels
/// without also offering the one setting that governs how they are read.
struct AudioAccess {
	std::function<AudioSnapshot()> snapshot;
	std::function<float()> beatSensitivity;
	std::function<void(float)> setBeatSensitivity;
};

} // namespace obsdmx

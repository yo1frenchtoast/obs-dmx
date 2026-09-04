#pragma once

#include <obs-data.h>

namespace obsdmx {

class Show;

/// The patch, the programmes and the scene attachments live in the scene
/// collection, so the lighting rig follows the OBS project. Output settings
/// depend on the machine and live elsewhere.
///
/// Key used inside the collection document.
inline constexpr const char *kShowDataKey = "obs-dmx";

void saveShow(const Show &show, obs_data_t *collectionData);
void loadShow(Show &show, obs_data_t *collectionData);

} // namespace obsdmx

#pragma once

#include <obs-data.h>

namespace obsdmx {

class Show;

/// Le patch, les programmes et les associations sont ranges dans la collection
/// de scenes : le montage lumiere suit ainsi le projet OBS. Les reglages de
/// sortie, eux, dependent de la machine et vivent ailleurs.
///
/// Cle utilisee dans le document de la collection.
inline constexpr const char *kShowDataKey = "obs-dmx";

void saveShow(const Show &show, obs_data_t *collectionData);
void loadShow(Show &show, obs_data_t *collectionData);

} // namespace obsdmx

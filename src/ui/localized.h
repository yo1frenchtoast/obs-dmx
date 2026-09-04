#pragma once

#include <QString>

#include <obs-module.h>

namespace obsdmx {

/// The module's translated text, as a QString.
///
/// obs_module_text returns char*; every page used to carry its own wrapper,
/// copied six times over. One is enough.
inline QString tr_(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

} // namespace obsdmx

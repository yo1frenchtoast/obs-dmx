#pragma once

#include "core/universe.h"

#include <string>

namespace obsdmx {

/// Interface commune a toutes les sorties DMX (Art-Net, sACN, Enttec).
/// Les implementations sont appelees depuis le thread du moteur, jamais
/// depuis le thread audio ni depuis l'interface.
class DmxOutput {
public:
	virtual ~DmxOutput() = default;

	/// Nom affiche a l'utilisateur.
	virtual std::string name() const = 0;

	/// Ouvre la sortie. Renvoie false et remplit error en cas d'echec.
	virtual bool open(std::string &error) = 0;

	virtual void close() = 0;

	/// Emet un univers. Appele a chaque tick du moteur (40 Hz).
	virtual void send(const Universe &universe) = 0;
};

} // namespace obsdmx

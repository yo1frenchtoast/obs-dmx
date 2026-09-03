#include "core/dmx-engine.h"

namespace obsdmx {

DmxEngine::DmxEngine()
{
	universes_.emplace_back(0);
}

DmxEngine::~DmxEngine()
{
	stop();
}

void DmxEngine::start()
{
	if (running_.exchange(true))
		return;
	thread_ = std::thread(&DmxEngine::run, this);
}

void DmxEngine::stop()
{
	if (!running_.exchange(false))
		return;
	cv_.notify_all();
	if (thread_.joinable())
		thread_.join();

	std::lock_guard<std::mutex> lock(mutex_);
	for (auto &output : outputs_)
		output->close();
}

void DmxEngine::setUniverseCount(size_t count)
{
	if (count < 1)
		count = 1;
	std::lock_guard<std::mutex> lock(mutex_);
	while (universes_.size() < count)
		universes_.emplace_back(static_cast<uint16_t>(universes_.size()));
	universes_.resize(count);
}

void DmxEngine::setUniverseId(size_t index, uint16_t id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (index < universes_.size())
		universes_[index].setId(id);
}

size_t DmxEngine::universeCount() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return universes_.size();
}

void DmxEngine::setRenderFn(RenderFn fn)
{
	std::lock_guard<std::mutex> lock(mutex_);
	render_ = std::move(fn);
}

void DmxEngine::addOutput(std::shared_ptr<DmxOutput> output)
{
	if (!output)
		return;
	std::lock_guard<std::mutex> lock(mutex_);
	outputs_.push_back(std::move(output));
}

void DmxEngine::clearOutputs()
{
	std::vector<std::shared_ptr<DmxOutput>> stale;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		stale.swap(outputs_);
	}
	// Fermeture hors verrou : close() peut bloquer sur un descripteur.
	for (auto &output : stale)
		output->close();
}

std::vector<Universe> DmxEngine::snapshot() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return universes_;
}

void DmxEngine::run()
{
	// Cadence absolue : on vise des instants fixes plutot que d'ajouter un
	// delai apres chaque trame, sinon le temps de rendu decale peu a peu la
	// sortie et les chasers derivent.
	auto next = std::chrono::steady_clock::now();

	while (running_.load(std::memory_order_relaxed)) {
		next += kTickPeriod;

		{
			std::unique_lock<std::mutex> lock(mutex_);
			if (cv_.wait_until(lock, next, [this] { return !running_.load(std::memory_order_relaxed); }))
				break;
		}

		auto now = std::chrono::steady_clock::now();

		// Si le rendu a pris trop de retard (machine chargee, veille),
		// on se recale au lieu de rattraper des dizaines de trames.
		if (now - next > kTickPeriod * 4)
			next = now;

		tick(now);
	}
}

void DmxEngine::tick(std::chrono::steady_clock::time_point now)
{
	std::vector<std::shared_ptr<DmxOutput>> outputs;
	std::vector<Universe> frame;

	{
		std::lock_guard<std::mutex> lock(mutex_);

		for (auto &universe : universes_)
			universe.clear();

		if (render_ && !blackout_.load(std::memory_order_relaxed))
			render_(universes_, now);

		frame = universes_;
		outputs = outputs_;
	}

	// Emission hors verrou : un envoi reseau ne doit pas bloquer l'interface.
	for (auto &output : outputs)
		for (const auto &universe : frame)
			output->send(universe);

	frames_.fetch_add(1, std::memory_order_relaxed);
}

} // namespace obsdmx

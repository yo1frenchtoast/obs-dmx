#pragma once

#include <QWidget>

#include <array>
#include <atomic>
#include <cstdint>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;

namespace obsdmx {

class DmxEngine;
class Universe;

/// Output settings: protocol, destination, universe, plus a per-channel test
/// bench. These depend on the machine rather than on the OBS project, so they
/// live in the module's configuration and not in the scene collection.
class OutputSettingsPage : public QWidget {
	Q_OBJECT

public:
	explicit OutputSettingsPage(DmxEngine &engine, QWidget *parent = nullptr);

	void load();
	void save() const;

	/// Opens the chosen output and hands it to the engine.
	void applyToEngine();

	/// Injects the test bench value. Called from the engine thread, so only
	/// through atomics: reading a Qt widget off the GUI thread is not safe.
	void renderTest(Universe &universe) const;

private slots:
	void onProtocolChanged();
	void onConnectionChanged();
	void onTestChanged();
	void refreshStatus();

private:
	void updateStatus(const QString &message, bool isError);
	void refreshSerialPorts();

	DmxEngine &engine_;

	QComboBox *protocol_ = nullptr;
	QLineEdit *host_ = nullptr;
	QSpinBox *universe_ = nullptr;
	QSpinBox *priority_ = nullptr;
	QComboBox *serialPort_ = nullptr;
	QCheckBox *enabled_ = nullptr;
	QLabel *status_ = nullptr;

	// The form rows, so only those relevant to the chosen protocol are
	// shown.
	QWidget *hostLabel_ = nullptr;
	QWidget *universeLabel_ = nullptr;
	QWidget *priorityLabel_ = nullptr;
	QWidget *serialLabel_ = nullptr;

	/// sACN source identifier, drawn once and then kept: two sources sharing a
	/// CID interfere with each other.
	std::array<uint8_t, 16> sacnCid_{};

	QCheckBox *testEnabledBox_ = nullptr;
	QSpinBox *testChannelBox_ = nullptr;
	QSlider *testValueSlider_ = nullptr;
	QLabel *testValueLabel_ = nullptr;

	// Shared with the engine thread.
	std::atomic<bool> testEnabled_{false};
	std::atomic<int> testChannel_{1};
	std::atomic<int> testValue_{0};

	QString sendingTo_;
	uint64_t lastFrames_ = 0;
	bool loading_ = false;
};

} // namespace obsdmx

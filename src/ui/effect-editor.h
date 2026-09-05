#pragma once

#include "core/effect.h"

#include <QWidget>

#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QSpinBox;
class QFormLayout;
class QStackedWidget;
class QTableWidget;

namespace obsdmx {

class Patch;
class Show;
class SliderRow;

/// Editor for one effect. The type is fixed at creation: turning a chase into a
/// strobe makes no sense, it is better to create another one.
class EffectEditor : public QWidget {
	Q_OBJECT

public:
	EffectEditor(Show &show, std::function<AudioSnapshot()> audioProvider, QWidget *parent = nullptr);

	/// Loads an effect into the form. Passing nullptr empties and disables
	/// it.
	void setEffect(const Effect *effect);
	void reloadFixtures();

signals:
	/// The effect changed: it is up to the programme to take it back.
	void effectChanged(const Effect &effect);

private slots:
	void commit();

private:
	QWidget *buildChaserPage();
	QWidget *buildStrobePage();
	QWidget *buildSoundPage();
	QWidget *buildBuiltinPage();

	void loadStep();
	void commitStep();
	void addStep();
	void removeStep();
	void refreshStepList();
	void refreshBuiltinEffects();
	void refreshSoundPage();
	void refreshChaserTiming();
	void refreshManualTable();
	void commitManualTable();
	void addManualChannel();
	void removeManualChannel();
	/// Smallest channel count among the targeted fixtures, 0 if there are none.
	size_t smallestFootprint() const;

	Show &show_;
	std::function<AudioSnapshot()> audioProvider_;
	Effect effect_;
	bool valid_ = false;
	bool loading_ = false;

	QLineEdit *name_ = nullptr;
	QCheckBox *enabled_ = nullptr;
	QComboBox *blend_ = nullptr;
	QListWidget *targets_ = nullptr;
	QStackedWidget *pages_ = nullptr;

	// Chaser
	QListWidget *steps_ = nullptr;
	SliderRow *stepIntensity_ = nullptr;
	SliderRow *stepColorMix_ = nullptr;
	SliderRow *stepHue_ = nullptr;
	SliderRow *stepSaturation_ = nullptr;
	SliderRow *stepCct_ = nullptr;
	QComboBox *chaserTiming_ = nullptr;
	QWidget *stepMsLabel_ = nullptr;
	QWidget *bpmLabel_ = nullptr;
	QSpinBox *stepMs_ = nullptr;
	QSpinBox *bpm_ = nullptr;
	SliderRow *fadeRatio_ = nullptr;
	QComboBox *direction_ = nullptr;

	// Strobe
	SliderRow *strobeHz_ = nullptr;
	SliderRow *strobeDuty_ = nullptr;
	QCheckBox *strobeUseBase_ = nullptr;
	QCheckBox *strobeHardware_ = nullptr;

	// Sound
	QComboBox *soundTarget_ = nullptr;
	QComboBox *soundBand_ = nullptr;
	SliderRow *soundSensitivity_ = nullptr;
	SliderRow *soundThreshold_ = nullptr;
	QCheckBox *soundUseBase_ = nullptr;
	QWidget *soundColorBox_ = nullptr;
	SliderRow *soundColorMix_ = nullptr;
	SliderRow *soundHue_ = nullptr;
	SliderRow *soundSaturation_ = nullptr;
	SliderRow *soundCct_ = nullptr;
	QLabel *soundBlendWarning_ = nullptr;

	// Built-in effects
	QComboBox *builtinEffect_ = nullptr;
	QComboBox *builtinFrequency_ = nullptr;
	QLabel *builtinWarning_ = nullptr;
	/// Kept in order to hide whole rows: hiding a QFormLayout's field leaves its
	/// label orphaned.
	QFormLayout *builtinForm_ = nullptr;
	QCheckBox *builtinManual_ = nullptr;
	QWidget *builtinManualBox_ = nullptr;
	QTableWidget *builtinTable_ = nullptr;
	QLabel *builtinFootprint_ = nullptr;
};

} // namespace obsdmx

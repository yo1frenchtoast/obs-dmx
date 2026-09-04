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
class QStackedWidget;
class QTableWidget;

namespace obsdmx {

class Patch;
class Show;
class SliderRow;

/// Editeur d'un effet. Le type est fixe a la creation : changer un chaser en
/// strobe n'a pas de sens, mieux vaut en creer un autre.
class EffectEditor : public QWidget {
	Q_OBJECT

public:
	EffectEditor(Show &show, std::function<AudioSnapshot()> audioProvider, QWidget *parent = nullptr);

	/// Charge un effet dans le formulaire. Passer nullptr le vide et le
	/// desactive.
	void setEffect(const Effect *effect);
	void reloadFixtures();

signals:
	/// L'effet a ete modifie : au programme de le reprendre.
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
	void refreshManualTable();
	void commitManualTable();
	void addManualChannel();
	void removeManualChannel();
	/// Plus petit nombre de canaux parmi les projecteurs vises, 0 si aucun.
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
	QCheckBox *useBpm_ = nullptr;
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

	// Effets embarques
	QComboBox *builtinEffect_ = nullptr;
	QComboBox *builtinFrequency_ = nullptr;
	QLabel *builtinWarning_ = nullptr;
	QCheckBox *builtinManual_ = nullptr;
	QWidget *builtinManualBox_ = nullptr;
	QTableWidget *builtinTable_ = nullptr;
	QLabel *builtinFootprint_ = nullptr;
};

} // namespace obsdmx

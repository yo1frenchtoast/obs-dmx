#pragma once

#include <QWidget>

#include <atomic>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;

namespace obsdmx {

class DmxEngine;
class Universe;

/// Reglages de sortie : protocole, destination, univers, plus un banc d'essai
/// par canal. Ces reglages dependent de la machine et non du projet OBS : ils
/// sont donc ranges dans la configuration du module, pas dans la collection de
/// scenes.
class OutputSettingsPage : public QWidget {
	Q_OBJECT

public:
	explicit OutputSettingsPage(DmxEngine &engine, QWidget *parent = nullptr);

	void load();
	void save() const;

	/// Ouvre la sortie choisie et la donne au moteur.
	void applyToEngine();

	/// Injecte la valeur du banc d'essai. Appele depuis le thread du moteur,
	/// donc uniquement a travers des atomiques : lire un widget Qt hors du
	/// thread graphique n'est pas sur.
	void renderTest(Universe &universe) const;

private slots:
	void onConnectionChanged();
	void onTestChanged();
	void refreshStatus();

private:
	void updateStatus(const QString &message, bool isError);

	DmxEngine &engine_;

	QComboBox *protocol_ = nullptr;
	QLineEdit *host_ = nullptr;
	QSpinBox *universe_ = nullptr;
	QCheckBox *enabled_ = nullptr;
	QLabel *status_ = nullptr;

	QCheckBox *testEnabledBox_ = nullptr;
	QSpinBox *testChannelBox_ = nullptr;
	QSlider *testValueSlider_ = nullptr;
	QLabel *testValueLabel_ = nullptr;

	// Partages avec le thread du moteur.
	std::atomic<bool> testEnabled_{false};
	std::atomic<int> testChannel_{1};
	std::atomic<int> testValue_{0};

	uint64_t lastFrames_ = 0;
	bool loading_ = false;
};

} // namespace obsdmx

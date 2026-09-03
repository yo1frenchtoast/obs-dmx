#pragma once

#include "core/show.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTableWidget;

namespace obsdmx {

class SliderRow;

/// Onglet Programmes : construire des ambiances et les associer aux scenes OBS.
///
/// L'apercu en direct est actif tant que cet onglet est visible : l'utilisateur
/// voit ce qu'il regle au lieu de deviner.
class ProgramsPage : public QWidget {
	Q_OBJECT

public:
	ProgramsPage(Show &show, QWidget *parent = nullptr);
	~ProgramsPage() override;

	/// Reconstruit tout depuis le spectacle.
	void reload();
	/// Recharge la seule liste des projecteurs, apres modification du patch.
	void reloadFixtures();
	/// Recharge la seule table des scenes, apres un changement dans OBS.
	void reloadScenes();

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private slots:
	void onProgramSelected();
	void addProgram();
	void removeProgram();
	void renameProgram();
	void onFixtureSelectionChanged();
	void onFixtureChecked(QListWidgetItem *item);
	void onLightChanged();

private:
	Program *currentProgram();
	void loadStateIntoControls();
	void applyControlsToSelection();
	void pushPreview();
	void updateSwatches();

	Show &show_;
	std::vector<Program> programs_;
	std::string currentProgramId_;

	QComboBox *programSelector_ = nullptr;
	QPushButton *removeButton_ = nullptr;
	QListWidget *fixtures_ = nullptr;
	QLabel *noFixturesHint_ = nullptr;

	QWidget *controls_ = nullptr;
	SliderRow *intensity_ = nullptr;
	SliderRow *colorMix_ = nullptr;
	SliderRow *hue_ = nullptr;
	SliderRow *saturation_ = nullptr;
	SliderRow *cct_ = nullptr;
	SliderRow *greenMagenta_ = nullptr;
	SliderRow *strobe_ = nullptr;

	QTableWidget *scenes_ = nullptr;

	bool populating_ = false;
};

} // namespace obsdmx

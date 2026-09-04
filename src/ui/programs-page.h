#pragma once

#include "core/show.h"

#include <QWidget>

#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTableWidget;

namespace obsdmx {

class EffectEditor;
class SliderRow;

/// Programmes tab: building looks and attaching them to OBS scenes.
///
/// The live preview is on while this tab is visible, so the user sees what they
/// are setting instead of guessing.
class ProgramsPage : public QWidget {
	Q_OBJECT

public:
	ProgramsPage(Show &show, std::function<AudioSnapshot()> audioProvider, QWidget *parent = nullptr);
	~ProgramsPage() override;

	/// Rebuilds everything from the show.
	void reload();
	/// Reloads only the fixture list, after the patch changed.
	void reloadFixtures();
	/// Reloads only the scene table, after a change in OBS.
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
	void onEffectSelected();
	void addEffect();
	void removeEffect();
	void onEffectChanged(const Effect &effect);
	void onEffectToggled(QListWidgetItem *item);
	void bindToCurrentScene();

private:
	Program *currentProgram();
	void loadStateIntoControls();
	void applyControlsToSelection();
	void pushPreview();
	void updateSwatches();
	void refreshEffectList();
	void updateBindingStatus();
	void reapplyIfCurrentScene(const std::string &sceneUuid);
	Effect *currentEffect();

	Show &show_;
	std::vector<Program> programs_;
	std::string currentProgramId_;

	QComboBox *programSelector_ = nullptr;
	QPushButton *removeButton_ = nullptr;
	QLabel *bindingStatus_ = nullptr;
	QPushButton *bindButton_ = nullptr;
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

	QListWidget *effects_ = nullptr;
	EffectEditor *effectEditor_ = nullptr;
	QPushButton *addEffectButton_ = nullptr;
	QPushButton *removeEffectButton_ = nullptr;

	QTableWidget *scenes_ = nullptr;

	int effectIdCounter_ = 0;
	bool populating_ = false;
};

} // namespace obsdmx

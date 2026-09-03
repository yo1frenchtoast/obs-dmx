#pragma once

#include <QWidget>

class QPushButton;
class QTabWidget;

namespace obsdmx {

class DmxEngine;
class ObsAudioTap;
class FixtureLibrary;
class OutputSettingsPage;
class PatchPage;
class ProgramsPage;
class Show;

/// Le dock principal, ancre dans OBS.
class DmxDock : public QWidget {
	Q_OBJECT

public:
	DmxDock(DmxEngine &engine, Show &show, const FixtureLibrary &library, ObsAudioTap &audio,
		QWidget *parent = nullptr);
	~DmxDock() override;

	/// Reconstruit l'interface apres un chargement de collection de scenes.
	void reloadFromShow();

	void setBlackout(bool on);

private slots:
	void toggleBlackout();

private:
	DmxEngine &engine_;
	Show &show_;
	ObsAudioTap &audio_;

	QTabWidget *tabs_ = nullptr;
	PatchPage *patchPage_ = nullptr;
	ProgramsPage *programsPage_ = nullptr;
	OutputSettingsPage *outputPage_ = nullptr;
	QPushButton *blackoutButton_ = nullptr;
};

} // namespace obsdmx

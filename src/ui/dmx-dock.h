#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QTimer;

namespace obsdmx {

class DmxEngine;

/// Le dock principal, ancre dans OBS. A ce stade il expose l'etat du moteur et
/// le blackout ; les onglets Projecteurs et Programmes arrivent aux etapes 3 et 4.
class DmxDock : public QWidget {
	Q_OBJECT

public:
	explicit DmxDock(DmxEngine &engine, QWidget *parent = nullptr);

private slots:
	void refreshStatus();
	void toggleBlackout();

private:
	QWidget *buildOutputPage();
	QWidget *buildPlaceholderPage(const QString &title, const QString &explanation);

	DmxEngine &engine_;
	QLabel *statusLabel_ = nullptr;
	QPushButton *blackoutButton_ = nullptr;
	QTimer *statusTimer_ = nullptr;

	uint64_t lastFrames_ = 0;
};

} // namespace obsdmx

#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

namespace obsdmx {

class DmxEngine;
class OutputSettingsPage;

/// Le dock principal, ancre dans OBS. Les onglets Projecteurs et Programmes
/// arrivent aux etapes 3 et 4 ; la sortie est deja fonctionnelle.
class DmxDock : public QWidget {
	Q_OBJECT

public:
	explicit DmxDock(DmxEngine &engine, QWidget *parent = nullptr);
	~DmxDock() override;

private slots:
	void toggleBlackout();

private:
	QWidget *buildPlaceholderPage(const QString &title, const QString &explanation);

	DmxEngine &engine_;
	OutputSettingsPage *outputPage_ = nullptr;
	QPushButton *blackoutButton_ = nullptr;
};

} // namespace obsdmx

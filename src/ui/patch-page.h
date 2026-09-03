#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;

namespace obsdmx {

class FixtureLibrary;
class Show;

/// Onglet Projecteurs : declarer ses appareils et leurs adresses.
class PatchPage : public QWidget {
	Q_OBJECT

public:
	PatchPage(Show &show, const FixtureLibrary &library, QWidget *parent = nullptr);

	/// Reconstruit le tableau depuis le spectacle.
	void reload();

signals:
	/// Le patch a change : les autres pages doivent se rafraichir.
	void patchChanged();

private slots:
	void addFixture();
	void removeSelected();
	void onCellChanged(int row, int column);

private:
	void refreshConflicts();
	std::string selectedFixtureId() const;

	Show &show_;
	const FixtureLibrary &library_;

	QTableWidget *table_ = nullptr;
	QLabel *conflictLabel_ = nullptr;
	QPushButton *removeButton_ = nullptr;
};

} // namespace obsdmx

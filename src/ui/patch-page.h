#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;

namespace obsdmx {

class FixtureLibrary;
class Show;

/// Fixtures tab: declaring the rig and its addresses.
class PatchPage : public QWidget {
	Q_OBJECT

public:
	PatchPage(Show &show, const FixtureLibrary &library, QWidget *parent = nullptr);

	/// Rebuilds the table from the show.
	void reload();

signals:
	/// The patch changed: the other pages must refresh.
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

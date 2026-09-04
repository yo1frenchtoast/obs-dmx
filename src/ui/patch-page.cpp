#include "ui/patch-page.h"
#include "ui/localized.h"

#include "core/fixture-library.h"
#include "core/show.h"
#include "ui/add-fixture-dialog.h"

#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <obs-module.h>

namespace obsdmx {

namespace {

enum Column { ColName = 0, ColModel, ColMode, ColUniverse, ColAddress, ColChannels, ColumnCount };

constexpr int kFixtureIdRole = Qt::UserRole;

} // namespace

PatchPage::PatchPage(Show &show, const FixtureLibrary &library, QWidget *parent)
	: QWidget(parent), show_(show), library_(library)
{
	auto *layout = new QVBoxLayout(this);

	auto *hint = new QLabel(tr_("Patch.Hint"), this);
	hint->setWordWrap(true);
	layout->addWidget(hint);

	table_ = new QTableWidget(0, ColumnCount, this);
	table_->setHorizontalHeaderLabels({tr_("Patch.Column.Name"), tr_("Patch.Column.Model"),
					   tr_("Patch.Column.Mode"), tr_("Patch.Column.Universe"),
					   tr_("Patch.Column.Address"), tr_("Patch.Column.Channels")});
	table_->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
	table_->horizontalHeader()->setSectionResizeMode(ColModel, QHeaderView::ResizeToContents);
	table_->verticalHeader()->setVisible(false);
	table_->setSelectionBehavior(QAbstractItemView::SelectRows);
	table_->setSelectionMode(QAbstractItemView::SingleSelection);
	layout->addWidget(table_, 1);

	conflictLabel_ = new QLabel(this);
	conflictLabel_->setWordWrap(true);
	layout->addWidget(conflictLabel_);

	auto *buttons = new QHBoxLayout();
	auto *addButton = new QPushButton(tr_("Patch.Add"), this);
	removeButton_ = new QPushButton(tr_("Patch.Remove"), this);
	buttons->addWidget(addButton);
	buttons->addWidget(removeButton_);
	buttons->addStretch();
	layout->addLayout(buttons);

	connect(addButton, &QPushButton::clicked, this, &PatchPage::addFixture);
	connect(removeButton_, &QPushButton::clicked, this, &PatchPage::removeSelected);
	connect(table_, &QTableWidget::cellChanged, this, &PatchPage::onCellChanged);
	connect(table_, &QTableWidget::itemSelectionChanged, this,
		[this] { removeButton_->setEnabled(!selectedFixtureId().empty()); });

	reload();
}

void PatchPage::reload()
{
	// Filling the table emits cellChanged for every cell. Blocking the signals
	// beats a flag: one forgotten path would make the interface loop on
	// itself.
	const QSignalBlocker blocker(table_);
	table_->setRowCount(0);

	show_.withPatch([this](const Patch &patch) {
		for (const auto &fixture : patch.fixtures()) {
			const int row = table_->rowCount();
			table_->insertRow(row);

			const FixtureProfile *profile = library_.find(fixture.profileId);
			const FixtureMode *mode = patch.modeOf(fixture);

			auto *name = new QTableWidgetItem(QString::fromStdString(fixture.name));
			name->setData(kFixtureIdRole, QString::fromStdString(fixture.id));
			table_->setItem(row, ColName, name);

			// Model and mode are not editable here: changing them would
			// move the fixture within the universe without warning.
			auto *model = new QTableWidgetItem(
				profile ? QString::fromStdString(profile->displayName()) : tr_("Patch.UnknownProfile"));
			model->setFlags(model->flags() & ~Qt::ItemIsEditable);
			table_->setItem(row, ColModel, model);

			auto *modeItem = new QTableWidgetItem(
				mode ? QString::fromStdString(mode->label.empty() ? mode->id : mode->label) : QString());
			modeItem->setFlags(modeItem->flags() & ~Qt::ItemIsEditable);
			table_->setItem(row, ColMode, modeItem);

			table_->setItem(row, ColUniverse, new QTableWidgetItem(QString::number(fixture.universe)));
			table_->setItem(row, ColAddress, new QTableWidgetItem(QString::number(fixture.address)));

			const size_t span = patch.footprintOf(fixture);
			auto *channels = new QTableWidgetItem(
				span > 0 ? QString("%1 – %2").arg(fixture.address).arg(fixture.address + int(span) - 1)
					 : QString("—"));
			channels->setFlags(channels->flags() & ~Qt::ItemIsEditable);
			table_->setItem(row, ColChannels, channels);
		}
	});

	removeButton_->setEnabled(false);
	refreshConflicts();
}

void PatchPage::refreshConflicts()
{
	const auto conflicts = show_.withPatch([](const Patch &patch) { return patch.conflicts(); });

	if (conflicts.empty()) {
		conflictLabel_->clear();
		return;
	}

	// Name the fixtures involved: "there is a conflict" without saying which
	// helps nobody.
	QStringList details;
	show_.withPatch([&](const Patch &patch) {
		for (const auto &conflict : conflicts) {
			const Fixture *a = patch.find(conflict.firstFixtureId);
			const Fixture *b = patch.find(conflict.secondFixtureId);
			if (a && b)
				details << tr_("Patch.Conflict.Pair")
						   .arg(QString::fromStdString(a->name))
						   .arg(QString::fromStdString(b->name));
		}
	});

	conflictLabel_->setText(QStringLiteral("⚠ ") + tr_("Patch.Conflict") + " " + details.join("  •  "));
}

void PatchPage::addFixture()
{
	AddFixtureDialog dialog(show_, library_, this);
	if (dialog.exec() != QDialog::Accepted)
		return;

	if (dialog.profileId().empty())
		return;

	const FixtureProfile *profile = library_.find(dialog.profileId());
	const FixtureMode *mode = profile ? profile->findMode(dialog.modeId()) : nullptr;
	if (!mode)
		return;

	const int count = dialog.quantity();
	int address = dialog.address();

	show_.withPatch([&](Patch &patch) {
		for (int i = 0; i < count; ++i) {
			Fixture fixture;
			// Numbered only when there are several: "PAR" reads better
			// than "PAR 1" when it stands alone.
			fixture.name = count > 1 ? dialog.fixtureName() + " " + std::to_string(i + 1)
						 : dialog.fixtureName();
			fixture.profileId = dialog.profileId();
			fixture.modeId = dialog.modeId();
			fixture.universe = dialog.universe();
			fixture.address = address;
			patch.add(std::move(fixture));

			address += static_cast<int>(mode->channelCount());
			if (address + static_cast<int>(mode->channelCount()) - 1 > kSlotsPerUniverse)
				break;
		}
	});

	reload();
	emit patchChanged();
}

void PatchPage::removeSelected()
{
	const std::string id = selectedFixtureId();
	if (id.empty())
		return;

	show_.withPatch([&id](Patch &patch) { patch.remove(id); });
	reload();
	emit patchChanged();
}

void PatchPage::onCellChanged(int row, int column)
{
	if (column != ColName && column != ColUniverse && column != ColAddress)
		return;

	const QTableWidgetItem *nameItem = table_->item(row, ColName);
	if (!nameItem)
		return;
	const std::string id = nameItem->data(kFixtureIdRole).toString().toStdString();

	show_.withPatch([&](Patch &patch) {
		Fixture *fixture = patch.find(id);
		if (!fixture)
			return;

		switch (column) {
		case ColName:
			fixture->name = nameItem->text().toStdString();
			break;
		case ColUniverse:
			fixture->universe = static_cast<uint16_t>(
				std::clamp(table_->item(row, ColUniverse)->text().toInt(), 0, 32767));
			break;
		case ColAddress:
			fixture->address = std::clamp(table_->item(row, ColAddress)->text().toInt(), 1, kSlotsPerUniverse);
			break;
		default:
			break;
		}
	});

	// A changed address moves the occupied range and may create a conflict.
	reload();
	emit patchChanged();
}

std::string PatchPage::selectedFixtureId() const
{
	const auto rows = table_->selectionModel() ? table_->selectionModel()->selectedRows() : QModelIndexList();
	if (rows.isEmpty())
		return {};

	const QTableWidgetItem *item = table_->item(rows.first().row(), ColName);
	return item ? item->data(kFixtureIdRole).toString().toStdString() : std::string();
}

} // namespace obsdmx

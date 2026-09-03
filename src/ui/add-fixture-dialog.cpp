#include "ui/add-fixture-dialog.h"

#include "core/fixture-library.h"
#include "core/show.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QVBoxLayout>

#include <obs-module.h>

namespace obsdmx {

namespace {

QString tr_(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

constexpr int kProfileIdRole = Qt::UserRole;

} // namespace

AddFixtureDialog::AddFixtureDialog(const Show &show, const FixtureLibrary &library, QWidget *parent)
	: QDialog(parent), show_(show), library_(library)
{
	setWindowTitle(tr_("Patch.Add.Title"));
	setMinimumWidth(460);

	auto *layout = new QVBoxLayout(this);

	search_ = new QLineEdit(this);
	search_->setPlaceholderText(tr_("Patch.Add.Search"));
	search_->setClearButtonEnabled(true);
	layout->addWidget(search_);

	profiles_ = new QListWidget(this);
	profiles_->setMinimumHeight(180);
	layout->addWidget(profiles_);

	auto *form = new QFormLayout();
	layout->addLayout(form);

	modes_ = new QComboBox(this);
	form->addRow(tr_("Patch.Add.Mode"), modes_);

	modeWarning_ = new QLabel(tr_("Patch.Add.ModeWarning"), this);
	modeWarning_->setWordWrap(true);
	form->addRow(modeWarning_);

	footprint_ = new QLabel(this);
	form->addRow(tr_("Patch.Add.Footprint"), footprint_);

	name_ = new QLineEdit(this);
	form->addRow(tr_("Patch.Add.Name"), name_);

	universe_ = new QSpinBox(this);
	universe_->setRange(0, 32767);
	form->addRow(tr_("Patch.Add.Universe"), universe_);

	address_ = new QSpinBox(this);
	address_->setRange(1, 512);
	form->addRow(tr_("Patch.Add.Address"), address_);

	quantity_ = new QSpinBox(this);
	quantity_->setRange(1, 64);
	quantity_->setToolTip(tr_("Patch.Add.Quantity.Hint"));
	form->addRow(tr_("Patch.Add.Quantity"), quantity_);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);

	connect(search_, &QLineEdit::textChanged, this, &AddFixtureDialog::onSearchChanged);
	connect(profiles_, &QListWidget::currentItemChanged, this, &AddFixtureDialog::onProfileChanged);
	connect(modes_, &QComboBox::currentIndexChanged, this, &AddFixtureDialog::onModeChanged);
	connect(universe_, &QSpinBox::valueChanged, this, [this](int) { suggestAddress(); });

	onSearchChanged({});
}

void AddFixtureDialog::onSearchChanged(const QString &text)
{
	profiles_->clear();
	for (const FixtureProfile *profile : library_.search(text.toStdString())) {
		auto *item = new QListWidgetItem(QString::fromStdString(profile->displayName()), profiles_);
		item->setData(kProfileIdRole, QString::fromStdString(profile->id));
	}
	if (profiles_->count() > 0)
		profiles_->setCurrentRow(0);
	else
		onProfileChanged();
}

void AddFixtureDialog::onProfileChanged()
{
	modes_->clear();

	const FixtureProfile *profile = library_.find(profileId());
	if (!profile) {
		footprint_->clear();
		return;
	}

	for (const auto &mode : profile->modes)
		modes_->addItem(QString::fromStdString(mode.label.empty() ? mode.id : mode.label),
				QString::fromStdString(mode.id));

	// Presente d'emblee le mode conseille par le profil.
	if (const FixtureMode *preferred = profile->preferredMode()) {
		const int index = modes_->findData(QString::fromStdString(preferred->id));
		if (index >= 0)
			modes_->setCurrentIndex(index);
	}

	if (name_->text().isEmpty() || name_->text() == QString::fromStdString(profile->model))
		name_->setText(QString::fromStdString(profile->model));

	onModeChanged();
}

void AddFixtureDialog::onModeChanged()
{
	const FixtureProfile *profile = library_.find(profileId());
	const FixtureMode *mode = profile ? profile->findMode(modeId()) : nullptr;

	footprint_->setText(mode ? tr_("Patch.Add.Footprint.Value").arg(mode->channelCount()) : QString());
	suggestAddress();
}

void AddFixtureDialog::suggestAddress()
{
	const FixtureProfile *profile = library_.find(profileId());
	const FixtureMode *mode = profile ? profile->findMode(modeId()) : nullptr;
	if (!mode)
		return;

	const int suggestion = show_.withPatch([&](const Patch &patch) {
		return patch.suggestAddress(static_cast<uint16_t>(universe_->value()), mode->channelCount());
	});

	// 0 signifie que l'univers est plein : on laisse l'adresse en place plutot
	// que de la remettre a 1, ce qui creerait un chevauchement silencieux.
	if (suggestion > 0)
		address_->setValue(suggestion);
}

std::string AddFixtureDialog::profileId() const
{
	const QListWidgetItem *item = profiles_->currentItem();
	return item ? item->data(kProfileIdRole).toString().toStdString() : std::string();
}

std::string AddFixtureDialog::modeId() const
{
	return modes_->currentData().toString().toStdString();
}

std::string AddFixtureDialog::fixtureName() const
{
	return name_->text().trimmed().toStdString();
}

int AddFixtureDialog::address() const
{
	return address_->value();
}

uint16_t AddFixtureDialog::universe() const
{
	return static_cast<uint16_t>(universe_->value());
}

int AddFixtureDialog::quantity() const
{
	return quantity_->value();
}

} // namespace obsdmx

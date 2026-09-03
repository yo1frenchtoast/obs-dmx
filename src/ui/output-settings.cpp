#include "ui/output-settings.h"

#include "core/dmx-engine.h"
#include "output/artnet-output.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <obs-module.h>
#include <util/platform.h>

namespace obsdmx {

namespace {

constexpr int kStatusIntervalMs = 500;
constexpr const char *kConfigFile = "output.json";

QString tr_(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

} // namespace

OutputSettingsPage::OutputSettingsPage(DmxEngine &engine, QWidget *parent) : QWidget(parent), engine_(engine)
{
	auto *layout = new QVBoxLayout(this);

	// --- Connexion ---------------------------------------------------
	auto *connectionBox = new QGroupBox(tr_("Output.Connection"), this);
	auto *form = new QFormLayout(connectionBox);

	protocol_ = new QComboBox(connectionBox);
	protocol_->addItem(tr_("Output.Protocol.Artnet"), "artnet");
	form->addRow(tr_("Output.Protocol"), protocol_);

	host_ = new QLineEdit(connectionBox);
	host_->setPlaceholderText("192.168.1.50");
	form->addRow(tr_("Output.Host"), host_);

	universe_ = new QSpinBox(connectionBox);
	universe_->setRange(0, 32767);
	form->addRow(tr_("Output.Universe"), universe_);

	enabled_ = new QCheckBox(tr_("Output.Enabled"), connectionBox);
	form->addRow(QString(), enabled_);

	status_ = new QLabel(connectionBox);
	status_->setWordWrap(true);
	status_->setTextFormat(Qt::PlainText);
	form->addRow(status_);

	layout->addWidget(connectionBox);

	// --- Banc d'essai -------------------------------------------------
	auto *testBox = new QGroupBox(tr_("Output.Test"), this);
	auto *testForm = new QFormLayout(testBox);

	auto *testHint = new QLabel(tr_("Output.Test.Hint"), testBox);
	testHint->setWordWrap(true);
	testForm->addRow(testHint);

	testEnabledBox_ = new QCheckBox(tr_("Output.Test.Enable"), testBox);
	testForm->addRow(QString(), testEnabledBox_);

	testChannelBox_ = new QSpinBox(testBox);
	testChannelBox_->setRange(1, kSlotsPerUniverse);
	testForm->addRow(tr_("Output.Test.Channel"), testChannelBox_);

	testValueSlider_ = new QSlider(Qt::Horizontal, testBox);
	testValueSlider_->setRange(0, 255);
	testValueLabel_ = new QLabel("0", testBox);
	testValueLabel_->setMinimumWidth(32);
	auto *valueRow = new QWidget(testBox);
	auto *valueLayout = new QHBoxLayout(valueRow);
	valueLayout->setContentsMargins(0, 0, 0, 0);
	valueLayout->addWidget(testValueSlider_);
	valueLayout->addWidget(testValueLabel_);
	testForm->addRow(tr_("Output.Test.Value"), valueRow);

	layout->addWidget(testBox);
	layout->addStretch();

	connect(protocol_, &QComboBox::currentIndexChanged, this, &OutputSettingsPage::onConnectionChanged);
	connect(host_, &QLineEdit::editingFinished, this, &OutputSettingsPage::onConnectionChanged);
	connect(universe_, &QSpinBox::valueChanged, this, &OutputSettingsPage::onConnectionChanged);
	connect(enabled_, &QCheckBox::toggled, this, &OutputSettingsPage::onConnectionChanged);

	connect(testEnabledBox_, &QCheckBox::toggled, this, &OutputSettingsPage::onTestChanged);
	connect(testChannelBox_, &QSpinBox::valueChanged, this, &OutputSettingsPage::onTestChanged);
	connect(testValueSlider_, &QSlider::valueChanged, this, &OutputSettingsPage::onTestChanged);

	auto *timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, &OutputSettingsPage::refreshStatus);
	timer->start(kStatusIntervalMs);

	load();
}

void OutputSettingsPage::load()
{
	loading_ = true;

	char *path = obs_module_config_path(kConfigFile);
	obs_data_t *data = path ? obs_data_create_from_json_file(path) : nullptr;
	bfree(path);

	if (data) {
		const char *protocol = obs_data_get_string(data, "protocol");
		const int index = protocol_->findData(QString::fromUtf8(protocol));
		if (index >= 0)
			protocol_->setCurrentIndex(index);

		host_->setText(QString::fromUtf8(obs_data_get_string(data, "host")));
		universe_->setValue(static_cast<int>(obs_data_get_int(data, "universe")));
		enabled_->setChecked(obs_data_get_bool(data, "enabled"));
		obs_data_release(data);
	}

	loading_ = false;
	applyToEngine();
}

void OutputSettingsPage::save() const
{
	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, "protocol", protocol_->currentData().toString().toUtf8().constData());
	obs_data_set_string(data, "host", host_->text().toUtf8().constData());
	obs_data_set_int(data, "universe", universe_->value());
	obs_data_set_bool(data, "enabled", enabled_->isChecked());

	// obs_module_config_path pointe vers un dossier qui n'existe pas encore
	// au premier lancement.
	char *dir = obs_module_config_path("");
	if (dir) {
		os_mkdirs(dir);
		bfree(dir);
	}

	char *path = obs_module_config_path(kConfigFile);
	if (path) {
		if (!obs_data_save_json_safe(data, path, "tmp", "bak"))
			blog(LOG_WARNING, "[obs-dmx] impossible d'enregistrer %s", path);
		bfree(path);
	}
	obs_data_release(data);
}

void OutputSettingsPage::applyToEngine()
{
	engine_.setUniverseId(0, static_cast<uint16_t>(universe_->value()));
	engine_.clearOutputs();

	if (!enabled_->isChecked()) {
		updateStatus(tr_("Output.Status.Disabled"), false);
		return;
	}

	const QString host = host_->text().trimmed();
	if (host.isEmpty()) {
		updateStatus(tr_("Output.Status.NoHost"), true);
		return;
	}

	auto output = std::make_shared<ArtnetOutput>(host.toStdString());
	std::string error;
	if (!output->open(error)) {
		updateStatus(tr_("Output.Status.Failed").arg(QString::fromStdString(error)), true);
		return;
	}

	engine_.addOutput(std::move(output));
	updateStatus(tr_("Output.Status.Sending").arg(host).arg(universe_->value()), false);
}

void OutputSettingsPage::renderTest(Universe &universe) const
{
	if (!testEnabled_.load(std::memory_order_relaxed))
		return;
	universe.set(testChannel_.load(std::memory_order_relaxed),
		     static_cast<uint8_t>(testValue_.load(std::memory_order_relaxed)));
}

void OutputSettingsPage::onConnectionChanged()
{
	if (loading_)
		return;
	save();
	applyToEngine();
}

void OutputSettingsPage::onTestChanged()
{
	testEnabled_.store(testEnabledBox_->isChecked(), std::memory_order_relaxed);
	testChannel_.store(testChannelBox_->value(), std::memory_order_relaxed);
	testValue_.store(testValueSlider_->value(), std::memory_order_relaxed);
	testValueLabel_->setText(QString::number(testValueSlider_->value()));
}

void OutputSettingsPage::refreshStatus()
{
	const uint64_t frames = engine_.framesSent();
	const double rate = static_cast<double>(frames - lastFrames_) * 1000.0 / kStatusIntervalMs;
	lastFrames_ = frames;

	if (enabled_->isChecked() && !status_->text().isEmpty() && rate > 0.0) {
		// On complete le message existant plutot que de l'ecraser : garder
		// l'adresse sous les yeux aide au diagnostic.
		const QString base = tr_("Output.Status.Sending").arg(host_->text().trimmed()).arg(universe_->value());
		status_->setText(base + " — " + tr_("Output.Status.Rate").arg(rate, 0, 'f', 1));
	}
}

void OutputSettingsPage::updateStatus(const QString &message, bool isError)
{
	// Pas de couleur codee en dur : OBS a plusieurs themes, clairs comme
	// sombres, et un rouge choisi ici serait illisible dans la moitie d'entre
	// eux. Un prefixe reste visible partout.
	status_->setText(isError ? QStringLiteral("\u26a0 ") + message : message);
}

} // namespace obsdmx

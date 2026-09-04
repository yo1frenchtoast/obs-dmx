#include "ui/effect-editor.h"

#include "core/show.h"
#include "core/universe.h"
#include "ui/level-meter.h"
#include "ui/slider-row.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>

#include <obs-module.h>

#include <algorithm>

namespace obsdmx {

namespace {

QString tr_(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

constexpr int kFixtureIdRole = Qt::UserRole;

/// Les pages de la pile suivent l'ordre de EffectType.
int pageFor(EffectType type)
{
	return static_cast<int>(type);
}

} // namespace

EffectEditor::EffectEditor(Show &show, std::function<AudioSnapshot()> audioProvider, QWidget *parent)
	: QWidget(parent), show_(show), audioProvider_(std::move(audioProvider))
{
	auto *layout = new QVBoxLayout(this);

	auto *header = new QFormLayout();
	name_ = new QLineEdit(this);
	header->addRow(tr_("Effect.Name"), name_);

	enabled_ = new QCheckBox(tr_("Effect.Enabled"), this);
	header->addRow(QString(), enabled_);
	layout->addLayout(header);

	auto *targetsBox = new QGroupBox(tr_("Effect.Targets"), this);
	auto *targetsLayout = new QVBoxLayout(targetsBox);
	auto *targetsHint = new QLabel(tr_("Effect.Targets.Hint"), targetsBox);
	targetsHint->setWordWrap(true);
	targetsLayout->addWidget(targetsHint);
	targets_ = new QListWidget(targetsBox);
	targets_->setMaximumHeight(120);
	targetsLayout->addWidget(targets_);
	layout->addWidget(targetsBox);

	pages_ = new QStackedWidget(this);
	pages_->addWidget(buildChaserPage());
	pages_->addWidget(buildStrobePage());
	pages_->addWidget(buildSoundPage());
	pages_->addWidget(buildBuiltinPage());
	layout->addWidget(pages_);

	// Les modes de fusion sont un vocabulaire de console : ils vivent sous un
	// repli, pas au premier plan.
	auto *advanced = new QGroupBox(tr_("Effect.Advanced"), this);
	advanced->setCheckable(true);
	advanced->setChecked(false);
	auto *advancedLayout = new QFormLayout(advanced);
	blend_ = new QComboBox(advanced);
	blend_->addItem(tr_("Effect.Blend.Htp"), static_cast<int>(BlendMode::Htp));
	blend_->addItem(tr_("Effect.Blend.Replace"), static_cast<int>(BlendMode::Replace));
	blend_->setToolTip(tr_("Effect.Blend.Hint"));
	advancedLayout->addRow(tr_("Effect.Blend"), blend_);
	layout->addWidget(advanced);

	layout->addStretch();

	connect(name_, &QLineEdit::editingFinished, this, &EffectEditor::commit);
	connect(enabled_, &QCheckBox::toggled, this, &EffectEditor::commit);
	connect(blend_, &QComboBox::currentIndexChanged, this, &EffectEditor::commit);
	connect(targets_, &QListWidget::itemChanged, this, &EffectEditor::commit);

	setEffect(nullptr);
}

QWidget *EffectEditor::buildChaserPage()
{
	auto *page = new QWidget(this);
	auto *layout = new QVBoxLayout(page);
	layout->setContentsMargins(0, 0, 0, 0);

	auto *hint = new QLabel(tr_("Effect.Chaser.Hint"), page);
	hint->setWordWrap(true);
	layout->addWidget(hint);

	auto *stepsRow = new QHBoxLayout();
	steps_ = new QListWidget(page);
	steps_->setMaximumHeight(120);
	stepsRow->addWidget(steps_, 1);

	auto *stepButtons = new QVBoxLayout();
	auto *addStepButton = new QPushButton(tr_("Effect.Chaser.AddStep"), page);
	auto *removeStepButton = new QPushButton(tr_("Effect.Chaser.RemoveStep"), page);
	stepButtons->addWidget(addStepButton);
	stepButtons->addWidget(removeStepButton);
	stepButtons->addStretch();
	stepsRow->addLayout(stepButtons);
	layout->addLayout(stepsRow);

	auto *stepForm = new QFormLayout();
	stepIntensity_ = new SliderRow(0.0f, 100.0f, 100, " %", 0, page);
	stepForm->addRow(tr_("Programs.Intensity"), stepIntensity_);
	stepColorMix_ = new SliderRow(0.0f, 100.0f, 100, " %", 0, page);
	stepForm->addRow(tr_("Programs.ColorMix"), stepColorMix_);
	stepHue_ = new SliderRow(0.0f, 360.0f, 360, "°", 0, page);
	stepHue_->setGradient(
		"QSlider::groove:horizontal { height: 10px; border-radius: 5px; background: "
		"qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ff0000, stop:0.167 #ffff00, stop:0.333 #00ff00, "
		"stop:0.5 #00ffff, stop:0.667 #0000ff, stop:0.833 #ff00ff, stop:1 #ff0000); }");
	stepForm->addRow(tr_("Programs.Hue"), stepHue_);
	stepSaturation_ = new SliderRow(0.0f, 100.0f, 100, " %", 0, page);
	stepForm->addRow(tr_("Programs.Saturation"), stepSaturation_);
	stepCct_ = new SliderRow(2000.0f, 10000.0f, 160, " K", 0, page);
	stepForm->addRow(tr_("Programs.Cct"), stepCct_);
	layout->addLayout(stepForm);

	auto *timing = new QFormLayout();
	useBpm_ = new QCheckBox(tr_("Effect.Chaser.UseBpm"), page);
	timing->addRow(QString(), useBpm_);

	stepMs_ = new QSpinBox(page);
	stepMs_->setRange(20, 60000);
	stepMs_->setSingleStep(50);
	stepMs_->setSuffix(" ms");
	timing->addRow(tr_("Effect.Chaser.StepDuration"), stepMs_);

	bpm_ = new QSpinBox(page);
	bpm_->setRange(20, 300);
	timing->addRow(tr_("Effect.Chaser.Bpm"), bpm_);

	fadeRatio_ = new SliderRow(0.0f, 100.0f, 100, " %", 0, page);
	fadeRatio_->setToolTip(tr_("Effect.Chaser.Fade.Hint"));
	timing->addRow(tr_("Effect.Chaser.Fade"), fadeRatio_);

	direction_ = new QComboBox(page);
	direction_->addItem(tr_("Effect.Chaser.Forward"), static_cast<int>(ChaserDirection::Forward));
	direction_->addItem(tr_("Effect.Chaser.Backward"), static_cast<int>(ChaserDirection::Backward));
	direction_->addItem(tr_("Effect.Chaser.PingPong"), static_cast<int>(ChaserDirection::PingPong));
	direction_->addItem(tr_("Effect.Chaser.Random"), static_cast<int>(ChaserDirection::Random));
	timing->addRow(tr_("Effect.Chaser.Direction"), direction_);
	layout->addLayout(timing);

	connect(addStepButton, &QPushButton::clicked, this, &EffectEditor::addStep);
	connect(removeStepButton, &QPushButton::clicked, this, &EffectEditor::removeStep);
	connect(steps_, &QListWidget::currentRowChanged, this, [this](int) { loadStep(); });
	for (SliderRow *slider : {stepIntensity_, stepColorMix_, stepHue_, stepSaturation_, stepCct_})
		connect(slider, &SliderRow::valueChanged, this, [this](float) { commitStep(); });

	connect(useBpm_, &QCheckBox::toggled, this, [this](bool bpm) {
		// Les deux reglages s'excluent : n'afficher que celui qui agit.
		stepMs_->setEnabled(!bpm);
		bpm_->setEnabled(bpm);
		commit();
	});
	connect(stepMs_, &QSpinBox::valueChanged, this, &EffectEditor::commit);
	connect(bpm_, &QSpinBox::valueChanged, this, &EffectEditor::commit);
	connect(fadeRatio_, &SliderRow::valueChanged, this, [this](float) { commit(); });
	connect(direction_, &QComboBox::currentIndexChanged, this, &EffectEditor::commit);

	return page;
}

QWidget *EffectEditor::buildStrobePage()
{
	auto *page = new QWidget(this);
	auto *form = new QFormLayout(page);
	form->setContentsMargins(0, 0, 0, 0);

	strobeHz_ = new SliderRow(1.0f, 25.0f, 240, " Hz", 1, page);
	form->addRow(tr_("Effect.Strobe.Rate"), strobeHz_);

	strobeDuty_ = new SliderRow(5.0f, 95.0f, 90, " %", 0, page);
	strobeDuty_->setToolTip(tr_("Effect.Strobe.Duty.Hint"));
	form->addRow(tr_("Effect.Strobe.Duty"), strobeDuty_);

	strobeUseBase_ = new QCheckBox(tr_("Effect.Strobe.UseBaseColor"), page);
	form->addRow(QString(), strobeUseBase_);

	strobeHardware_ = new QCheckBox(tr_("Effect.Strobe.Hardware"), page);
	strobeHardware_->setToolTip(tr_("Effect.Strobe.Hardware.Hint"));
	form->addRow(QString(), strobeHardware_);

	auto *note = new QLabel(tr_("Effect.Strobe.Note"), page);
	note->setWordWrap(true);
	form->addRow(note);

	connect(strobeHz_, &SliderRow::valueChanged, this, [this](float) { commit(); });
	connect(strobeDuty_, &SliderRow::valueChanged, this, [this](float) { commit(); });
	connect(strobeUseBase_, &QCheckBox::toggled, this, &EffectEditor::commit);
	connect(strobeHardware_, &QCheckBox::toggled, this, &EffectEditor::commit);

	return page;
}

QWidget *EffectEditor::buildSoundPage()
{
	auto *page = new QWidget(this);
	auto *layout = new QVBoxLayout(page);
	layout->setContentsMargins(0, 0, 0, 0);

	auto *form = new QFormLayout();
	soundTarget_ = new QComboBox(page);
	soundTarget_->addItem(tr_("Effect.Sound.Intensity"), static_cast<int>(SoundTarget::Intensity));
	soundTarget_->addItem(tr_("Effect.Sound.Hue"), static_cast<int>(SoundTarget::Hue));
	soundTarget_->addItem(tr_("Effect.Sound.FlashOnBeat"), static_cast<int>(SoundTarget::FlashOnBeat));
	soundTarget_->addItem(tr_("Effect.Sound.StepOnBeat"), static_cast<int>(SoundTarget::StepOnBeat));
	form->addRow(tr_("Effect.Sound.Target"), soundTarget_);

	soundBand_ = new QComboBox(page);
	soundBand_->addItem(tr_("Effect.Sound.Band.Low"), 0);
	soundBand_->addItem(tr_("Effect.Sound.Band.Mid"), 1);
	soundBand_->addItem(tr_("Effect.Sound.Band.High"), 2);
	form->addRow(tr_("Effect.Sound.Band"), soundBand_);

	soundSensitivity_ = new SliderRow(0.1f, 5.0f, 49, "×", 1, page);
	form->addRow(tr_("Effect.Sound.Sensitivity"), soundSensitivity_);

	soundThreshold_ = new SliderRow(0.0f, 50.0f, 50, " %", 0, page);
	soundThreshold_->setToolTip(tr_("Effect.Sound.Threshold.Hint"));
	form->addRow(tr_("Effect.Sound.Threshold"), soundThreshold_);
	layout->addLayout(form);

	auto *meterHint = new QLabel(tr_("Effect.Sound.Meter.Hint"), page);
	meterHint->setWordWrap(true);
	layout->addWidget(meterHint);
	layout->addWidget(new LevelMeter(audioProvider_, page));

	connect(soundTarget_, &QComboBox::currentIndexChanged, this, &EffectEditor::commit);
	connect(soundBand_, &QComboBox::currentIndexChanged, this, &EffectEditor::commit);
	connect(soundSensitivity_, &SliderRow::valueChanged, this, [this](float) { commit(); });
	connect(soundThreshold_, &SliderRow::valueChanged, this, [this](float) { commit(); });

	return page;
}

QWidget *EffectEditor::buildBuiltinPage()
{
	auto *page = new QWidget(this);
	auto *form = new QFormLayout(page);
	form->setContentsMargins(0, 0, 0, 0);

	builtinEffect_ = new QComboBox(page);
	form->addRow(tr_("Effect.Builtin.Effect"), builtinEffect_);

	builtinFrequency_ = new QComboBox(page);
	for (int i = 1; i <= 10; ++i)
		builtinFrequency_->addItem(QString::number(i), i);
	builtinFrequency_->addItem(tr_("Effect.Builtin.Random"), 0);
	form->addRow(tr_("Effect.Builtin.Frequency"), builtinFrequency_);

	builtinWarning_ = new QLabel(page);
	builtinWarning_->setWordWrap(true);
	form->addRow(builtinWarning_);

	// Repli quand le profil ne decrit pas les effets de l'appareil :
	// l'utilisateur recopie la table de canaux du constructeur.
	builtinManual_ = new QCheckBox(tr_("Effect.Builtin.Manual"), page);
	builtinManual_->setToolTip(tr_("Effect.Builtin.Manual.Hint"));
	form->addRow(QString(), builtinManual_);

	builtinManualBox_ = new QWidget(page);
	auto *manualLayout = new QVBoxLayout(builtinManualBox_);
	manualLayout->setContentsMargins(0, 0, 0, 0);

	auto *manualHint = new QLabel(tr_("Effect.Builtin.Manual.Explain"), builtinManualBox_);
	manualHint->setWordWrap(true);
	manualLayout->addWidget(manualHint);

	builtinTable_ = new QTableWidget(0, 2, builtinManualBox_);
	builtinTable_->setHorizontalHeaderLabels({tr_("Effect.Builtin.Manual.Channel"),
						  tr_("Effect.Builtin.Manual.Value")});
	builtinTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	builtinTable_->verticalHeader()->setVisible(false);
	builtinTable_->setMaximumHeight(140);
	manualLayout->addWidget(builtinTable_);

	auto *manualButtons = new QHBoxLayout();
	auto *addChannelButton = new QPushButton(tr_("Effect.Builtin.Manual.Add"), builtinManualBox_);
	auto *removeChannelButton = new QPushButton(tr_("Effect.Builtin.Manual.Remove"), builtinManualBox_);
	manualButtons->addWidget(addChannelButton);
	manualButtons->addWidget(removeChannelButton);
	manualButtons->addStretch();
	manualLayout->addLayout(manualButtons);

	builtinFootprint_ = new QLabel(builtinManualBox_);
	builtinFootprint_->setWordWrap(true);
	manualLayout->addWidget(builtinFootprint_);

	form->addRow(builtinManualBox_);

	connect(builtinEffect_, &QComboBox::currentIndexChanged, this, &EffectEditor::commit);
	connect(builtinFrequency_, &QComboBox::currentIndexChanged, this, &EffectEditor::commit);
	connect(builtinManual_, &QCheckBox::toggled, this, &EffectEditor::commit);
	connect(builtinTable_, &QTableWidget::cellChanged, this, [this](int, int) { commitManualTable(); });
	connect(addChannelButton, &QPushButton::clicked, this, &EffectEditor::addManualChannel);
	connect(removeChannelButton, &QPushButton::clicked, this, &EffectEditor::removeManualChannel);

	return page;
}

void EffectEditor::setEffect(const Effect *effect)
{
	valid_ = effect != nullptr;
	setEnabled(valid_);
	if (!valid_) {
		effect_ = Effect();
		return;
	}

	effect_ = *effect;
	loading_ = true;

	name_->setText(QString::fromStdString(effect_.name));
	enabled_->setChecked(effect_.enabled);
	blend_->setCurrentIndex(blend_->findData(static_cast<int>(effect_.blend)));
	pages_->setCurrentIndex(pageFor(effect_.type));

	reloadFixtures();
	refreshStepList();

	useBpm_->setChecked(effect_.chaser.useBpm);
	stepMs_->setValue(effect_.chaser.stepMs);
	bpm_->setValue(static_cast<int>(effect_.chaser.bpm));
	stepMs_->setEnabled(!effect_.chaser.useBpm);
	bpm_->setEnabled(effect_.chaser.useBpm);
	fadeRatio_->setValueSilently(effect_.chaser.fadeRatio * 100.0f);
	direction_->setCurrentIndex(direction_->findData(static_cast<int>(effect_.chaser.direction)));

	strobeHz_->setValueSilently(effect_.strobe.hz);
	strobeDuty_->setValueSilently(effect_.strobe.dutyCycle * 100.0f);
	strobeUseBase_->setChecked(effect_.strobe.useBaseColor);
	strobeHardware_->setChecked(effect_.strobe.preferHardware);

	soundTarget_->setCurrentIndex(soundTarget_->findData(static_cast<int>(effect_.sound.target)));
	soundBand_->setCurrentIndex(soundBand_->findData(effect_.sound.band));
	soundSensitivity_->setValueSilently(effect_.sound.sensitivity);
	soundThreshold_->setValueSilently(effect_.sound.threshold * 100.0f);

	{
		const QSignalBlocker blocker(builtinManual_);
		builtinManual_->setChecked(effect_.builtin.useManual);
	}
	refreshBuiltinEffects();

	loading_ = false;
	loadStep();
}

void EffectEditor::reloadFixtures()
{
	const QSignalBlocker blocker(targets_);
	targets_->clear();

	show_.withPatch([this](const Patch &patch) {
		for (const auto &fixture : patch.fixtures()) {
			auto *item = new QListWidgetItem(QString::fromStdString(fixture.name), targets_);
			item->setData(kFixtureIdRole, QString::fromStdString(fixture.id));
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

			const bool targeted = std::find(effect_.fixtureIds.begin(), effect_.fixtureIds.end(),
							fixture.id) != effect_.fixtureIds.end();
			item->setCheckState(targeted ? Qt::Checked : Qt::Unchecked);
		}
	});
}

void EffectEditor::refreshStepList()
{
	const QSignalBlocker blocker(steps_);
	const int previous = steps_->currentRow();
	steps_->clear();

	for (size_t i = 0; i < effect_.chaser.steps.size(); ++i) {
		const auto &state = effect_.chaser.steps[i];
		auto *item = new QListWidgetItem(
			tr_("Effect.Chaser.Step").arg(i + 1).arg(int(state.intensity * 100.0f)), steps_);

		// Une pastille de couleur : lire une liste de nombres ne dit rien
		// de ce que le chaser va donner.
		const Rgb tint = hsToRgb(state.hue, state.saturation);
		const Rgb white = cctToRgb(state.cct);
		const float mix = std::clamp(state.colorMix, 0.0f, 1.0f);
		QPixmap pixmap(14, 14);
		pixmap.fill(QColor(int((white.r + (tint.r - white.r) * mix) * 255),
				   int((white.g + (tint.g - white.g) * mix) * 255),
				   int((white.b + (tint.b - white.b) * mix) * 255)));
		item->setIcon(QIcon(pixmap));
	}

	if (previous >= 0 && previous < steps_->count())
		steps_->setCurrentRow(previous);
	else if (steps_->count() > 0)
		steps_->setCurrentRow(0);
}

void EffectEditor::loadStep()
{
	const int row = steps_->currentRow();
	const bool hasStep = row >= 0 && row < static_cast<int>(effect_.chaser.steps.size());

	for (SliderRow *slider : {stepIntensity_, stepColorMix_, stepHue_, stepSaturation_, stepCct_})
		slider->setEnabled(hasStep);
	if (!hasStep)
		return;

	const LightState &state = effect_.chaser.steps[static_cast<size_t>(row)];
	loading_ = true;
	stepIntensity_->setValueSilently(state.intensity * 100.0f);
	stepColorMix_->setValueSilently(state.colorMix * 100.0f);
	stepHue_->setValueSilently(state.hue);
	stepSaturation_->setValueSilently(state.saturation * 100.0f);
	stepCct_->setValueSilently(state.cct);
	loading_ = false;
}

void EffectEditor::commitStep()
{
	if (loading_ || !valid_)
		return;

	const int row = steps_->currentRow();
	if (row < 0 || row >= static_cast<int>(effect_.chaser.steps.size()))
		return;

	LightState &state = effect_.chaser.steps[static_cast<size_t>(row)];
	state.intensity = stepIntensity_->value() / 100.0f;
	state.colorMix = stepColorMix_->value() / 100.0f;
	state.hue = stepHue_->value();
	state.saturation = stepSaturation_->value() / 100.0f;
	state.cct = stepCct_->value();

	refreshStepList();
	emit effectChanged(effect_);
}

void EffectEditor::addStep()
{
	if (!valid_)
		return;

	// Le nouveau pas reprend le precedent : construire une suite se fait en
	// dupliquant puis en ajustant, pas en repartant du noir a chaque fois.
	LightState state;
	state.intensity = 1.0f;
	if (!effect_.chaser.steps.empty())
		state = effect_.chaser.steps.back();

	effect_.chaser.steps.push_back(state);
	refreshStepList();
	steps_->setCurrentRow(static_cast<int>(effect_.chaser.steps.size()) - 1);
	emit effectChanged(effect_);
}

void EffectEditor::removeStep()
{
	const int row = steps_->currentRow();
	if (row < 0 || row >= static_cast<int>(effect_.chaser.steps.size()))
		return;

	effect_.chaser.steps.erase(effect_.chaser.steps.begin() + row);
	refreshStepList();
	emit effectChanged(effect_);
}

void EffectEditor::refreshBuiltinEffects()
{
	const QSignalBlocker blocker(builtinEffect_);
	builtinEffect_->clear();

	// On ne propose que les effets que les projecteurs vises savent faire.
	std::vector<std::pair<std::string, std::string>> available;
	show_.withPatch([&](const Patch &patch) {
		for (const auto &fixtureId : effect_.fixtureIds) {
			const Fixture *fixture = patch.find(fixtureId);
			if (!fixture)
				continue;
			const FixtureMode *mode = patch.modeOf(*fixture);
			if (!mode)
				continue;
			for (const auto &builtin : mode->effects) {
				const auto already = std::find_if(
					available.begin(), available.end(),
					[&builtin](const auto &entry) { return entry.first == builtin.id; });
				if (already == available.end())
					available.emplace_back(builtin.id, builtin.label);
			}
		}
	});

	for (const auto &[id, label] : available)
		builtinEffect_->addItem(QString::fromStdString(label), QString::fromStdString(id));

	const int index = builtinEffect_->findData(QString::fromStdString(effect_.builtin.effectId));
	if (index >= 0)
		builtinEffect_->setCurrentIndex(index);

	const QSignalBlocker frequencyBlocker(builtinFrequency_);
	const int frequencyIndex = builtinFrequency_->findData(effect_.builtin.frequency);
	if (frequencyIndex >= 0)
		builtinFrequency_->setCurrentIndex(frequencyIndex);

	// Sans effet connu, la saisie manuelle est la seule voie : on l'impose
	// plutot que de laisser une liste vide sans explication.
	if (available.empty() && !effect_.builtin.useManual) {
		effect_.builtin.useManual = true;
		const QSignalBlocker manualBlocker(builtinManual_);
		builtinManual_->setChecked(true);
	}
	builtinManual_->setEnabled(!available.empty());

	const bool manual = effect_.builtin.useManual;
	builtinEffect_->setVisible(!manual);
	builtinFrequency_->setVisible(!manual);
	builtinManualBox_->setVisible(manual);

	builtinWarning_->setText(available.empty() ? QStringLiteral("⚠ ") + tr_("Effect.Builtin.None")
						   : QStringLiteral("⚠ ") + tr_("Effect.Builtin.Warning"));

	refreshManualTable();
}

size_t EffectEditor::smallestFootprint() const
{
	// Le plus petit denominateur : ecrire au-dela ne toucherait qu'une partie
	// des projecteurs vises, ce qui serait deroutant.
	size_t smallest = 0;
	bool first = true;

	show_.withPatch([&](const Patch &patch) {
		for (const auto &fixtureId : effect_.fixtureIds) {
			const Fixture *fixture = patch.find(fixtureId);
			if (!fixture)
				continue;
			const size_t span = patch.footprintOf(*fixture);
			if (span == 0)
				continue;
			smallest = first ? span : std::min(smallest, span);
			first = false;
		}
	});

	return smallest;
}

void EffectEditor::refreshManualTable()
{
	const QSignalBlocker blocker(builtinTable_);
	builtinTable_->setRowCount(0);

	for (const auto &entry : effect_.builtin.manual) {
		const int row = builtinTable_->rowCount();
		builtinTable_->insertRow(row);
		builtinTable_->setItem(row, 0, new QTableWidgetItem(QString::number(entry.channel)));
		builtinTable_->setItem(row, 1, new QTableWidgetItem(QString::number(entry.value)));
	}

	const size_t footprint = smallestFootprint();
	if (footprint == 0) {
		builtinFootprint_->setText(tr_("Effect.Builtin.Manual.NoFixture"));
		return;
	}

	// Un canal hors de l'empreinte du projecteur n'est pas emis : le dire,
	// plutot que de laisser l'utilisateur chercher pourquoi rien ne bouge.
	QStringList tooHigh;
	for (const auto &entry : effect_.builtin.manual)
		if (entry.channel < 1 || entry.channel > static_cast<int>(footprint))
			tooHigh << QString::number(entry.channel);

	builtinFootprint_->setText(tooHigh.isEmpty()
					   ? tr_("Effect.Builtin.Manual.Footprint").arg(footprint)
					   : QStringLiteral("⚠ ") +
						     tr_("Effect.Builtin.Manual.OutOfRange")
							     .arg(tooHigh.join(", "))
							     .arg(footprint));
}

void EffectEditor::commitManualTable()
{
	if (loading_ || !valid_)
		return;

	effect_.builtin.manual.clear();
	for (int row = 0; row < builtinTable_->rowCount(); ++row) {
		const QTableWidgetItem *channelItem = builtinTable_->item(row, 0);
		const QTableWidgetItem *valueItem = builtinTable_->item(row, 1);
		if (!channelItem || !valueItem)
			continue;

		ManualChannel entry;
		entry.channel = std::clamp(channelItem->text().toInt(), 1, kSlotsPerUniverse);
		entry.value = static_cast<uint8_t>(std::clamp(valueItem->text().toInt(), 0, 255));
		effect_.builtin.manual.push_back(entry);
	}

	refreshManualTable();
	emit effectChanged(effect_);
}

void EffectEditor::addManualChannel()
{
	if (!valid_)
		return;

	// Le canal suivant celui deja saisi : on recopie une table de haut en bas.
	ManualChannel entry;
	if (!effect_.builtin.manual.empty())
		entry.channel = effect_.builtin.manual.back().channel + 1;

	effect_.builtin.manual.push_back(entry);
	refreshManualTable();
	emit effectChanged(effect_);
}

void EffectEditor::removeManualChannel()
{
	const int row = builtinTable_->currentRow();
	if (row < 0 || row >= static_cast<int>(effect_.builtin.manual.size()))
		return;

	effect_.builtin.manual.erase(effect_.builtin.manual.begin() + row);
	refreshManualTable();
	emit effectChanged(effect_);
}

void EffectEditor::commit()
{
	if (loading_ || !valid_)
		return;

	effect_.name = name_->text().trimmed().toStdString();
	effect_.enabled = enabled_->isChecked();
	effect_.blend = static_cast<BlendMode>(blend_->currentData().toInt());

	effect_.fixtureIds.clear();
	for (int i = 0; i < targets_->count(); ++i) {
		const QListWidgetItem *item = targets_->item(i);
		if (item->checkState() == Qt::Checked)
			effect_.fixtureIds.push_back(item->data(kFixtureIdRole).toString().toStdString());
	}

	effect_.chaser.useBpm = useBpm_->isChecked();
	effect_.chaser.stepMs = stepMs_->value();
	effect_.chaser.bpm = static_cast<float>(bpm_->value());
	effect_.chaser.fadeRatio = fadeRatio_->value() / 100.0f;
	effect_.chaser.direction = static_cast<ChaserDirection>(direction_->currentData().toInt());

	effect_.strobe.hz = strobeHz_->value();
	effect_.strobe.dutyCycle = strobeDuty_->value() / 100.0f;
	effect_.strobe.useBaseColor = strobeUseBase_->isChecked();
	effect_.strobe.preferHardware = strobeHardware_->isChecked();

	effect_.sound.target = static_cast<SoundTarget>(soundTarget_->currentData().toInt());
	effect_.sound.band = soundBand_->currentData().toInt();
	effect_.sound.sensitivity = soundSensitivity_->value();
	effect_.sound.threshold = soundThreshold_->value() / 100.0f;

	effect_.builtin.effectId = builtinEffect_->currentData().toString().toStdString();
	effect_.builtin.frequency = builtinFrequency_->currentData().toInt();
	effect_.builtin.useManual = builtinManual_->isChecked();

	// La liste des effets embarques depend des projecteurs vises, qui viennent
	// peut-etre de changer.
	if (effect_.type == EffectType::BuiltinFx)
		refreshBuiltinEffects();

	emit effectChanged(effect_);
}

} // namespace obsdmx

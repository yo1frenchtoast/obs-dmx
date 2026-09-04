#include "ui/programs-page.h"

#include "obs/scene-binding.h"
#include "ui/effect-editor.h"
#include "ui/slider-row.h"

#include <QComboBox>
#include <QSignalBlocker>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
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
constexpr int kSceneUuidRole = Qt::UserRole;
constexpr int kEffectIdRole = Qt::UserRole;

enum SceneColumn { SceneName = 0, SceneProgram, SceneFade, SceneColumnCount };

/// Couleur approximative d'un etat, pour la pastille de la liste.
QColor swatchFor(const LightState &state)
{
	const Rgb tint = hsToRgb(state.hue, state.saturation);
	const Rgb white = cctToRgb(state.cct);
	const float mix = std::clamp(state.colorMix, 0.0f, 1.0f);

	// L'intensite est prise en compte pour que l'utilisateur distingue un
	// projecteur eteint d'un projecteur allume dans la meme teinte.
	const float scale = std::clamp(state.intensity, 0.0f, 1.0f);
	const auto channel = [&](float w, float t) {
		return static_cast<int>(std::lround((w + (t - w) * mix) * scale * 255.0f));
	};

	return QColor(channel(white.r, tint.r), channel(white.g, tint.g), channel(white.b, tint.b));
}

QIcon swatchIcon(const QColor &color)
{
	QPixmap pixmap(14, 14);
	pixmap.fill(color);
	return QIcon(pixmap);
}

} // namespace

ProgramsPage::ProgramsPage(Show &show, std::function<AudioSnapshot()> audioProvider, QWidget *parent)
	: QWidget(parent), show_(show)
{
	auto *layout = new QVBoxLayout(this);

	// --- choix du programme ---
	// Un dock est etroit : la liste deroulante prend toute la largeur, les
	// boutons vont dessous. Cote a cote, le dernier finissait coupe.
	programSelector_ = new QComboBox(this);
	programSelector_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	layout->addWidget(programSelector_);

	auto *programRow = new QHBoxLayout();
	auto *addButton = new QPushButton(tr_("Programs.Add"), this);
	auto *renameButton = new QPushButton(tr_("Programs.Rename"), this);
	removeButton_ = new QPushButton(tr_("Programs.Remove"), this);
	programRow->addWidget(addButton);
	programRow->addWidget(renameButton);
	programRow->addWidget(removeButton_);
	layout->addLayout(programRow);

	// Sans cette ligne, un programme qu'aucune scene ne declenche s'allume
	// pendant l'edition puis s'eteint des qu'on quitte l'onglet, sans que rien
	// n'explique pourquoi.
	bindingStatus_ = new QLabel(this);
	bindingStatus_->setWordWrap(true);
	layout->addWidget(bindingStatus_);

	bindButton_ = new QPushButton(tr_("Programs.Binding.BindToCurrent"), this);
	layout->addWidget(bindButton_);

	// --- projecteurs et reglages ---
	auto *fixtureBox = new QGroupBox(tr_("Programs.Fixtures"), this);
	auto *fixtureLayout = new QVBoxLayout(fixtureBox);
	auto *fixtureHint = new QLabel(tr_("Programs.Fixtures.Hint"), fixtureBox);
	fixtureHint->setWordWrap(true);
	fixtureLayout->addWidget(fixtureHint);

	fixtures_ = new QListWidget(fixtureBox);
	fixtures_->setSelectionMode(QAbstractItemView::ExtendedSelection);
	fixtures_->setMinimumHeight(120);
	fixtureLayout->addWidget(fixtures_, 1);

	noFixturesHint_ = new QLabel(tr_("Programs.NoFixtures"), fixtureBox);
	noFixturesHint_->setWordWrap(true);
	fixtureLayout->addWidget(noFixturesHint_);
	layout->addWidget(fixtureBox);

	controls_ = new QGroupBox(tr_("Programs.Light"), this);
	auto *form = new QFormLayout(controls_);
	// Dans une colonne etroite, « Température du blanc » ecraserait son
	// curseur : le libelle passe alors au-dessus.
	form->setRowWrapPolicy(QFormLayout::WrapLongRows);

	intensity_ = new SliderRow(0.0f, 100.0f, 100, " %", 0, controls_);
	form->addRow(tr_("Programs.Intensity"), intensity_);

	colorMix_ = new SliderRow(0.0f, 100.0f, 100, " %", 0, controls_);
	form->addRow(tr_("Programs.ColorMix"), colorMix_);

	hue_ = new SliderRow(0.0f, 360.0f, 360, "°", 0, controls_);
	// Un degrade sous le curseur : on choisit sa teinte a l'oeil plutot qu'au
	// nombre.
	hue_->setGradient(
		"QSlider::groove:horizontal { height: 10px; border-radius: 5px; background: "
		"qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ff0000, stop:0.167 #ffff00, stop:0.333 #00ff00, "
		"stop:0.5 #00ffff, stop:0.667 #0000ff, stop:0.833 #ff00ff, stop:1 #ff0000); }");
	form->addRow(tr_("Programs.Hue"), hue_);

	saturation_ = new SliderRow(0.0f, 100.0f, 100, " %", 0, controls_);
	form->addRow(tr_("Programs.Saturation"), saturation_);

	cct_ = new SliderRow(2000.0f, 10000.0f, 160, " K", 0, controls_);
	form->addRow(tr_("Programs.Cct"), cct_);

	greenMagenta_ = new SliderRow(-100.0f, 100.0f, 200, "", 0, controls_);
	form->addRow(tr_("Programs.GreenMagenta"), greenMagenta_);

	strobe_ = new SliderRow(0.0f, 25.0f, 25, " Hz", 0, controls_);
	strobe_->setToolTip(tr_("Programs.Strobe.Hint"));
	form->addRow(tr_("Programs.Strobe"), strobe_);

	layout->addWidget(controls_);

	// --- effets ---
	auto *effectBox = new QGroupBox(tr_("Programs.Effects"), this);
	auto *effectLayout = new QVBoxLayout(effectBox);

	auto *effectListColumn = new QVBoxLayout();
	auto *effectHint = new QLabel(tr_("Programs.Effects.Hint"), effectBox);
	effectHint->setWordWrap(true);
	effectListColumn->addWidget(effectHint);

	effects_ = new QListWidget(effectBox);
	effects_->setMinimumHeight(110);
	effects_->setMaximumHeight(170);
	effectListColumn->addWidget(effects_, 1);

	auto *effectButtons = new QHBoxLayout();
	addEffectButton_ = new QPushButton(tr_("Programs.Effects.Add"), effectBox);
	removeEffectButton_ = new QPushButton(tr_("Programs.Effects.Remove"), effectBox);
	effectButtons->addWidget(addEffectButton_);
	effectButtons->addWidget(removeEffectButton_);
	effectListColumn->addLayout(effectButtons);
	effectLayout->addLayout(effectListColumn);

	effectEditor_ = new EffectEditor(show_, std::move(audioProvider), effectBox);
	effectLayout->addWidget(effectEditor_);
	layout->addWidget(effectBox);

	// --- association aux scenes OBS ---
	auto *sceneBox = new QGroupBox(tr_("Programs.Scenes"), this);
	auto *sceneLayout = new QVBoxLayout(sceneBox);
	auto *sceneHint = new QLabel(tr_("Programs.Scenes.Hint"), sceneBox);
	sceneHint->setWordWrap(true);
	sceneLayout->addWidget(sceneHint);

	scenes_ = new QTableWidget(0, SceneColumnCount, sceneBox);
	scenes_->setHorizontalHeaderLabels(
		{tr_("Programs.Scenes.Scene"), tr_("Programs.Scenes.Program"), tr_("Programs.Scenes.Fade")});
	scenes_->horizontalHeader()->setSectionResizeMode(SceneName, QHeaderView::Stretch);
	scenes_->verticalHeader()->setVisible(false);
	scenes_->setEditTriggers(QAbstractItemView::NoEditTriggers);
	scenes_->setMinimumHeight(120);
	sceneLayout->addWidget(scenes_);
	layout->addWidget(sceneBox);
	layout->addStretch();

	connect(programSelector_, &QComboBox::currentIndexChanged, this, &ProgramsPage::onProgramSelected);
	connect(addButton, &QPushButton::clicked, this, &ProgramsPage::addProgram);
	connect(bindButton_, &QPushButton::clicked, this, &ProgramsPage::bindToCurrentScene);
	connect(renameButton, &QPushButton::clicked, this, &ProgramsPage::renameProgram);
	connect(removeButton_, &QPushButton::clicked, this, &ProgramsPage::removeProgram);
	connect(fixtures_, &QListWidget::itemSelectionChanged, this, &ProgramsPage::onFixtureSelectionChanged);
	connect(fixtures_, &QListWidget::itemChanged, this, &ProgramsPage::onFixtureChecked);

	connect(effects_, &QListWidget::currentRowChanged, this, [this](int) { onEffectSelected(); });
	connect(effects_, &QListWidget::itemChanged, this, &ProgramsPage::onEffectToggled);
	connect(addEffectButton_, &QPushButton::clicked, this, &ProgramsPage::addEffect);
	connect(removeEffectButton_, &QPushButton::clicked, this, &ProgramsPage::removeEffect);
	connect(effectEditor_, &EffectEditor::effectChanged, this, &ProgramsPage::onEffectChanged);

	for (SliderRow *slider : {intensity_, colorMix_, hue_, saturation_, cct_, greenMagenta_, strobe_})
		connect(slider, &SliderRow::valueChanged, this, &ProgramsPage::onLightChanged);

	reload();

}

ProgramsPage::~ProgramsPage()
{
	// L'apercu ne doit pas survivre a la page.
	show_.setPreview(std::nullopt);
}

void ProgramsPage::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	pushPreview();
}

void ProgramsPage::hideEvent(QHideEvent *event)
{
	QWidget::hideEvent(event);
	// Onglet quitte : le programme reellement actif reprend la main.
	show_.setPreview(std::nullopt);
}

void ProgramsPage::reload()
{
	programs_ = show_.programs();

	populating_ = true;
	const QString previous = QString::fromStdString(currentProgramId_);
	programSelector_->clear();
	for (const auto &program : programs_)
		programSelector_->addItem(QString::fromStdString(program.name.empty() ? program.id : program.name),
					  QString::fromStdString(program.id));

	const int index = programSelector_->findData(previous);
	programSelector_->setCurrentIndex(index >= 0 ? index : (programs_.empty() ? -1 : 0));
	currentProgramId_ = programSelector_->currentData().toString().toStdString();
	populating_ = false;

	reloadFixtures();
	reloadScenes();
	onProgramSelected();
}

void ProgramsPage::reloadFixtures()
{
	const QSignalBlocker blocker(fixtures_);
	fixtures_->clear();

	show_.withPatch([this](const Patch &patch) {
		for (const auto &fixture : patch.fixtures()) {
			auto *item = new QListWidgetItem(QString::fromStdString(fixture.name), fixtures_);
			item->setData(kFixtureIdRole, QString::fromStdString(fixture.id));
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
			item->setCheckState(Qt::Unchecked);
		}
	});

	noFixturesHint_->setVisible(fixtures_->count() == 0);

	if (effectEditor_)
		effectEditor_->reloadFixtures();

	updateSwatches();
}

void ProgramsPage::reloadScenes()
{
	populating_ = true;
	scenes_->setRowCount(0);

	const auto bindings = show_.bindings();
	const auto sceneList = SceneBinder::currentScenes();

	for (const auto &scene : sceneList) {
		const int row = scenes_->rowCount();
		scenes_->insertRow(row);

		auto *nameItem = new QTableWidgetItem(QString::fromStdString(scene.name));
		nameItem->setData(kSceneUuidRole, QString::fromStdString(scene.uuid));
		scenes_->setItem(row, SceneName, nameItem);

		auto *combo = new QComboBox(scenes_);
		combo->addItem(tr_("Programs.Scenes.None"), QString());
		for (const auto &program : programs_)
			combo->addItem(QString::fromStdString(program.name.empty() ? program.id : program.name),
				       QString::fromStdString(program.id));

		auto *fade = new QSpinBox(scenes_);
		fade->setRange(0, 60000);
		fade->setSingleStep(100);
		fade->setSuffix(" ms");
		fade->setValue(500);

		const auto binding = std::find_if(bindings.begin(), bindings.end(), [&scene](const SceneBinding &b) {
			return b.sceneUuid == scene.uuid;
		});
		if (binding != bindings.end()) {
			const int index = combo->findData(QString::fromStdString(binding->programId));
			if (index >= 0)
				combo->setCurrentIndex(index);
			fade->setValue(binding->fadeMs);
		}

		const std::string uuid = scene.uuid;
		const std::string name = scene.name;
		auto commit = [this, uuid, name, combo, fade] {
			show_.bindScene(uuid, name, combo->currentData().toString().toStdString(), fade->value());
			updateBindingStatus();
		};
		connect(combo, &QComboBox::currentIndexChanged, this, [commit](int) { commit(); });
		connect(fade, &QSpinBox::valueChanged, this, [commit](int) { commit(); });

		scenes_->setCellWidget(row, SceneProgram, combo);
		scenes_->setCellWidget(row, SceneFade, fade);
	}

	populating_ = false;
}

Program *ProgramsPage::currentProgram()
{
	const auto it = std::find_if(programs_.begin(), programs_.end(),
				     [this](const Program &p) { return p.id == currentProgramId_; });
	return it != programs_.end() ? &*it : nullptr;
}

void ProgramsPage::updateBindingStatus()
{
	Program *program = currentProgram();
	if (!program) {
		bindingStatus_->clear();
		bindButton_->setVisible(false);
		return;
	}

	// Toutes les scenes que ce programme sert.
	QStringList scenes;
	for (const auto &binding : show_.bindings())
		if (binding.programId == program->id)
			scenes << QString::fromStdString(binding.sceneName);

	if (!scenes.isEmpty()) {
		bindingStatus_->setText(tr_("Programs.Binding.Active").arg(scenes.join(", ")));
		bindButton_->setVisible(false);
		return;
	}

	bindingStatus_->setText(QStringLiteral("⚠ ") + tr_("Programs.Binding.None"));

	// Le bouton n'a de sens que s'il y a une scene courante a associer.
	const std::string uuid = SceneBinder::currentSceneUuid();
	bindButton_->setVisible(!uuid.empty());
}

void ProgramsPage::bindToCurrentScene()
{
	Program *program = currentProgram();
	if (!program)
		return;

	const std::string uuid = SceneBinder::currentSceneUuid();
	if (uuid.empty())
		return;

	// Retrouver le nom pour l'affichage : l'association, elle, retient l'UUID.
	std::string name;
	for (const auto &scene : SceneBinder::currentScenes())
		if (scene.uuid == uuid)
			name = scene.name;

	// 500 ms est le fondu par defaut de l'interface : rester coherent avec ce
	// que le tableau des scenes propose.
	show_.bindScene(uuid, name, program->id, 500);

	reloadScenes();
	updateBindingStatus();
}

void ProgramsPage::onProgramSelected()
{
	if (populating_)
		return;

	currentProgramId_ = programSelector_->currentData().toString().toStdString();

	const bool hasProgram = currentProgram() != nullptr;
	controls_->setEnabled(hasProgram);
	fixtures_->setEnabled(hasProgram);
	removeButton_->setEnabled(hasProgram);
	// Un effet appartient a un programme : sans programme choisi, il n'y a
	// nulle part ou le mettre.
	addEffectButton_->setEnabled(hasProgram);
	effects_->setEnabled(hasProgram);

	{
		const QSignalBlocker blocker(fixtures_);
		Program *program = currentProgram();
		for (int i = 0; i < fixtures_->count(); ++i) {
			QListWidgetItem *item = fixtures_->item(i);
			const std::string id = item->data(kFixtureIdRole).toString().toStdString();
			item->setCheckState(program && program->lookFor(id) ? Qt::Checked : Qt::Unchecked);
		}
	}

	updateSwatches();
	refreshEffectList();
	loadStateIntoControls();
	updateBindingStatus();
	pushPreview();
}

void ProgramsPage::addProgram()
{
	bool ok = false;
	const QString name = QInputDialog::getText(this, tr_("Programs.Add.Title"), tr_("Programs.Add.Prompt"),
						   QLineEdit::Normal, tr_("Programs.Add.Default"), &ok);
	if (!ok || name.trimmed().isEmpty())
		return;

	Program program;
	program.name = name.trimmed().toStdString();
	currentProgramId_ = show_.addProgram(std::move(program));

	reload();
}

void ProgramsPage::renameProgram()
{
	Program *program = currentProgram();
	if (!program)
		return;

	bool ok = false;
	const QString name = QInputDialog::getText(this, tr_("Programs.Rename"), tr_("Programs.Add.Prompt"),
						   QLineEdit::Normal, QString::fromStdString(program->name), &ok);
	if (!ok || name.trimmed().isEmpty())
		return;

	program->name = name.trimmed().toStdString();
	show_.updateProgram(*program);
	reload();
}

void ProgramsPage::removeProgram()
{
	if (currentProgramId_.empty())
		return;

	show_.removeProgram(currentProgramId_);
	currentProgramId_.clear();
	reload();
}

void ProgramsPage::onFixtureChecked(QListWidgetItem *item)
{
	if (populating_ || !item)
		return;

	Program *program = currentProgram();
	if (!program)
		return;

	const std::string id = item->data(kFixtureIdRole).toString().toStdString();
	const bool included = item->checkState() == Qt::Checked;

	const auto it = std::find_if(program->looks.begin(), program->looks.end(),
				     [&id](const FixtureLook &look) { return look.fixtureId == id; });

	if (included && it == program->looks.end()) {
		// Un projecteur qu'on vient d'inclure part d'un blanc a mi-course,
		// visible immediatement : un noir laisserait croire a une panne.
		LightState state;
		state.intensity = 0.8f;
		state.colorMix = 0.0f;
		state.cct = 5600.0f;
		program->looks.push_back({id, state});
	} else if (!included && it != program->looks.end()) {
		program->looks.erase(it);
	}

	show_.updateProgram(*program);
	updateSwatches();
	pushPreview();
}

void ProgramsPage::onFixtureSelectionChanged()
{
	if (!populating_)
		loadStateIntoControls();
}

void ProgramsPage::loadStateIntoControls()
{
	Program *program = currentProgram();
	if (!program)
		return;

	// On affiche l'etat du premier projecteur selectionne qui appartient au
	// programme : montrer une moyenne de plusieurs etats serait trompeur.
	const LightState *reference = nullptr;
	for (QListWidgetItem *item : fixtures_->selectedItems()) {
		const std::string id = item->data(kFixtureIdRole).toString().toStdString();
		if (const LightState *state = program->lookFor(id)) {
			reference = state;
			break;
		}
	}

	const LightState state = reference ? *reference : LightState();

	populating_ = true;
	intensity_->setValueSilently(state.intensity * 100.0f);
	colorMix_->setValueSilently(state.colorMix * 100.0f);
	hue_->setValueSilently(state.hue);
	saturation_->setValueSilently(state.saturation * 100.0f);
	cct_->setValueSilently(state.cct);
	greenMagenta_->setValueSilently(state.greenMagenta * 100.0f);
	strobe_->setValueSilently(state.strobeHz);
	populating_ = false;
}

void ProgramsPage::onLightChanged()
{
	if (populating_)
		return;
	applyControlsToSelection();
}

void ProgramsPage::applyControlsToSelection()
{
	Program *program = currentProgram();
	if (!program)
		return;

	LightState state;
	state.intensity = intensity_->value() / 100.0f;
	state.colorMix = colorMix_->value() / 100.0f;
	state.hue = hue_->value();
	state.saturation = saturation_->value() / 100.0f;
	state.cct = cct_->value();
	state.greenMagenta = greenMagenta_->value() / 100.0f;
	state.strobeHz = strobe_->value();

	// Le reglage s'applique a tous les projecteurs selectionnes : c'est ce
	// qui permet de colorer six PAR d'un geste.
	for (QListWidgetItem *item : fixtures_->selectedItems()) {
		const std::string id = item->data(kFixtureIdRole).toString().toStdString();

		const auto it = std::find_if(program->looks.begin(), program->looks.end(),
					     [&id](const FixtureLook &look) { return look.fixtureId == id; });
		if (it != program->looks.end()) {
			it->state = state;
			continue;
		}

		// Regler un projecteur non coche l'ajoute au programme : c'est ce
		// que l'utilisateur vient de demander implicitement.
		program->looks.push_back({id, state});
		const QSignalBlocker blocker(fixtures_);
		item->setCheckState(Qt::Checked);
	}

	show_.updateProgram(*program);
	updateSwatches();
	pushPreview();
}

Effect *ProgramsPage::currentEffect()
{
	Program *program = currentProgram();
	const int row = effects_ ? effects_->currentRow() : -1;
	if (!program || row < 0 || row >= static_cast<int>(program->effects.size()))
		return nullptr;
	return &program->effects[static_cast<size_t>(row)];
}

void ProgramsPage::refreshEffectList()
{
	const QSignalBlocker blocker(effects_);
	const int previous = effects_->currentRow();
	effects_->clear();

	if (Program *program = currentProgram())
		for (const auto &effect : program->effects) {
			auto *item = new QListWidgetItem(QString::fromStdString(effect.name), effects_);
			item->setData(kEffectIdRole, QString::fromStdString(effect.id));
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
			// La case a cocher allume ou eteint l'effet sans le supprimer :
			// c'est le geste courant pendant un spectacle.
			item->setCheckState(effect.enabled ? Qt::Checked : Qt::Unchecked);
		}

	if (previous >= 0 && previous < effects_->count())
		effects_->setCurrentRow(previous);
	else if (effects_->count() > 0)
		effects_->setCurrentRow(0);

	onEffectSelected();
}

void ProgramsPage::onEffectSelected()
{
	Effect *effect = currentEffect();
	removeEffectButton_->setEnabled(effect != nullptr);
	effectEditor_->setEffect(effect);
}

void ProgramsPage::addEffect()
{
	Program *program = currentProgram();
	if (!program)
		return;

	QMenu menu(this);
	const struct {
		const char *key;
		EffectType type;
	} kinds[] = {
		{"Effect.Type.Chaser", EffectType::Chaser},
		{"Effect.Type.Strobe", EffectType::Strobe},
		{"Effect.Type.Sound", EffectType::Sound},
		{"Effect.Type.Builtin", EffectType::BuiltinFx},
	};

	for (const auto &kind : kinds) {
		QAction *action = menu.addAction(tr_(kind.key));
		action->setData(static_cast<int>(kind.type));
	}

	QAction *chosen = menu.exec(QCursor::pos());
	if (!chosen)
		return;

	Effect effect;
	effect.type = static_cast<EffectType>(chosen->data().toInt());
	effect.name = chosen->text().toStdString();
	effect.id = "effect-" + std::to_string(++effectIdCounter_);

	// Un effet sonore module la lumiere du programme : il doit pouvoir la
	// baisser, ce que « le plus lumineux gagne » interdit par construction.
	if (effect.type == EffectType::Sound)
		effect.blend = BlendMode::Replace;

	// Un chaser sans pas ne fait rien : on en donne deux, de quoi voir
	// immediatement quelque chose bouger.
	if (effect.type == EffectType::Chaser) {
		LightState on;
		on.intensity = 1.0f;
		on.colorMix = 0.0f;
		on.cct = 5600.0f;
		LightState off = on;
		off.intensity = 0.0f;
		effect.chaser.steps = {on, off};
	}

	program->effects.push_back(std::move(effect));
	show_.updateProgram(*program);

	refreshEffectList();
	effects_->setCurrentRow(effects_->count() - 1);
	pushPreview();
}

void ProgramsPage::removeEffect()
{
	Program *program = currentProgram();
	const int row = effects_->currentRow();
	if (!program || row < 0 || row >= static_cast<int>(program->effects.size()))
		return;

	program->effects.erase(program->effects.begin() + row);
	show_.updateProgram(*program);
	refreshEffectList();
	pushPreview();
}

void ProgramsPage::onEffectToggled(QListWidgetItem *item)
{
	Program *program = currentProgram();
	if (!program || !item)
		return;

	const int row = effects_->row(item);
	if (row < 0 || row >= static_cast<int>(program->effects.size()))
		return;

	program->effects[static_cast<size_t>(row)].enabled = item->checkState() == Qt::Checked;
	show_.updateProgram(*program);

	// L'editeur affiche peut-etre cet effet : le tenir a jour.
	if (row == effects_->currentRow())
		effectEditor_->setEffect(&program->effects[static_cast<size_t>(row)]);
	pushPreview();
}

void ProgramsPage::onEffectChanged(const Effect &effect)
{
	Program *program = currentProgram();
	const int row = effects_->currentRow();
	if (!program || row < 0 || row >= static_cast<int>(program->effects.size()))
		return;

	program->effects[static_cast<size_t>(row)] = effect;
	show_.updateProgram(*program);

	const QSignalBlocker blocker(effects_);
	effects_->item(row)->setText(QString::fromStdString(effect.name));
	effects_->item(row)->setCheckState(effect.enabled ? Qt::Checked : Qt::Unchecked);

	pushPreview();
}

void ProgramsPage::updateSwatches()
{
	Program *program = currentProgram();

	// setIcon() emet itemChanged, qui ramene ici : sans ce blocage, la pile
	// deborde et OBS s'arrete net.
	const QSignalBlocker blocker(fixtures_);

	for (int i = 0; i < fixtures_->count(); ++i) {
		QListWidgetItem *item = fixtures_->item(i);
		const std::string id = item->data(kFixtureIdRole).toString().toStdString();

		const LightState *state = program ? program->lookFor(id) : nullptr;
		item->setIcon(state ? swatchIcon(swatchFor(*state)) : QIcon());
	}
}

void ProgramsPage::pushPreview()
{
	// L'apercu n'a de sens que quand l'onglet est sous les yeux.
	if (!isVisible()) {
		show_.setPreview(std::nullopt);
		return;
	}

	if (Program *program = currentProgram())
		show_.setPreview(*program);
	else
		show_.setPreview(std::nullopt);
}

} // namespace obsdmx

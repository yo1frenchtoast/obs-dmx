#include "ui/dmx-dock.h"

#include "core/dmx-engine.h"
#include "core/show.h"
#include "ui/output-settings.h"
#include "ui/patch-page.h"
#include "ui/programs-page.h"

#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include <obs-module.h>

namespace obsdmx {

namespace {

QString tr_(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

} // namespace

DmxDock::DmxDock(DmxEngine &engine, Show &show, const FixtureLibrary &library, QWidget *parent)
	: QWidget(parent), engine_(engine), show_(show)
{
	auto *layout = new QVBoxLayout(this);

	patchPage_ = new PatchPage(show_, library, this);
	programsPage_ = new ProgramsPage(show_, this);
	outputPage_ = new OutputSettingsPage(engine_, this);

	tabs_ = new QTabWidget(this);
	tabs_->addTab(patchPage_, tr_("Tab.Fixtures"));
	tabs_->addTab(programsPage_, tr_("Tab.Programs"));
	tabs_->addTab(outputPage_, tr_("Tab.Output"));
	layout->addWidget(tabs_);

	blackoutButton_ = new QPushButton(tr_("Blackout"), this);
	blackoutButton_->setCheckable(true);
	connect(blackoutButton_, &QPushButton::clicked, this, &DmxDock::toggleBlackout);
	layout->addWidget(blackoutButton_);

	// Ajouter ou deplacer un projecteur change la liste que voit l'editeur
	// de programmes.
	connect(patchPage_, &PatchPage::patchChanged, programsPage_, &ProgramsPage::reloadFixtures);

	// Le rendu tourne sur le thread du moteur. Le spectacle prend son propre
	// verrou ; le banc d'essai passe par des atomiques. Aucun widget n'est
	// touche ici.
	engine_.setRenderFn([this](std::vector<Universe> &universes, std::chrono::steady_clock::time_point now) {
		show_.render(universes, now);
		outputPage_->renderTest(universes[0]);
	});
}

DmxDock::~DmxDock()
{
	// Le moteur survit au dock : on retire le rendu avant que les pages ne
	// disparaissent, sinon le thread appellerait des objets detruits.
	engine_.setRenderFn(nullptr);
}

void DmxDock::reloadFromShow()
{
	patchPage_->reload();
	programsPage_->reload();
}

void DmxDock::setBlackout(bool on)
{
	blackoutButton_->setChecked(on);
	engine_.setBlackout(on);
}

void DmxDock::toggleBlackout()
{
	engine_.setBlackout(blackoutButton_->isChecked());
}

} // namespace obsdmx

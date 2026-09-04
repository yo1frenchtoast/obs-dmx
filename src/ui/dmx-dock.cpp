#include "ui/dmx-dock.h"

#include "core/dmx-engine.h"
#include "audio/obs-audio-tap.h"
#include "core/show.h"
#include "ui/output-settings.h"
#include "ui/patch-page.h"
#include "ui/programs-page.h"

#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

#include <obs-module.h>

namespace obsdmx {

namespace {

QString tr_(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

/// Rend une page defilante.
///
/// Le dock est etroit et haut, et son contenu depasse souvent la hauteur
/// disponible. Sans cela, Qt comprime tout : les tableaux sont ecrases a rien
/// et les textes explicatifs sont coupes en plein milieu.
QScrollArea *scrollable(QWidget *page, QWidget *parent)
{
	auto *area = new QScrollArea(parent);
	area->setWidget(page);
	// La page suit la largeur du dock ; seule la hauteur defile.
	area->setWidgetResizable(true);
	// Jamais AlwaysOff : si un contenu depasse malgre tout en largeur, il
	// serait coupe sans aucun moyen d'y acceder.
	area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	area->setFrameShape(QFrame::NoFrame);
	return area;
}

} // namespace

DmxDock::DmxDock(DmxEngine &engine, Show &show, const FixtureLibrary &library, ObsAudioTap &audio,
		 QWidget *parent)
	: QWidget(parent), engine_(engine), show_(show), audio_(audio)
{
	auto *layout = new QVBoxLayout(this);

	patchPage_ = new PatchPage(show_, library, this);
	programsPage_ = new ProgramsPage(show_, [this] { return audio_.snapshot(); }, this);
	outputPage_ = new OutputSettingsPage(engine_, this);

	tabs_ = new QTabWidget(this);
	tabs_->addTab(scrollable(patchPage_, this), tr_("Tab.Fixtures"));
	tabs_->addTab(scrollable(programsPage_, this), tr_("Tab.Programs"));
	tabs_->addTab(scrollable(outputPage_, this), tr_("Tab.Output"));
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
		show_.render(universes, now, audio_.snapshot());
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

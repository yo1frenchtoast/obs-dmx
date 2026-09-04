#include "ui/dmx-dock.h"
#include "ui/localized.h"

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

/// Makes a page scrollable.
///
/// The dock is narrow and tall, and its content often exceeds the height
/// available. Without this Qt squeezes everything: tables are crushed to nothing
/// and explanatory text is cut off mid-sentence.
QScrollArea *scrollable(QWidget *page, QWidget *parent)
{
	auto *area = new QScrollArea(parent);
	area->setWidget(page);
	// The page follows the dock's width; only the height scrolls.
	area->setWidgetResizable(true);
	// Never AlwaysOff: if something still overflows horizontally it would be
	// clipped with no way to reach it.
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

	// Adding or moving a fixture changes the list the programme editor
	// shows.
	connect(patchPage_, &PatchPage::patchChanged, programsPage_, &ProgramsPage::reloadFixtures);

	// The render runs on the engine thread. The show takes its own lock; the
	// test bench goes through atomics. No widget is touched here.
	engine_.setRenderFn([this](std::vector<Universe> &universes, std::chrono::steady_clock::time_point now) {
		show_.render(universes, now, audio_.snapshot());
		outputPage_->renderTest(universes[0]);
	});
}

DmxDock::~DmxDock()
{
	// The engine outlives the dock: drop the render before the pages go away,
	// otherwise the thread would call into destroyed objects.
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

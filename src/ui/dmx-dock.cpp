#include "ui/dmx-dock.h"

#include "core/dmx-engine.h"
#include "ui/output-settings.h"

#include <QLabel>
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

DmxDock::DmxDock(DmxEngine &engine, QWidget *parent) : QWidget(parent), engine_(engine)
{
	auto *layout = new QVBoxLayout(this);

	outputPage_ = new OutputSettingsPage(engine_, this);

	auto *tabs = new QTabWidget(this);
	tabs->addTab(buildPlaceholderPage(tr_("Tab.Fixtures"), tr_("Placeholder.Fixtures")), tr_("Tab.Fixtures"));
	tabs->addTab(buildPlaceholderPage(tr_("Tab.Programs"), tr_("Placeholder.Programs")), tr_("Tab.Programs"));
	tabs->addTab(outputPage_, tr_("Tab.Output"));
	layout->addWidget(tabs);

	blackoutButton_ = new QPushButton(tr_("Blackout"), this);
	blackoutButton_->setCheckable(true);
	connect(blackoutButton_, &QPushButton::clicked, this, &DmxDock::toggleBlackout);
	layout->addWidget(blackoutButton_);

	// Le rendu tourne sur le thread du moteur : il ne touche que des
	// atomiques, jamais un widget.
	engine_.setRenderFn([this](std::vector<Universe> &universes, std::chrono::steady_clock::time_point) {
		outputPage_->renderTest(universes[0]);
	});
}

DmxDock::~DmxDock()
{
	// Le moteur survit au dock : on retire le rendu avant que outputPage_ ne
	// disparaisse, sinon le thread appellerait un objet detruit.
	engine_.setRenderFn(nullptr);
}

QWidget *DmxDock::buildPlaceholderPage(const QString &title, const QString &explanation)
{
	auto *page = new QWidget(this);
	auto *layout = new QVBoxLayout(page);

	auto *heading = new QLabel(title, page);
	auto font = heading->font();
	font.setBold(true);
	heading->setFont(font);
	layout->addWidget(heading);

	auto *body = new QLabel(explanation, page);
	body->setWordWrap(true);
	layout->addWidget(body);

	layout->addStretch();
	return page;
}

void DmxDock::toggleBlackout()
{
	engine_.setBlackout(blackoutButton_->isChecked());
}

} // namespace obsdmx

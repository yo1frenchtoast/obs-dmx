#include "ui/dmx-dock.h"

#include "core/dmx-engine.h"

#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <obs-module.h>

namespace obsdmx {

namespace {
/// Intervalle de rafraichissement de l'affichage. Volontairement lent : c'est
/// de l'information de confort, pas du temps reel.
constexpr int kStatusIntervalMs = 500;
} // namespace

DmxDock::DmxDock(DmxEngine &engine, QWidget *parent) : QWidget(parent), engine_(engine)
{
	auto *layout = new QVBoxLayout(this);

	auto *tabs = new QTabWidget(this);
	tabs->addTab(buildPlaceholderPage(QString::fromUtf8(obs_module_text("Tab.Fixtures")),
					  QString::fromUtf8(obs_module_text("Placeholder.Fixtures"))),
		     QString::fromUtf8(obs_module_text("Tab.Fixtures")));
	tabs->addTab(buildPlaceholderPage(QString::fromUtf8(obs_module_text("Tab.Programs")),
					  QString::fromUtf8(obs_module_text("Placeholder.Programs"))),
		     QString::fromUtf8(obs_module_text("Tab.Programs")));
	tabs->addTab(buildOutputPage(), QString::fromUtf8(obs_module_text("Tab.Output")));
	layout->addWidget(tabs);

	blackoutButton_ = new QPushButton(QString::fromUtf8(obs_module_text("Blackout")), this);
	blackoutButton_->setCheckable(true);
	connect(blackoutButton_, &QPushButton::clicked, this, &DmxDock::toggleBlackout);
	layout->addWidget(blackoutButton_);

	statusTimer_ = new QTimer(this);
	connect(statusTimer_, &QTimer::timeout, this, &DmxDock::refreshStatus);
	statusTimer_->start(kStatusIntervalMs);

	refreshStatus();
}

QWidget *DmxDock::buildOutputPage()
{
	auto *page = new QWidget(this);
	auto *layout = new QVBoxLayout(page);

	statusLabel_ = new QLabel(page);
	statusLabel_->setTextFormat(Qt::PlainText);
	layout->addWidget(statusLabel_);

	auto *note = new QLabel(QString::fromUtf8(obs_module_text("Placeholder.Output")), page);
	note->setWordWrap(true);
	layout->addWidget(note);

	layout->addStretch();
	return page;
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

void DmxDock::refreshStatus()
{
	const uint64_t frames = engine_.framesSent();
	// Trames emises depuis le dernier rafraichissement, ramenees a une cadence.
	const double rate = static_cast<double>(frames - lastFrames_) * 1000.0 / kStatusIntervalMs;
	lastFrames_ = frames;

	statusLabel_->setText(QString::fromUtf8(obs_module_text("Status.Engine"))
				      .arg(engine_.running() ? QString::fromUtf8(obs_module_text("Status.Running"))
							     : QString::fromUtf8(obs_module_text("Status.Stopped")))
				      .arg(rate, 0, 'f', 1)
				      .arg(engine_.universeCount()));
}

void DmxDock::toggleBlackout()
{
	engine_.setBlackout(blackoutButton_->isChecked());
}

} // namespace obsdmx

#pragma once

#include "core/effect.h"

#include <QPainter>
#include <QTimer>
#include <QWidget>

#include <functional>

namespace obsdmx {

/// Trois barres montrant ce que l'analyse entend, plus un temoin de temps.
///
/// Sans cela, regler la sensibilite revient a tourner un bouton en esperant :
/// l'utilisateur doit voir ce que la machine entend.
class LevelMeter : public QWidget {
	Q_OBJECT

public:
	explicit LevelMeter(std::function<AudioSnapshot()> provider, QWidget *parent = nullptr)
		: QWidget(parent), provider_(std::move(provider))
	{
		setMinimumHeight(48);
		auto *timer = new QTimer(this);
		connect(timer, &QTimer::timeout, this, [this] {
			const AudioSnapshot snap = provider_();
			if (snap.beatCount != lastBeat_) {
				lastBeat_ = snap.beatCount;
				beatFlash_ = 4; // quelques rafraichissements
			} else if (beatFlash_ > 0) {
				--beatFlash_;
			}
			snapshot_ = snap;
			update();
		});
		timer->start(50);
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);

		const QStringList labels{tr("Grave"), tr("Médium"), tr("Aigu")};
		const int barHeight = 10;
		const int spacing = 4;
		const int labelWidth = 56;

		for (int i = 0; i < 3; ++i) {
			const int y = i * (barHeight + spacing);
			painter.setPen(palette().color(QPalette::WindowText));
			painter.drawText(QRect(0, y, labelWidth, barHeight), Qt::AlignLeft | Qt::AlignVCenter,
					 labels[i]);

			const QRect track(labelWidth, y, width() - labelWidth - 20, barHeight);
			painter.setPen(Qt::NoPen);
			painter.setBrush(palette().color(QPalette::Base));
			painter.drawRoundedRect(track, 3, 3);

			QRect filled = track;
			filled.setWidth(int(track.width() * qBound(0.0f, snapshot_.bands[i], 1.0f)));
			painter.setBrush(palette().color(QPalette::Highlight));
			painter.drawRoundedRect(filled, 3, 3);
		}

		// Temoin de temps, a droite.
		const QRect beat(width() - 14, 0, 12, 12);
		painter.setBrush(beatFlash_ > 0 ? palette().color(QPalette::Highlight)
						: palette().color(QPalette::Base));
		painter.drawEllipse(beat);
	}

private:
	std::function<AudioSnapshot()> provider_;
	AudioSnapshot snapshot_;
	uint64_t lastBeat_ = 0;
	int beatFlash_ = 0;
};

} // namespace obsdmx

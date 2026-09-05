#pragma once

#include "ui/audio-access.h"
#include "ui/localized.h"
#include "ui/slider-row.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>

namespace obsdmx {

/// Three bars showing what the analysis hears, plus a beat indicator.
///
/// Without it, setting the sensitivity means turning a knob and hoping: the user
/// has to see what the machine hears.
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
				beatFlash_ = 4; // a few refreshes, so the eye catches it
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

		const QStringList labels{tr_("Effect.Sound.Band.Low"), tr_("Effect.Sound.Band.Mid"),
					 tr_("Effect.Sound.Band.High")};
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

		// Beat indicator, on the right.
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

/// The level meter with the one knob that governs beat detection.
///
/// Detection is done once, for the whole plugin, so this setting is global: two
/// effects cannot disagree about what a beat is. It appears wherever beats are
/// used -- the sound effect and a chase driven by the beat -- and says as much,
/// so nobody takes it for a per-effect setting.
class BeatTuning : public QWidget {
	Q_OBJECT

public:
	explicit BeatTuning(AudioAccess audio, QWidget *parent = nullptr)
		: QWidget(parent), audio_(std::move(audio))
	{
		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(0, 0, 0, 0);

		auto *hint = new QLabel(tr_("Effect.Sound.Meter.Hint"), this);
		hint->setWordWrap(true);
		layout->addWidget(hint);

		layout->addWidget(new LevelMeter(audio_.snapshot, this));

		auto *row = new QHBoxLayout();
		auto *label = new QLabel(tr_("Beat.Sensitivity"), this);
		row->addWidget(label);

		slider_ = new SliderRow(0.0f, 100.0f, 100, " %", 0, this);
		slider_->setToolTip(tr_("Beat.Sensitivity.Hint"));
		row->addWidget(slider_, 1);
		layout->addLayout(row);

		auto *note = new QLabel(tr_("Beat.Sensitivity.Global"), this);
		note->setWordWrap(true);
		layout->addWidget(note);

		slider_->setValueSilently(toPercent(audio_.beatSensitivity()));
		connect(slider_, &SliderRow::valueChanged, this,
			[this](float percent) { audio_.setBeatSensitivity(toFactor(percent)); });
	}

private:
	/// The engine wants a threshold factor, the user wants a sensitivity: more
	/// means more beats, so the two run opposite ways.
	static constexpr float kStrictest = 3.0f;
	static constexpr float kMostSensitive = 0.5f;

	static float toFactor(float percent)
	{
		return kStrictest - (percent / 100.0f) * (kStrictest - kMostSensitive);
	}

	static float toPercent(float factor)
	{
		return (kStrictest - factor) / (kStrictest - kMostSensitive) * 100.0f;
	}

	SliderRow *slider_ = nullptr;
	AudioAccess audio_;
};

} // namespace obsdmx

#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QWidget>

#include <functional>

namespace obsdmx {

/// Un curseur avec sa valeur lisible a cote.
///
/// Les curseurs Qt sont entiers ; les grandeurs manipulees ici sont continues.
/// Cette classe fait la conversion une fois pour toutes, plutot que de la
/// repeter dans chaque page.
class SliderRow : public QWidget {
	Q_OBJECT

public:
	/// steps : finesse du curseur. suffix et decimals servent a l'affichage.
	SliderRow(float minimum, float maximum, int steps, QString suffix, int decimals, QWidget *parent = nullptr)
		: QWidget(parent), minimum_(minimum), maximum_(maximum), suffix_(std::move(suffix)),
		  decimals_(decimals)
	{
		auto *layout = new QHBoxLayout(this);
		layout->setContentsMargins(0, 0, 0, 0);

		slider_ = new QSlider(Qt::Horizontal, this);
		slider_->setRange(0, steps);
		layout->addWidget(slider_, 1);

		readout_ = new QLabel(this);
		readout_->setMinimumWidth(58);
		readout_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		layout->addWidget(readout_);

		connect(slider_, &QSlider::valueChanged, this, [this](int) {
			updateReadout();
			if (!blocked_)
				emit valueChanged(value());
		});
		updateReadout();
	}

	float value() const
	{
		const float t = static_cast<float>(slider_->value()) / static_cast<float>(slider_->maximum());
		return minimum_ + t * (maximum_ - minimum_);
	}

	/// Positionne le curseur sans emettre de signal : utile pour recharger un
	/// formulaire sans declencher une sauvegarde en boucle.
	void setValueSilently(float value)
	{
		const float t = (value - minimum_) / (maximum_ - minimum_);
		blocked_ = true;
		slider_->setValue(static_cast<int>(std::lround(t * slider_->maximum())));
		blocked_ = false;
		updateReadout();
	}

	/// Peint un degrade sous le curseur, pour que la teinte se choisisse a l'oeil.
	void setGradient(const QString &css) { slider_->setStyleSheet(css); }

signals:
	void valueChanged(float value);

private:
	void updateReadout() { readout_->setText(QString::number(value(), 'f', decimals_) + suffix_); }

	QSlider *slider_ = nullptr;
	QLabel *readout_ = nullptr;
	float minimum_;
	float maximum_;
	QString suffix_;
	int decimals_;
	bool blocked_ = false;
};

} // namespace obsdmx

#pragma once

#include <QDialog>

#include <string>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QSpinBox;

namespace obsdmx {

class FixtureLibrary;
class Show;

/// Dialog for adding a fixture: pick a model, a mode and an address.
///
/// Choosing the mode is the single biggest trap in the whole plugin: it must
/// match what is set on the fixture's own screen, and nothing here can verify
/// it. The dialog therefore says so outright.
class AddFixtureDialog : public QDialog {
	Q_OBJECT

public:
	AddFixtureDialog(const Show &show, const FixtureLibrary &library, QWidget *parent = nullptr);

	std::string profileId() const;
	std::string modeId() const;
	std::string fixtureName() const;
	int address() const;
	uint16_t universe() const;
	int quantity() const;

private slots:
	void onSearchChanged(const QString &text);
	void onProfileChanged();
	void onModeChanged();

private:
	void suggestAddress();

	const Show &show_;
	const FixtureLibrary &library_;

	QLineEdit *search_ = nullptr;
	QListWidget *profiles_ = nullptr;
	QComboBox *modes_ = nullptr;
	QLabel *modeWarning_ = nullptr;
	QLabel *profileNote_ = nullptr;
	QLabel *footprint_ = nullptr;
	QLineEdit *name_ = nullptr;
	QSpinBox *universe_ = nullptr;
	QSpinBox *address_ = nullptr;
	QSpinBox *quantity_ = nullptr;
};

} // namespace obsdmx

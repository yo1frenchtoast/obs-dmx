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

/// Boite d'ajout d'un projecteur : on choisit un modele, un mode, une adresse.
///
/// Le choix du mode est le piege principal de tout le plugin : il doit
/// correspondre au reglage fait sur l'ecran de l'appareil, et rien ne permet
/// de le verifier depuis le logiciel. L'ecran le dit donc explicitement.
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
	QLabel *footprint_ = nullptr;
	QLineEdit *name_ = nullptr;
	QSpinBox *universe_ = nullptr;
	QSpinBox *address_ = nullptr;
	QSpinBox *quantity_ = nullptr;
};

} // namespace obsdmx

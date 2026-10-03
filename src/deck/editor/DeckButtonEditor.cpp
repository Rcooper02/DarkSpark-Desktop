// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/editor/DeckButtonEditor.hpp"

#include "deck/editor/InstalledAppPicker.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace darkspark::deck::editor {

DeckButtonEditor::DeckButtonEditor(
    const models::DeckButtonConfig& config,
    QWidget* parent)
    : QDialog(parent),
      titleEdit_(new QLineEdit(config.title, this)),
      actionType_(new QComboBox(this)),
      targetEdit_(new QLineEdit(config.target, this)),
      iconEdit_(new QLineEdit(config.iconPath, this)),
      browseProgram_(new QPushButton(
          QStringLiteral("Browse Apps..."), this)),
      browseIcon_(new QPushButton(
          QStringLiteral("Browse Icon..."), this)),
      form_(new QFormLayout()),
      targetRow_(new QWidget(this)),
      iconRow_(new QWidget(this)),
      slot_(config.slot) {

    setWindowTitle(QStringLiteral("Edit Control Deck Button"));
    setModal(true);
    setMinimumWidth(620);

    auto* root = new QVBoxLayout(this);

    auto* heading = new QLabel(
        QStringLiteral("BUTTON %1").arg(slot_ + 1),
        this);

    QFont headingFont = heading->font();
    headingFont.setBold(true);
    headingFont.setPointSize(14);
    heading->setFont(headingFont);

    root->addWidget(heading);

    actionType_->addItem(
        QStringLiteral("Built-in Control"),
        QStringLiteral("control"));

    actionType_->addItem(
        QStringLiteral("Launch Application"),
        QStringLiteral("launch_app"));

    actionType_->addItem(
        QStringLiteral("Open URL"),
        QStringLiteral("url"));

    actionType_->addItem(
        QStringLiteral("Run Command"),
        QStringLiteral("command"));

    actionType_->addItem(
        QStringLiteral("Open DarkSpark Page"),
        QStringLiteral("page"));

    const int actionIndex =
        actionType_->findData(config.actionType);

    if (actionIndex >= 0)
        actionType_->setCurrentIndex(actionIndex);

    form_->addRow(
        QStringLiteral("Title"),
        titleEdit_);

    form_->addRow(
        QStringLiteral("Action"),
        actionType_);

    auto* targetLayout = new QHBoxLayout(targetRow_);
    targetLayout->setContentsMargins(0, 0, 0, 0);
    targetLayout->setSpacing(8);

    targetLayout->addWidget(targetEdit_, 1);
    targetLayout->addWidget(browseProgram_);

    form_->addRow(
        QStringLiteral("Program"),
        targetRow_);

    auto* iconLayout = new QHBoxLayout(iconRow_);
    iconLayout->setContentsMargins(0, 0, 0, 0);
    iconLayout->setSpacing(8);

    iconLayout->addWidget(iconEdit_, 1);
    iconLayout->addWidget(browseIcon_);

    form_->addRow(
        QStringLiteral("Icon"),
        iconRow_);

    root->addLayout(form_);

    connect(
        browseProgram_,
        &QPushButton::clicked,
        this,
        [this]() {
            InstalledAppPicker picker(this);

            if (picker.exec() != QDialog::Accepted)
                return;

            const InstalledApp app =
                picker.selectedApp();

            if (app.command.isEmpty())
                return;

            targetEdit_->setText(app.command);

            // Picking an app should normally set its proper display name.
            titleEdit_->setText(app.name);

            // Also adopt the application's icon automatically.
            if (!app.icon.isEmpty())
                iconEdit_->setText(app.icon);
        });

    connect(
        browseIcon_,
        &QPushButton::clicked,
        this,
        [this]() {
            const QString path =
                QFileDialog::getOpenFileName(
                    this,
                    QStringLiteral("Choose Button Icon"),
                    iconEdit_->text(),
                    QStringLiteral(
                        "Images (*.png *.jpg *.jpeg *.svg *.gif *.webp);;"
                        "All Files (*)"));

            if (!path.isEmpty())
                iconEdit_->setText(path);
        });

    connect(
        actionType_,
        &QComboBox::currentIndexChanged,
        this,
        [this](int) {
            updateActionUi();
        });

    auto* buttons = new QDialogButtonBox(this);

    auto* saveButton =
        buttons->addButton(
            QStringLiteral("Save"),
            QDialogButtonBox::AcceptRole);

    auto* clearButton =
        buttons->addButton(
            QStringLiteral("Clear Slot"),
            QDialogButtonBox::DestructiveRole);

    auto* cancelButton =
        buttons->addButton(
            QDialogButtonBox::Cancel);

    connect(
        saveButton,
        &QPushButton::clicked,
        this,
        &QDialog::accept);

    connect(
        clearButton,
        &QPushButton::clicked,
        this,
        [this]() {
            clearRequested_ = true;
            accept();
        });

    connect(
        cancelButton,
        &QPushButton::clicked,
        this,
        &QDialog::reject);

    root->addWidget(buttons);

    updateActionUi();
}

void DeckButtonEditor::updateActionUi() {
    const QString type =
        actionType_->currentData().toString();

    const bool launchApp =
        type == QStringLiteral("launch_app");

    browseProgram_->setVisible(launchApp);

    if (launchApp) {
        form_->setWidget(
            2,
            QFormLayout::LabelRole,
            new QLabel(QStringLiteral("Program"), this));

        targetEdit_->setPlaceholderText(
            QStringLiteral("Choose an installed application"));
    } else if (type == QStringLiteral("url")) {
        form_->setWidget(
            2,
            QFormLayout::LabelRole,
            new QLabel(QStringLiteral("URL"), this));

        targetEdit_->setPlaceholderText(
            QStringLiteral("https://example.com"));
    } else if (type == QStringLiteral("command")) {
        form_->setWidget(
            2,
            QFormLayout::LabelRole,
            new QLabel(QStringLiteral("Command"), this));

        targetEdit_->setPlaceholderText(
            QStringLiteral("program --argument value"));
    } else if (type == QStringLiteral("page")) {
        form_->setWidget(
            2,
            QFormLayout::LabelRole,
            new QLabel(QStringLiteral("Page"), this));

        targetEdit_->setPlaceholderText(
            QStringLiteral("Command, System, Control Deck..."));
    } else {
        form_->setWidget(
            2,
            QFormLayout::LabelRole,
            new QLabel(QStringLiteral("Action ID"), this));

        targetEdit_->setPlaceholderText(
            QStringLiteral("Built-in control action"));
    }
}

models::DeckButtonConfig DeckButtonEditor::config() const {
    models::DeckButtonConfig result;

    result.slot = slot_;
    result.title =
        titleEdit_->text().trimmed();

    result.actionType =
        actionType_->currentData().toString();

    result.target =
        targetEdit_->text().trimmed();

    result.iconPath =
        iconEdit_->text().trimmed();

    return result;
}

}  // namespace darkspark::deck::editor

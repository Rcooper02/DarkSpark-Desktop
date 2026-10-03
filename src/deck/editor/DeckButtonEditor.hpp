// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_EDITOR_DECKBUTTONEDITOR_HPP
#define DARKSPARK_DECK_EDITOR_DECKBUTTONEDITOR_HPP

#include "models/DeckButtonConfig.hpp"

#include <QDialog>

class QComboBox;
class QFormLayout;
class QLineEdit;
class QPushButton;

namespace darkspark::deck::editor {

class DeckButtonEditor final : public QDialog {
    Q_OBJECT

public:
    explicit DeckButtonEditor(
        const models::DeckButtonConfig& config,
        QWidget* parent = nullptr);

    [[nodiscard]] models::DeckButtonConfig config() const;
    [[nodiscard]] bool clearRequested() const {
        return clearRequested_;
    }

private:
    void updateActionUi();

    QLineEdit* titleEdit_;
    QComboBox* actionType_;
    QLineEdit* targetEdit_;
    QLineEdit* iconEdit_;

    QPushButton* browseProgram_;
    QPushButton* browseIcon_;

    QFormLayout* form_;
    QWidget* targetRow_;
    QWidget* iconRow_;

    int slot_;
    bool clearRequested_ = false;
};

}  // namespace darkspark::deck::editor

#endif

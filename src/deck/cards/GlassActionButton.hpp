// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_CARDS_GLASSACTIONBUTTON_HPP
#define DARKSPARK_DECK_CARDS_GLASSACTIONBUTTON_HPP

#include <QPushButton>

namespace darkspark::deck::cards {

/// Touch button painted as a raised glass key with physical press travel.
class GlassActionButton final : public QPushButton {
    Q_OBJECT

public:
    explicit GlassActionButton(const QString& text, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

}  // namespace darkspark::deck::cards

#endif  // DARKSPARK_DECK_CARDS_GLASSACTIONBUTTON_HPP

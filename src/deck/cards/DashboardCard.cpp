// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/cards/DashboardCard.hpp"

#include "deck/cards/StatusIndicator.hpp"
#include "themes/LegacyTheme.hpp"

#include <QColor>
#include <QFocusEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QStyle>
#include <QVBoxLayout>

#include <array>

namespace darkspark::deck::cards {

using themes::LegacyTheme;

namespace {

StatusIndicator::State toIndicatorState(DashboardCard::State state) {
    switch (state) {
    case DashboardCard::State::Normal:
        return StatusIndicator::State::Normal;
    case DashboardCard::State::Loading:
        return StatusIndicator::State::Loading;
    case DashboardCard::State::Empty:
        return StatusIndicator::State::Empty;
    case DashboardCard::State::Unavailable:
        return StatusIndicator::State::Unavailable;
    case DashboardCard::State::Warning:
        return StatusIndicator::State::Warning;
    case DashboardCard::State::Critical:
        return StatusIndicator::State::Critical;
    case DashboardCard::State::Error:
        return StatusIndicator::State::Error;
    case DashboardCard::State::Disabled:
        return StatusIndicator::State::Disabled;
    }
    return StatusIndicator::State::Normal;
}

const char* stateStatusText(DashboardCard::State state) {
    switch (state) {
    case DashboardCard::State::Normal:
        return "Placeholder";
    case DashboardCard::State::Loading:
        return "Loading";
    case DashboardCard::State::Empty:
        return "No content";
    case DashboardCard::State::Unavailable:
        return "Unavailable";
    case DashboardCard::State::Warning:
        return "Warning";
    case DashboardCard::State::Critical:
        return "Critical";
    case DashboardCard::State::Error:
        return "Error";
    case DashboardCard::State::Disabled:
        return "Disabled";
    }
    return "Placeholder";
}

const char* stateName(DashboardCard::State state) {
    switch (state) {
    case DashboardCard::State::Normal:
        return "normal";
    case DashboardCard::State::Loading:
        return "loading";
    case DashboardCard::State::Empty:
        return "empty";
    case DashboardCard::State::Unavailable:
        return "unavailable";
    case DashboardCard::State::Warning:
        return "warning";
    case DashboardCard::State::Critical:
        return "critical";
    case DashboardCard::State::Error:
        return "error";
    case DashboardCard::State::Disabled:
        return "disabled";
    }
    return "normal";
}

const char* accentName(DashboardCard::Accent accent) {
    switch (accent) {
    case DashboardCard::Accent::None:
        return "none";
    case DashboardCard::Accent::Cyan:
        return "cyan";
    case DashboardCard::Accent::Purple:
        return "purple";
    }
    return "none";
}

}  // namespace

DashboardCard::DashboardCard(QString title, QWidget* parent)
    : QFrame(parent), titleLabel_(new QLabel(title, this)),
      subtitleLabel_(new QLabel(QString(), this)),
      valueLabel_(new QLabel(QString(), this)),
      placeholderLabel_(new QLabel(QString(), this)),
      statusLabel_(new QLabel(QString(), this)),
      divider_(new QFrame(this)), indicator_(new StatusIndicator(this)),
      glow_(new QGraphicsDropShadowEffect(this)) {
    setObjectName(LegacyTheme::cardObjectName());
    setFrameShape(QFrame::NoFrame);  // border comes from the style sheet
    setFocusPolicy(Qt::StrongFocus);

    // Controlled cyan glow for focus/press. Installed but disabled by default so
    // idle cards do not glow (docs/VISUAL_LANGUAGE.md: default card glow absent).
    QColor glowColor = LegacyTheme::accentCyan();
    glow_->setColor(glowColor);
    glow_->setBlurRadius(LegacyTheme::glowRadius());
    glow_->setOffset(0, 0);
    glow_->setEnabled(false);
    setGraphicsEffect(glow_);

    titleLabel_->setProperty("legacyRole", "cardTitle");
    subtitleLabel_->setProperty("legacyRole", "cardSubtitle");
    valueLabel_->setProperty("legacyRole", "primaryValue");
    placeholderLabel_->setProperty("legacyRole", "placeholder");
    statusLabel_->setProperty("legacyRole", "statusText");

    titleLabel_->setWordWrap(true);
    subtitleLabel_->setWordWrap(true);
    placeholderLabel_->setWordWrap(true);
    statusLabel_->setWordWrap(true);
    subtitleLabel_->setVisible(false);
    // The value is hidden until a card is given one, so placeholder-only cards
    // are visually unchanged.
    valueLabel_->setVisible(false);

    divider_->setObjectName(LegacyTheme::cardDividerObjectName());
    divider_->setFrameShape(QFrame::NoFrame);

    // --- Header region: title (stretch) + status indicator ------------------
    auto* header = new QWidget(this);
    header->setObjectName(LegacyTheme::cardHeaderObjectName());
    auto* headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(LegacyTheme::spaceSm());
    headerRow->addWidget(titleLabel_, /*stretch=*/1);
    headerRow->addWidget(indicator_, /*stretch=*/0, Qt::AlignTop | Qt::AlignRight);

    // --- Content region: placeholder text, occupies meaningful space --------
    auto* content = new QWidget(this);
    content->setObjectName(LegacyTheme::cardContentObjectName());
    auto* contentCol = new QVBoxLayout(content);
    contentCol->setContentsMargins(0, 0, 0, 0);
    contentCol->setSpacing(LegacyTheme::spaceXs());
    contentCol->addWidget(valueLabel_);
    contentCol->addWidget(placeholderLabel_);
    contentCol->addStretch(1);

    // --- Card root: header, subtitle, divider, content, footer --------------
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(LegacyTheme::spaceLg(), LegacyTheme::spaceLg(),
                             LegacyTheme::spaceLg(), LegacyTheme::spaceMd());
    root->setSpacing(LegacyTheme::spaceSm());
    root->addWidget(header);
    root->addWidget(subtitleLabel_);
    root->addWidget(divider_);
    root->addWidget(content, /*stretch=*/1);
    root->addWidget(statusLabel_);  // footer/status, aligned at bottom

    // A meaningful minimum so the content region reads as intentional.
    setMinimumHeight(LegacyTheme::touchTargetMin() * 3);

    setPlaceholderText(QString());  // install default placeholder
    applyStateContent();
    refreshVisualState();
}

// --- Content ----------------------------------------------------------------
void DashboardCard::setTitle(const QString& title) { titleLabel_->setText(title); }

void DashboardCard::setSubtitle(const QString& subtitle) {
    subtitleLabel_->setText(subtitle);
    subtitleLabel_->setVisible(!subtitle.isEmpty());
    divider_->setVisible(!subtitle.isEmpty());
}

void DashboardCard::setStatusText(const QString& status) {
    customStatusText_ = !status.isEmpty();
    statusLabel_->setText(status);
}

void DashboardCard::setPlaceholderText(const QString& text) {
    placeholderLabel_->setText(text.isEmpty() ? QStringLiteral("No data") : text);
}

void DashboardCard::setValueText(const QString& text) {
    valueLabel_->setText(text);
    // A card shows either a live value or a placeholder, never both: showing
    // "54.5 %" beside "No data" would be contradictory.
    const bool hasValue = !text.isEmpty();
    valueLabel_->setVisible(hasValue);
    placeholderLabel_->setVisible(!hasValue);
}

QString DashboardCard::title() const { return titleLabel_->text(); }
QString DashboardCard::subtitle() const { return subtitleLabel_->text(); }
QString DashboardCard::statusText() const { return statusLabel_->text(); }
QString DashboardCard::valueText() const { return valueLabel_->text(); }

// --- State / accent / size --------------------------------------------------
void DashboardCard::setState(State state) {
    state_ = state;
    setEnabled(state_ != State::Disabled);
    applyStateContent();
    refreshVisualState();
}

DashboardCard::State DashboardCard::state() const { return state_; }

void DashboardCard::setAccent(Accent accent) {
    accent_ = accent;
    setProperty("legacyAccent", QString::fromUtf8(accentName(accent_)));
    refreshVisualState();
}

DashboardCard::Accent DashboardCard::accent() const { return accent_; }

void DashboardCard::setSizeRole(Size size) { size_ = size; }
DashboardCard::Size DashboardCard::sizeRole() const { return size_; }

// --- Internals --------------------------------------------------------------
bool DashboardCard::isInteractive() const { return state_ != State::Disabled; }

void DashboardCard::applyStateContent() {
    indicator_->setState(toIndicatorState(state_));
    if (!customStatusText_) {
        statusLabel_->setText(QString::fromUtf8(stateStatusText(state_)));
    }
    // Propagate state onto the status + title + subtitle labels so the style
    // sheet can tint them (color reinforces the shape + text, never replaces).
    const QString name = QString::fromUtf8(stateName(state_));
    statusLabel_->setProperty("legacyState", name);
    titleLabel_->setProperty("legacyState", name);
    subtitleLabel_->setProperty("legacyState", name);
    valueLabel_->setProperty("legacyState", name);
    const std::array<QLabel*, 4> labels{statusLabel_, titleLabel_, subtitleLabel_,
                                        valueLabel_};
    for (QLabel* label : labels) {
        label->style()->unpolish(label);
        label->style()->polish(label);
    }
}

void DashboardCard::refreshVisualState() {
    // Semantic precedence resolved here into ONE property value:
    //   Error > Warning > Unavailable > Disabled > Loading > Interaction >
    //   Accent > Normal.
    // Content states (error..loading) always win over interaction visuals.
    // Interaction (pressed/focused) wins over accent/normal. Accent is carried
    // separately in "legacyAccent" and only applies when state == normal (the
    // style sheet enforces that), so accent never overrides a real state.
    QString visual;
    switch (state_) {
    case State::Error:
    case State::Critical:
    case State::Warning:
    case State::Unavailable:
    case State::Disabled:
    case State::Loading:
        visual = QString::fromUtf8(stateName(state_));
        break;
    case State::Empty:
        // Empty is a content state but lower than interaction feedback so the
        // user still sees focus/press response on an empty card.
        if (pressed_ && isInteractive()) {
            visual = QStringLiteral("pressed");
        } else if (hasFocus() && isInteractive()) {
            visual = QStringLiteral("focused");
        } else {
            visual = QStringLiteral("empty");
        }
        break;
    case State::Normal:
        if (pressed_ && isInteractive()) {
            visual = QStringLiteral("pressed");
        } else if (hasFocus() && isInteractive()) {
            visual = QStringLiteral("focused");
        } else {
            visual = QStringLiteral("normal");
        }
        break;
    }
    setProperty("legacyState", visual);
    // Controlled glow only for focus/press interaction visuals.
    setGlowActive(visual == QStringLiteral("focused")
                  || visual == QStringLiteral("pressed"));
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void DashboardCard::setGlowActive(bool active) {
    if (glow_->isEnabled() != active) {
        glow_->setEnabled(active);
    }
}

void DashboardCard::mousePressEvent(QMouseEvent* event) {
    if (isInteractive() && event->button() == Qt::LeftButton) {
        pressed_ = true;
        refreshVisualState();  // immediate press feedback
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

void DashboardCard::mouseReleaseEvent(QMouseEvent* event) {
    if (pressed_ && event->button() == Qt::LeftButton) {
        pressed_ = false;
        refreshVisualState();
        if (isInteractive() && rect().contains(event->pos())) {
            emit activated();
        }
        event->accept();
        return;
    }
    QFrame::mouseReleaseEvent(event);
}

void DashboardCard::keyPressEvent(QKeyEvent* event) {
    if (isInteractive()
        && (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return
            || event->key() == Qt::Key_Enter)) {
        pressed_ = true;
        refreshVisualState();
        event->accept();
        return;
    }
    QFrame::keyPressEvent(event);
}

void DashboardCard::keyReleaseEvent(QKeyEvent* event) {
    if (pressed_
        && (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return
            || event->key() == Qt::Key_Enter)) {
        pressed_ = false;
        refreshVisualState();
        if (isInteractive()) {
            emit activated();
        }
        event->accept();
        return;
    }
    QFrame::keyReleaseEvent(event);
}

void DashboardCard::focusInEvent(QFocusEvent* event) {
    QFrame::focusInEvent(event);
    refreshVisualState();
}

void DashboardCard::focusOutEvent(QFocusEvent* event) {
    pressed_ = false;
    QFrame::focusOutEvent(event);
    refreshVisualState();
}

}  // namespace darkspark::deck::cards

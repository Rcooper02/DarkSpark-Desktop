// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/pages/DeckPage.hpp"

#include "deck/cards/DashboardCard.hpp"
#include "models/MetricSample.hpp"
#include "themes/LegacyTheme.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QString>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace darkspark::deck::pages {

using cards::DashboardCard;
using themes::LegacyTheme;

namespace {
// Target width budget per single-column card. Column count is derived from the
// available content width divided by this, then clamped. Chosen so 2560px wide
// yields a wide multi-column deck while a small window collapses gracefully.
constexpr int kTargetCardWidth = 300;
constexpr int kMinColumns = 1;
constexpr int kMaxColumns = 6;

// Local metric-to-card mapping. Deliberately a small direct lookup: this page
// presents one telemetry metric today, and a registry or generalized routing
// abstraction would be disproportionate. Returns nullptr for a metric this page
// does not present.
[[nodiscard]] const char* cardTitleForMetric(models::MetricId id) {
    switch (id) {
    case models::MetricId::CpuTotalUtilization:
        return "CPU";
    case models::MetricId::MemoryUtilization:
        return "Memory";
    case models::MetricId::CpuTemperature:
        // Intentional: CpuTemperature has NO dashboard mapping in T7A.1. This
        // batch introduces model and telemetry support only. This is not
        // unfinished UI work and must not be "completed" by adding a temporary
        // CPU temperature card. Dashboard presentation for temperature will
        // arrive with the future subsystem-widget architecture, at which point
        // this case gains its real mapping. Returning nullptr routes any
        // temperature sample to the safe "no card for this metric" path.
        //
        // The case is explicit (rather than folded into a default) on purpose:
        // the project relies on -Wswitch to force a deliberate decision here
        // whenever a new MetricId is added. A default branch would silently
        // absorb future sensors and defeat that guarantee.
        return nullptr;
    }
    return nullptr;
}

/// Escalation thresholds, kept local to the page: they are a semantic
/// judgement about a metric, not a presentation fact, and they may differ per
/// metric. DashboardCard stays presentation-only.
struct Thresholds {
    double warning;
    double critical;
};

/// Thresholds are selected per metric so they can diverge without touching any
/// call site. CPU and memory share values today; that is a current judgement,
/// not a structural assumption.
[[nodiscard]] Thresholds thresholdsForMetric(models::MetricId id) {
    switch (id) {
    case models::MetricId::CpuTotalUtilization:
        return {85.0, 95.0};
    case models::MetricId::MemoryUtilization:
        return {85.0, 95.0};
    case models::MetricId::CpuTemperature:
        // Intentional: no temperature thresholds in T7A.1. This batch is model
        // and telemetry only; it deliberately computes no health for
        // temperature. Health (Normal / Medium / High / Critical) is a separate
        // axis owned exclusively by the future Health Engine, never by this
        // page. The unreachable-high sentinel guarantees no escalation is ever
        // produced here even if a temperature reading were routed to a card
        // (it is not, in T7A.1 -- see cardTitleForMetric above).
        //
        // Explicit case, not a default branch, so -Wswitch forces a deliberate
        // decision when the next MetricId is added rather than silently
        // inheriting these values.
        return {1.0e9, 1.0e9};
    }
    return {85.0, 95.0};
}

/// Complete presentation string including the unit, so DashboardCard performs
/// no formatting and stays metric-agnostic (docs/STYLE_GUIDE.md pairs a number
/// with its unit and fixes the decimal count so values do not jitter).
///
/// Formatting dispatches on the sample's unit rather than assuming a
/// percentage, so a future unit is a new case here and nothing else changes.
[[nodiscard]] QString formatValue(double value, models::MetricUnit unit) {
    switch (unit) {
    case models::MetricUnit::Percent:
        return QString::number(value, 'f', 1) + QStringLiteral(" %");
    case models::MetricUnit::Celsius:
        // Degree symbol, no space, per the frozen formatting rule: e.g. 64.0°C.
        return QString::number(value, 'f', 1) + QStringLiteral("\u00B0C");
    }
    return QString::number(value, 'f', 1);
}

/// Card presentation for a fresh reading, escalating by that metric's
/// thresholds.
[[nodiscard]] cards::DashboardCard::State stateForReading(models::MetricId id,
                                                          double value) {
    const Thresholds limits = thresholdsForMetric(id);
    if (value >= limits.critical) {
        return cards::DashboardCard::State::Critical;
    }
    if (value >= limits.warning) {
        return cards::DashboardCard::State::Warning;
    }
    return cards::DashboardCard::State::Normal;
}
}  // namespace

DeckPage::DeckPage(QString title, QString subtitle, QWidget* parent)
    : QWidget(parent), title_(std::move(title)), subtitle_(std::move(subtitle)),
      grid_(new QGridLayout) {
    setObjectName(LegacyTheme::pageObjectName());

    auto* root = new QVBoxLayout(this);
    // Compact top/bottom margins so the header does not consume excessive
    // vertical space on the 2560x720 target (docs/VISUAL_LANGUAGE.md).
    root->setContentsMargins(LegacyTheme::space2xl(), LegacyTheme::spaceLg(),
                             LegacyTheme::space2xl(), LegacyTheme::spaceLg());
    root->setSpacing(LegacyTheme::spaceLg());

    // --- Page header: title over a quieter subtitle, tightly grouped --------
    auto* headerBlock = new QWidget(this);
    headerBlock->setObjectName(LegacyTheme::pageHeaderObjectName());
    auto* headerCol = new QVBoxLayout(headerBlock);
    headerCol->setContentsMargins(0, 0, 0, 0);
    headerCol->setSpacing(LegacyTheme::spaceXs());

    auto* titleLabel = new QLabel(title_, headerBlock);
    titleLabel->setProperty("legacyRole", "pageTitle");
    headerCol->addWidget(titleLabel);

    if (!subtitle_.isEmpty()) {
        auto* subtitleLabel = new QLabel(subtitle_, headerBlock);
        subtitleLabel->setProperty("legacyRole", "pageSubtitle");
        headerCol->addWidget(subtitleLabel);
    }
    root->addWidget(headerBlock);

    grid_->setHorizontalSpacing(LegacyTheme::spaceLg());
    grid_->setVerticalSpacing(LegacyTheme::spaceLg());
    root->addLayout(grid_);
    root->addStretch(1);
}

void DeckPage::addCard(DashboardCard* card) {
    if (card == nullptr) {
        return;
    }
    card->setParent(this);
    cards_.append(card);
    // Force a relayout with the current width on next event; do it now so the
    // card is placed immediately even before the first resize.
    currentColumns_ = 0;  // invalidate so relayout re-runs
    const int contentWidth = width() > 0 ? width() : (kTargetCardWidth * 4);
    relayout(columnsForWidth(contentWidth));
}

QString DeckPage::title() const { return title_; }

QString DeckPage::subtitle() const { return subtitle_; }

int DeckPage::cardCount() const { return static_cast<int>(cards_.size()); }

void DeckPage::receiveTelemetry(const models::MetricSample& sample) {
    const char* wantedTitle = cardTitleForMetric(sample.id());
    if (wantedTitle == nullptr) {
        // Unknown or unrepresented metric: ignore safely.
        return;
    }

    const QString title = QString::fromUtf8(wantedTitle);
    DashboardCard* target = nullptr;
    for (DashboardCard* card : cards_) {
        if (card != nullptr && card->title() == title) {
            target = card;
            break;
        }
    }
    if (target == nullptr) {
        // This page does not hold the card for that metric; ignore safely.
        return;
    }

    // Telemetry state to card presentation. This mapping is intentionally local
    // to the page: MetricState semantics are not changed to match the card, and
    // no new card state is introduced.
    switch (sample.state()) {
    case models::MetricState::Fresh:
        if (sample.value().has_value()) {
            target->setState(stateForReading(sample.id(), *sample.value()));
            target->setValueText(formatValue(*sample.value(), sample.unit()));
        } else {
            target->setState(DashboardCard::State::Normal);
        }
        // Clearing custom status text restores the state's default footer text.
        target->setStatusText(QString());
        break;
    case models::MetricState::Stale:
        target->setState(DashboardCard::State::Warning);
        // Retain and show the last valid percentage, explicitly marked stale.
        if (sample.value().has_value()) {
            target->setValueText(formatValue(*sample.value(), sample.unit()));
        }
        target->setStatusText(QStringLiteral("Stale"));
        break;
    case models::MetricState::Unavailable:
        target->setState(DashboardCard::State::Unavailable);
        target->setStatusText(QString());
        // Never show a misleading numeric value when no value exists: clearing
        // the value restores the quiet placeholder.
        target->setValueText(QString());
        target->setPlaceholderText(QString());
        break;
    }
}

int DeckPage::columnsForWidth(int contentWidth) const {
    const int margins = LegacyTheme::spaceXl() * 2;
    const int usable = std::max(contentWidth - margins, kTargetCardWidth);
    int columns = usable / kTargetCardWidth;
    columns = std::clamp(columns, kMinColumns, kMaxColumns);
    return columns;
}

int DeckPage::spanForSize(const DashboardCard* card, int columns) {
    if (card == nullptr) {
        return 1;
    }
    int span = 1;
    switch (card->sizeRole()) {
    case DashboardCard::Size::Small:
        span = 1;
        break;
    case DashboardCard::Size::Medium:
        span = 1;
        break;
    case DashboardCard::Size::Large:
        span = 2;
        break;
    case DashboardCard::Size::Wide:
        span = 3;
        break;
    }
    return std::clamp(span, 1, columns);
}

void DeckPage::relayout(int columns) {
    if (columns == currentColumns_) {
        return;
    }
    currentColumns_ = columns;

    // Remove all cards from the grid without deleting them, then re-add.
    for (DashboardCard* card : cards_) {
        grid_->removeWidget(card);
    }

    int row = 0;
    int col = 0;
    for (DashboardCard* card : cards_) {
        const int span = spanForSize(card, columns);
        // Wrap to the next row if this card would overflow the row.
        if (col + span > columns) {
            row += 1;
            col = 0;
        }
        grid_->addWidget(card, row, col, 1, span);
        col += span;
        if (col >= columns) {
            row += 1;
            col = 0;
        }
    }

    // Keep columns evenly stretched so cards fill the width consistently.
    for (int c = 0; c < kMaxColumns; ++c) {
        grid_->setColumnStretch(c, c < columns ? 1 : 0);
    }
}

void DeckPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    relayout(columnsForWidth(event->size().width()));
}

}  // namespace darkspark::deck::pages

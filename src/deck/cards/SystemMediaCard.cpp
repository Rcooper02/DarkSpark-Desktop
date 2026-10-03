// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/cards/SystemMediaCard.hpp"

#include "deck/cards/FeishinCard.hpp"
#include "themes/LegacyTheme.hpp"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

namespace darkspark::deck::cards {

namespace {

QFrame* makePanel(
    const QString& title,
    QLabel*& value,
    QWidget* parent) {

    auto* panel = new QFrame(parent);
    panel->setFrameShape(QFrame::NoFrame);

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(
        themes::LegacyTheme::spaceMd(),
        themes::LegacyTheme::spaceMd(),
        themes::LegacyTheme::spaceMd(),
        themes::LegacyTheme::spaceMd());

    layout->setSpacing(
        themes::LegacyTheme::spaceSm());

    auto* heading = new QLabel(title, panel);
    heading->setProperty("legacyRole", "cardTitle");

    value = new QLabel(
        QStringLiteral("No data"),
        panel);

    value->setAlignment(
        Qt::AlignLeft | Qt::AlignVCenter);

    QFont font = value->font();
    font.setBold(true);
    font.setPointSize(18);
    value->setFont(font);

    layout->addWidget(heading);
    layout->addWidget(value, 1);

    return panel;
}

QProgressBar* addProgressBar(
    QFrame* panel) {

    auto* layout =
        qobject_cast<QVBoxLayout*>(panel->layout());

    if (layout == nullptr)
        return nullptr;

    auto* bar = new QProgressBar(panel);
    bar->setRange(0, 100);
    bar->setTextVisible(false);
    bar->setValue(0);
    bar->setMaximumHeight(12);

    layout->addWidget(bar);

    return bar;
}

}  // namespace

SystemMediaCard::SystemMediaCard(QWidget* parent)
    : DashboardCard(QStringLiteral("SYSTEM"), parent) {

    // We don't need another title inside the page later,
    // but keeping the card identity is useful internally.
    setSubtitle(QString());
    setAccent(Accent::Cyan);
    setSizeRole(Size::Full);
    setStatusText(QStringLiteral("Ready"));

    auto* content = new QWidget(this);

    auto* split = new QHBoxLayout(content);
    split->setContentsMargins(0, 0, 0, 0);
    split->setSpacing(
        themes::LegacyTheme::spaceLg());

    //
    // LEFT HALF — System dashboard
    //
    auto* systemSide = new QWidget(content);

    auto* systemGrid = new QGridLayout(systemSide);
    systemGrid->setContentsMargins(0, 0, 0, 0);

    systemGrid->setHorizontalSpacing(
        themes::LegacyTheme::spaceMd());

    systemGrid->setVerticalSpacing(
        themes::LegacyTheme::spaceMd());

    QFrame* cpu =
        makePanel(
            QStringLiteral("CPU"),
            cpuValue_,
            systemSide);

    cpuBar_ = addProgressBar(cpu);

    QFrame* memory =
        makePanel(
            QStringLiteral("MEMORY"),
            memoryValue_,
            systemSide);

    memoryBar_ = addProgressBar(memory);

    QFrame* gpu =
        makePanel(
            QStringLiteral("GPU"),
            gpuValue_,
            systemSide);

    QFrame* network =
        makePanel(
            QStringLiteral("NETWORK"),
            networkValue_,
            systemSide);

    QFrame* storage =
        makePanel(
            QStringLiteral("STORAGE"),
            storageValue_,
            systemSide);

    systemGrid->addWidget(cpu,     0, 0);
    systemGrid->addWidget(memory,  0, 1);
    systemGrid->addWidget(gpu,     1, 0);
    systemGrid->addWidget(network, 1, 1);
    systemGrid->addWidget(storage, 2, 0, 1, 2);

    systemGrid->setColumnStretch(0, 1);
    systemGrid->setColumnStretch(1, 1);

    systemGrid->setRowStretch(0, 1);
    systemGrid->setRowStretch(1, 1);
    systemGrid->setRowStretch(2, 1);

    //
    // RIGHT HALF — Full Feishin
    //
    feishin_ = new FeishinCard(content);

    // Let the embedded browser use the whole right side.
    feishin_->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding);

    split->addWidget(systemSide, 1);
    split->addWidget(feishin_, 1);

    setContentWidget(content);
}

void SystemMediaCard::receiveTelemetry(
    const models::MetricSample& sample) {

    if (sample.state() != models::MetricState::Fresh ||
        !sample.value().has_value()) {
        return;
    }

    const double value =
        *sample.value();

    switch (sample.id()) {
    case models::MetricId::CpuTotalUtilization:
        cpuValue_->setText(
            QStringLiteral("%1 %")
                .arg(value, 0, 'f', 1));

        if (cpuBar_ != nullptr)
            cpuBar_->setValue(
                static_cast<int>(value));
        break;

    case models::MetricId::MemoryUtilization:
        memoryValue_->setText(
            QStringLiteral("%1 %")
                .arg(value, 0, 'f', 1));

        if (memoryBar_ != nullptr)
            memoryBar_->setValue(
                static_cast<int>(value));
        break;

    case models::MetricId::CpuTemperature:
        // We'll fold temperature into the richer CPU card shortly.
        break;
    }
}


}  // namespace darkspark::deck::cards

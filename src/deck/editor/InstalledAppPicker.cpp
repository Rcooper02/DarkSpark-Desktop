// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/editor/InstalledAppPicker.hpp"

#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QIcon>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace darkspark::deck::editor {

namespace {

constexpr int CommandRole = Qt::UserRole + 1;
constexpr int IconRole = Qt::UserRole + 2;

QString cleanedExec(QString exec) {
    // Desktop-file Exec fields may contain placeholders such as:
    // %f %F %u %U %i %c %k
    exec.remove(QRegularExpression(QStringLiteral(
        R"(\s*%[fFuUdDnNickvm])"
    )));

    return exec.simplified();
}

QStringList applicationDirectories() {
    QStringList paths;

    const QString home = QDir::homePath();

    paths << QDir(home).filePath(
                 QStringLiteral(".local/share/applications"))
          << QStringLiteral("/usr/local/share/applications")
          << QStringLiteral("/usr/share/applications")

          // Flatpak exports.
          << QDir(home).filePath(
                 QStringLiteral(
                     ".local/share/flatpak/exports/share/applications"))
          << QStringLiteral(
                 "/var/lib/flatpak/exports/share/applications");

    const QString standard =
        QStandardPaths::writableLocation(
            QStandardPaths::ApplicationsLocation);

    if (!standard.isEmpty() && !paths.contains(standard))
        paths.prepend(standard);

    paths.removeDuplicates();
    return paths;
}

}  // namespace

InstalledAppPicker::InstalledAppPicker(QWidget* parent)
    : QDialog(parent),
      search_(new QLineEdit(this)),
      list_(new QListWidget(this)) {

    setWindowTitle(QStringLiteral("Browse Installed Applications"));
    setModal(true);
    resize(650, 560);

    auto* root = new QVBoxLayout(this);

    search_->setPlaceholderText(
        QStringLiteral("Search installed applications..."));
    root->addWidget(search_);

    list_->setAlternatingRowColors(true);
    list_->setIconSize(QSize(40, 40));
    root->addWidget(list_, 1);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Open |
        QDialogButtonBox::Cancel,
        this);

    root->addWidget(buttons);

    connect(
        search_,
        &QLineEdit::textChanged,
        this,
        &InstalledAppPicker::filterApplications);

    connect(
        buttons,
        &QDialogButtonBox::accepted,
        this,
        [this]() {
            if (list_->currentItem() != nullptr)
                accept();
        });

    connect(
        buttons,
        &QDialogButtonBox::rejected,
        this,
        &QDialog::reject);

    connect(
        list_,
        &QListWidget::itemDoubleClicked,
        this,
        [this](QListWidgetItem*) {
            accept();
        });

    loadApplications();

    if (list_->count() > 0)
        list_->setCurrentRow(0);
}

void InstalledAppPicker::loadApplications() {
    QSet<QString> seen;

    for (const QString& directory : applicationDirectories()) {
        if (!QDir(directory).exists())
            continue;

        QDirIterator iterator(
            directory,
            QStringList{QStringLiteral("*.desktop")},
            QDir::Files,
            QDirIterator::Subdirectories);

        while (iterator.hasNext()) {
            const QString path = iterator.next();

            QSettings desktop(path, QSettings::IniFormat);
            desktop.beginGroup(QStringLiteral("Desktop Entry"));

            const QString type =
                desktop.value(QStringLiteral("Type")).toString();

            const bool hidden =
                desktop.value(QStringLiteral("Hidden"), false).toBool();

            const bool noDisplay =
                desktop.value(QStringLiteral("NoDisplay"), false).toBool();

            const QString name =
                desktop.value(QStringLiteral("Name"))
                    .toString()
                    .trimmed();

            const QString exec =
                cleanedExec(
                    desktop.value(QStringLiteral("Exec"))
                        .toString());

            const QString icon =
                desktop.value(QStringLiteral("Icon"))
                    .toString()
                    .trimmed();

            desktop.endGroup();

            if (type != QStringLiteral("Application") ||
                hidden ||
                noDisplay ||
                name.isEmpty() ||
                exec.isEmpty()) {
                continue;
            }

            const QString uniqueKey =
                name.toLower() + QLatin1Char('|') + exec;

            if (seen.contains(uniqueKey))
                continue;

            seen.insert(uniqueKey);

            auto* item = new QListWidgetItem(name, list_);
            item->setData(CommandRole, exec);
            item->setData(IconRole, icon);
            item->setToolTip(exec);

            if (!icon.isEmpty()) {
                QIcon appIcon;

                if (QFileInfo::exists(icon))
                    appIcon = QIcon(icon);
                else
                    appIcon = QIcon::fromTheme(icon);

                if (!appIcon.isNull())
                    item->setIcon(appIcon);
            }
        }
    }

    list_->sortItems(Qt::AscendingOrder);
}

void InstalledAppPicker::filterApplications(
    const QString& text) {

    const QString needle = text.trimmed();

    for (int row = 0; row < list_->count(); ++row) {
        QListWidgetItem* item = list_->item(row);

        const bool matches =
            needle.isEmpty() ||
            item->text().contains(
                needle,
                Qt::CaseInsensitive) ||
            item->data(CommandRole)
                .toString()
                .contains(
                    needle,
                    Qt::CaseInsensitive);

        item->setHidden(!matches);
    }
}

InstalledApp InstalledAppPicker::selectedApp() const {
    InstalledApp result;

    const QListWidgetItem* item =
        list_->currentItem();

    if (item == nullptr)
        return result;

    result.name = item->text();
    result.command =
        item->data(CommandRole).toString();
    result.icon =
        item->data(IconRole).toString();

    return result;
}

}  // namespace darkspark::deck::editor

// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_EDITOR_INSTALLEDAPPPICKER_HPP
#define DARKSPARK_DECK_EDITOR_INSTALLEDAPPPICKER_HPP

#include <QDialog>
#include <QString>

class QLineEdit;
class QListWidget;

namespace darkspark::deck::editor {

struct InstalledApp {
    QString name;
    QString command;
    QString icon;
};

class InstalledAppPicker final : public QDialog {
    Q_OBJECT

public:
    explicit InstalledAppPicker(QWidget* parent = nullptr);

    [[nodiscard]] InstalledApp selectedApp() const;

private:
    void loadApplications();
    void filterApplications(const QString& text);

    QLineEdit* search_;
    QListWidget* list_;
};

}  // namespace darkspark::deck::editor

#endif

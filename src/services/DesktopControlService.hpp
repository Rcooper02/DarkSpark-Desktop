// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_DESKTOPCONTROLSERVICE_HPP
#define DARKSPARK_SERVICES_DESKTOPCONTROLSERVICE_HPP

#include "models/ControlAction.hpp"

#include <QObject>
#include <QString>
#include <QStringList>

namespace darkspark::services {

/// Executes DarkSpark's fixed desktop-control allow list.
///
/// No UI type is referenced and no arbitrary command text is accepted. Missing
/// optional Fedora utilities fail visibly through actionCompleted().
class DesktopControlService final : public QObject {
    Q_OBJECT

public:
    explicit DesktopControlService(QObject* parent = nullptr);

public slots:
    void perform(models::ControlAction action);

signals:
    void actionCompleted(models::ControlAction action, bool success,
                         const QString& message);

private:
    void launchDetached(models::ControlAction action, const QString& program,
                        const QStringList& arguments,
                        const QString& successMessage);
    void runCommand(models::ControlAction action, const QString& program,
                    const QStringList& arguments,
                    const QString& successMessage);
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_DESKTOPCONTROLSERVICE_HPP

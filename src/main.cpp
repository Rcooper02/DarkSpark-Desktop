// SPDX-License-Identifier: GPL-3.0-or-later
#include "application/Application.hpp"

#include <QApplication>

/// DarkSpark Desktop entry point.
///
/// Constructs the QApplication and the composition-root Application, parses the
/// command line, and runs. Deck-0 uses Qt logging only (no logging framework),
/// so diagnostics go through qCInfo/qCWarning categories.
int main(int argc, char** argv) {
    QApplication qtApp(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("DarkSpark"));
    QApplication::setApplicationName(QStringLiteral("DarkSpark Desktop"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0-dev"));

    darkspark::application::Application app(qtApp);
    const auto options =
        darkspark::application::Application::parseArguments(QApplication::arguments());
    return app.run(options);
}

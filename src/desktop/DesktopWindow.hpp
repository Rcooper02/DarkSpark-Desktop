// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DESKTOP_DESKTOPWINDOW_HPP
#define DARKSPARK_DESKTOP_DESKTOPWINDOW_HPP

#include <QWidget>

namespace darkspark::desktop {

/// The standard development window.
///
/// DesktopWindow is a conventional resizable window used for development and
/// everyday desktop use. Per the documented architecture it may host or launch
/// Deck Mode, but it is not assumed to use the Deck page/card framework. In
/// Deck-0 it presents the project identity and a button to launch Deck Mode in
/// a normal window, giving a keyboard-and-mouse way to reach the Deck
/// experience during development.
///
/// Ownership: a top-level QWidget owned by the Application. Threading: GUI
/// thread only.
class DesktopWindow : public QWidget {
    Q_OBJECT

public:
    explicit DesktopWindow(QWidget* parent = nullptr);

signals:
    /// Emitted when the user asks to launch Deck Mode from the desktop window.
    void launchDeckRequested();
};

}  // namespace darkspark::desktop

#endif  // DARKSPARK_DESKTOP_DESKTOPWINDOW_HPP

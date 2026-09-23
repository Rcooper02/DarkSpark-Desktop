// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_COMPANIONSPEECHSERVICE_HPP
#define DARKSPARK_SERVICES_COMPANIONSPEECHSERVICE_HPP

#include <QObject>
#include <QString>

class QProcess;

namespace darkspark::services {

class CompanionSpeechService final : public QObject {
    Q_OBJECT

public:
    explicit CompanionSpeechService(QObject* parent = nullptr);
    ~CompanionSpeechService() override;

public slots:
    void speak(const QString& text);
    void stop();

signals:
    void speechStarted();
    void speechFinished();
    void speechFailed(const QString& message);

private:
    void startPlayback();
    QString findPiper() const;
    QString findModel() const;

    QProcess* piperProcess_;
    QProcess* playbackProcess_;
    QString outputPath_;
};

}  // namespace darkspark::services

#endif

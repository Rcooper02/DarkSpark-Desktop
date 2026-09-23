// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_COMPANIONLISTENINGSERVICE_HPP
#define DARKSPARK_SERVICES_COMPANIONLISTENINGSERVICE_HPP

#include <QObject>
#include <QString>

class QProcess;
class QTimer;

namespace darkspark::services {

class CompanionListeningService final : public QObject {
    Q_OBJECT

public:
    explicit CompanionListeningService(QObject* parent = nullptr);
    ~CompanionListeningService() override;

public slots:
    void listen();
    void stop();

signals:
    void listeningStarted();
    void transcriptionStarted();
    void transcriptionReady(const QString& text);
    void listeningFailed(const QString& message);

private:
    void stopRecordingAndTranscribe();
    QString findWhisperPython() const;

    QProcess* recordProcess_;
    QProcess* whisperProcess_;
    QTimer* recordTimer_;
    QString recordingPath_;
};

}  // namespace darkspark::services

#endif

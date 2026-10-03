// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_COMPANIONLISTENINGSERVICE_HPP
#define DARKSPARK_SERVICES_COMPANIONLISTENINGSERVICE_HPP

#include <QObject>
#include <QString>
#include <QByteArray>
class QProcess;

namespace darkspark::services {
class CompanionListeningService final : public QObject {
    Q_OBJECT
public:
    explicit CompanionListeningService(QObject* parent = nullptr);
    ~CompanionListeningService() override;
public slots:
    void listen();
    void stop();
    void setPaused(bool paused);
signals:
    void wakeDetected();
    void listeningStarted();
    void transcriptionStarted();
    void transcriptionReady(const QString& text);
    void listeningFailed(const QString& message);
private:
    bool startHelper();
    QString findWhisperPython() const;
    QProcess* helper_;
    QByteArray output_;
    bool paused_ = false;
    bool stopping_ = false;
};
}
#endif

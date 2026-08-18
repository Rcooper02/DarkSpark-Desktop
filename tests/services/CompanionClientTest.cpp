// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdio>

#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "services/CompanionClient.hpp"

using darkspark::services::CompanionClient;

namespace {
int g_failures = 0;
void reportFail(const char* expression, const char* file, int line) {
    std::fprintf(stderr, "FAIL: %s (%s:%d)\n", expression, file, line);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)

void test_press_posts_expected_companion_path() {
    QTcpServer server;
    CHECK(server.listen(QHostAddress::LocalHost, 0));

    QByteArray requestBytes;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
        QTcpSocket* socket = server.nextPendingConnection();
        CHECK(socket != nullptr);
        if (socket == nullptr) return;
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
            requestBytes += socket->readAll();
            if (!requestBytes.contains("\r\n\r\n")) return;
            socket->write("HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            socket->flush();
            socket->disconnectFromHost();
        });
    });

    CompanionClient client(QUrl(QStringLiteral("http://127.0.0.1:%1")
                                    .arg(server.serverPort())));
    bool succeeded = false;
    QEventLoop loop;
    QObject::connect(&client, &CompanionClient::requestSucceeded, &loop,
                     [&](const QString& action, int page, int row, int column) {
                         succeeded = action == QStringLiteral("press")
                                     && page == 1 && row == 0 && column == 3;
                         loop.quit();
                     });
    QObject::connect(&client, &CompanionClient::requestFailed, &loop,
                     [&](const QString&, int, int, int, const QString&) {
                         loop.quit();
                     });

    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(2000);

    client.press(1, 0, 3);
    loop.exec();

    CHECK(succeeded);
    CHECK(requestBytes.startsWith("POST /api/location/1/0/3/press HTTP/1.1"));
}

void test_health_check_updates_availability() {
    QTcpServer server;
    CHECK(server.listen(QHostAddress::LocalHost, 0));

    QByteArray requestBytes;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
        QTcpSocket* socket = server.nextPendingConnection();
        CHECK(socket != nullptr);
        if (socket == nullptr) return;
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
            requestBytes += socket->readAll();
            if (!requestBytes.contains("\r\n\r\n")) return;
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 2\r\nConnection: close\r\n\r\n[]");
            socket->flush();
            socket->disconnectFromHost();
        });
    });

    CompanionClient client(QUrl(QStringLiteral("http://127.0.0.1:%1")
                                    .arg(server.serverPort())));
    bool available = false;
    QEventLoop loop;
    QObject::connect(&client, &CompanionClient::availabilityChanged, &loop,
                     [&](bool state) {
                         available = state;
                         loop.quit();
                     });

    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(2000);

    client.checkHealth();
    loop.exec();

    CHECK(client.availabilityKnown());
    CHECK(client.available());
    CHECK(available);
    CHECK(requestBytes.startsWith("GET /api/connections HTTP/1.1"));
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    test_press_posts_expected_companion_path();
    test_health_check_updates_availability();
    if (g_failures == 0) {
        std::puts("All CompanionClient tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d CompanionClient check(s) failed.\n", g_failures);
    return 1;
}

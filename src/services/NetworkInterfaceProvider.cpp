// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/NetworkInterfaceProvider.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>

namespace darkspark::services {

namespace {

QString readFileTrimmed(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(f.readAll()).trimmed();
}

// The interface carrying the default route, from /proc/net/route. The default
// route has Destination 00000000. Returns empty when it cannot be determined --
// an honest limit, never a guess.
QString defaultRouteInterface() {
    QFile f(QStringLiteral("/proc/net/route"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    const QString content = QString::fromUtf8(f.readAll());
    const QStringList lines =
        content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    // First line is a header; subsequent lines: Iface Destination Gateway ...
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QStringList cols =
            lines.at(i).split(QLatin1Char('\t'), Qt::SkipEmptyParts);
        if (cols.size() < 2) {
            continue;
        }
        if (cols.at(1) == QStringLiteral("00000000")) {
            return cols.at(0);
        }
    }
    return QString();
}

// Classify an interface as physical using sysfs. A physical NIC has a real
// device symlink (/sys/class/net/<if>/device); virtual interfaces (veth, bridge,
// docker, tun/tap, wireguard) do not. Loopback is detected by name/flags.
bool interfaceIsPhysical(const QString& name) {
    const QString devLink =
        QStringLiteral("/sys/class/net/") + name + QStringLiteral("/device");
    return QFile::exists(devLink);
}

bool interfaceIsUp(const QString& name) {
    const QString state = readFileTrimmed(
        QStringLiteral("/sys/class/net/") + name + QStringLiteral("/operstate"));
    // "up" means carrier present; "unknown" is common for some virtual devices.
    return state == QStringLiteral("up");
}

std::vector<NetworkInterfaceSnapshot> enumerateProd() {
    std::vector<NetworkInterfaceSnapshot> out;
    QFile f(QStringLiteral("/proc/net/dev"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return out;
    }
    const QString defaultIface = defaultRouteInterface();
    const QString content = QString::fromUtf8(f.readAll());
    const QStringList lines =
        content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    // /proc/net/dev: two header lines, then "iface: rx_bytes rx_packets ... (16
    // fields) tx_bytes ...". Field 0 (after the colon) = rx_bytes; field 8 =
    // tx_bytes.
    for (const QString& line : lines) {
        const qsizetype colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0) {
            continue;  // header lines have no colon in the name position
        }
        const QString name = line.left(colon).trimmed();
        if (name.isEmpty()) {
            continue;
        }
        const QStringList nums = line.mid(colon + 1)
                                     .split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (nums.size() < 16) {
            continue;
        }
        bool okRx = false;
        bool okTx = false;
        const quint64 rx = nums.at(0).toULongLong(&okRx);
        const quint64 tx = nums.at(8).toULongLong(&okTx);
        if (!okRx || !okTx) {
            continue;
        }
        NetworkInterfaceSnapshot s;
        s.name = name;
        s.rxBytes = static_cast<std::uint64_t>(rx);
        s.txBytes = static_cast<std::uint64_t>(tx);
        s.isLoopback = (name == QStringLiteral("lo"));
        s.isPhysical = !s.isLoopback && interfaceIsPhysical(name);
        s.isUp = s.isLoopback ? true : interfaceIsUp(name);
        s.isDefaultRoute = !defaultIface.isEmpty() && name == defaultIface;
        out.push_back(s);
    }
    return out;
}

std::int64_t nowMsProd() {
    return static_cast<std::int64_t>(QDateTime::currentMSecsSinceEpoch());
}

}  // namespace

NetworkInterfaceProvider::NetworkInterfaceProvider()
    : interfaces_(enumerateProd), clock_(nowMsProd) {}

NetworkInterfaceProvider::NetworkInterfaceProvider(InterfaceSource interfaces,
                                                   Clock clock)
    : interfaces_(std::move(interfaces)), clock_(std::move(clock)) {}

QString NetworkInterfaceProvider::providerName() const {
    return QStringLiteral("interface");
}

std::vector<NormalizedNetworkSensor> NetworkInterfaceProvider::discover() const {
    std::vector<NormalizedNetworkSensor> out;
    const std::vector<NetworkInterfaceSnapshot> ifaces =
        interfaces_ ? interfaces_() : std::vector<NetworkInterfaceSnapshot>{};
    const std::int64_t nowMs = clock_ ? clock_() : 0;

    for (const NetworkInterfaceSnapshot& s : ifaces) {
        const std::string key = s.name.toStdString();
        InterfaceRates& rates = state_[key];
        // Advance the rate calculators now; capture the computed rate (or
        // nullopt) so the read() closure returns a stable value for this poll.
        // Reuses the exact Storage ThroughputRateCalculator, unchanged.
        rates.lastRxRate = rates.rx.update(s.rxBytes, nowMs);
        rates.lastTxRate = rates.tx.update(s.txBytes, nowMs);

        NetworkTarget target;
        target.selectionKey = s.name;
        target.isDefaultRoute = s.isDefaultRoute;
        target.isLoopback = s.isLoopback;
        target.isPhysical = s.isPhysical;
        target.isUp = s.isUp;

        SensorMetadata meta;
        meta.label = s.name;
        meta.stableId = QStringLiteral("net:") + s.name;
        meta.devicePath = QStringLiteral("/sys/class/net/") + s.name;
        meta.capabilities = QStringList{QStringLiteral("rx"), QStringLiteral("tx"),
                                        QStringLiteral("link")};

        const std::optional<double> rxRate = rates.lastRxRate;
        NormalizedNetworkSensor rx;
        rx.kind = NetworkSensorKind::ReceiveRate;
        rx.unit = NetworkSensorUnit::BytesPerSecond;
        rx.target = target;
        rx.metadata = meta;
        rx.read = [rxRate]() { return rxRate; };
        out.push_back(rx);

        const std::optional<double> txRate = rates.lastTxRate;
        NormalizedNetworkSensor tx;
        tx.kind = NetworkSensorKind::TransmitRate;
        tx.unit = NetworkSensorUnit::BytesPerSecond;
        tx.target = target;
        tx.metadata = meta;
        tx.read = [txRate]() { return txRate; };
        out.push_back(tx);

        const double rxCumulative = static_cast<double>(s.rxBytes);
        NormalizedNetworkSensor rxb;
        rxb.kind = NetworkSensorKind::ReceivedBytes;
        rxb.unit = NetworkSensorUnit::Bytes;
        rxb.target = target;
        rxb.metadata = meta;
        rxb.read = [rxCumulative]() {
            return std::optional<double>(rxCumulative);
        };
        out.push_back(rxb);

        const double txCumulative = static_cast<double>(s.txBytes);
        NormalizedNetworkSensor txb;
        txb.kind = NetworkSensorKind::TransmittedBytes;
        txb.unit = NetworkSensorUnit::Bytes;
        txb.target = target;
        txb.metadata = meta;
        txb.read = [txCumulative]() {
            return std::optional<double>(txCumulative);
        };
        out.push_back(txb);

        const double link = s.isUp ? 1.0 : 0.0;
        NormalizedNetworkSensor ls;
        ls.kind = NetworkSensorKind::LinkState;
        ls.unit = NetworkSensorUnit::Count;
        ls.target = target;
        ls.metadata = meta;
        ls.read = [link]() { return std::optional<double>(link); };
        out.push_back(ls);
    }
    return out;
}

}  // namespace darkspark::services

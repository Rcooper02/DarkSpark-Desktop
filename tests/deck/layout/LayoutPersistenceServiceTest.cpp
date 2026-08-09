// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for LayoutPersistenceService: the file-facing load/save/recover flows,
// driven against a temporary file path so no test touches the real config
// directory. Links Qt6::Core (QFile/QDir/QStandardPaths via the service); own
// main().

#include <cstdio>

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "deck/layout/DeckLayout.hpp"
#include "deck/layout/DeckLayoutSerializer.hpp"
#include "deck/layout/LayoutPersistenceService.hpp"

using namespace darkspark::deck::layout;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)

bool sameAsDefault(const DeckLayout& l) {
    const DeckLayout& d = defaultCommandDeckLayout();
    if (l.pageId != d.pageId || l.placements.size() != d.placements.size()) {
        return false;
    }
    for (std::size_t i = 0; i < d.placements.size(); ++i) {
        if (!(l.placements[i] == d.placements[i])) return false;
    }
    return true;
}

void writeFile(const QString& path, const QByteArray& bytes) {
    QFile f(path);

    CHECK(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    if (!f.isOpen()) {
        return;
    }

    CHECK(f.write(bytes) == bytes.size());
    f.close();
}

QByteArray readFile(const QString& path) {
    QFile f(path);

    CHECK(f.open(QIODevice::ReadOnly));
    if (!f.isOpen()) {
        return {};
    }

    const QByteArray b = f.readAll();
    f.close();
    return b;
}

void test_missing_file_writes_default() {
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("layout.json"));
    CHECK(!QFile::exists(path));  // precondition
    LayoutPersistenceService svc(path);
    const DeckLayout resolved = svc.loadOrDefault();
    // Returns the default AND the file now exists (materialised on first run).
    CHECK(sameAsDefault(resolved));
    CHECK(QFile::exists(path));
    // The written file is a valid, re-loadable default.
    const auto reparsed = deserializeLayout(readFile(path));
    CHECK(reparsed.has_value() && sameAsDefault(*reparsed));
}

void test_corrupt_file_restores_default() {
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("layout.json"));
    writeFile(path, QByteArray("{ this is not valid json"));
    LayoutPersistenceService svc(path);
    const DeckLayout resolved = svc.loadOrDefault();
    CHECK(sameAsDefault(resolved));
    // The bad file was overwritten with a valid default (self-recovery).
    const auto reparsed = deserializeLayout(readFile(path));
    CHECK(reparsed.has_value() && sameAsDefault(*reparsed));
}

void test_valid_file_loaded_unchanged() {
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("layout.json"));
    // A valid, non-default layout: disable Storage so it differs from default.
    DeckLayout custom = defaultCommandDeckLayout();
    for (DeckWidgetPlacement& p : custom.placements) {
        if (p.id == WidgetId::Storage) {
            p.enabled = false;
        }
    }
    CHECK(isValidLayout(custom));
    writeFile(path, serializeLayout(custom));
    LayoutPersistenceService svc(path);
    const DeckLayout resolved = svc.loadOrDefault();
    // Loaded unchanged: not replaced by the default.
    CHECK(!sameAsDefault(resolved));
    CHECK(resolved.placements.size() == custom.placements.size());
    bool storageDisabled = false;
    for (const DeckWidgetPlacement& p : resolved.placements) {
        if (p.id == WidgetId::Storage && !p.enabled) storageDisabled = true;
    }
    CHECK(storageDisabled);
}

void test_invalid_geometry_file_restores_default() {
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("layout.json"));
    // Parses, but two enabled widgets overlap -> isValidLayout false.
    const QByteArray overlap =
        "{ \"version\": 1, \"pageId\": 0, \"placements\": ["
        "  { \"widgetId\": \"gpu\", \"region\": \"secondary\","
        "    \"row\": 0, \"column\": 0, \"rowSpan\": 1, \"columnSpan\": 1,"
        "    \"sizeMode\": \"small\", \"enabled\": true },"
        "  { \"widgetId\": \"memory\", \"region\": \"secondary\","
        "    \"row\": 0, \"column\": 0, \"rowSpan\": 1, \"columnSpan\": 1,"
        "    \"sizeMode\": \"small\", \"enabled\": true } ] }";
    writeFile(path, overlap);
    LayoutPersistenceService svc(path);
    const DeckLayout resolved = svc.loadOrDefault();
    CHECK(sameAsDefault(resolved));
    const auto reparsed = deserializeLayout(readFile(path));
    CHECK(reparsed.has_value() && sameAsDefault(*reparsed));
}

void test_save_then_load_round_trip() {
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("nested/layout.json"));
    LayoutPersistenceService svc(path);
    // save() creates the nested directory.
    CHECK(svc.save(defaultCommandDeckLayout()));
    CHECK(QFile::exists(path));
    const DeckLayout resolved = svc.loadOrDefault();
    CHECK(sameAsDefault(resolved));
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    test_missing_file_writes_default();
    test_corrupt_file_restores_default();
    test_valid_file_loaded_unchanged();
    test_invalid_geometry_file_restores_default();
    test_save_then_load_round_trip();
    if (g_failures == 0) {
        std::puts("All LayoutPersistenceService tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d LayoutPersistenceService check(s) failed.\n",
                 g_failures);
    return 1;
}

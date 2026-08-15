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
#include "deck/layout/DeckLayoutCollection.hpp"
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

    CHECK(f.write(bytes) == static_cast<qint64>(bytes.size()));
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

// --- collection (v2) persistence + migration --------------------------------

bool sameLayout(const DeckLayout& a, const DeckLayout& b) {
    if (a.pageId != b.pageId) return false;
    if (a.placements.size() != b.placements.size()) return false;

    for (std::size_t i = 0; i < a.placements.size(); ++i) {
        if (!(a.placements[i] == b.placements[i])) return false;
    }

    return true;
}

bool sameAsDefaultCollection(const DeckLayoutCollection& c) {
    const DeckLayoutCollection& d = defaultCommandDeckCollection();
    if (c.activePageId != d.activePageId) return false;
    if (c.pages.size() != d.pages.size()) return false;
    for (std::size_t i = 0; i < d.pages.size(); ++i) {
        if (c.pages[i].pageId != d.pages[i].pageId) return false;
        if (c.pages[i].name != d.pages[i].name) return false;
        if (!sameLayout(c.pages[i].layout, d.pages[i].layout)) return false;
    }
    return true;
}

void test_missing_file_writes_default_collection() {
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("layout.json"));
    CHECK(!QFile::exists(path));
    LayoutPersistenceService svc(path);
    const DeckLayoutCollection c = svc.loadCollectionOrDefault();
    CHECK(sameAsDefaultCollection(c));
    CHECK(QFile::exists(path));
    // The written file is v2 and re-loads as the default collection.
    const auto reparsed = deserializeCollection(readFile(path));
    CHECK(reparsed.has_value() && sameAsDefaultCollection(*reparsed));
}

void test_corrupt_file_restores_default_collection() {
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("layout.json"));
    writeFile(path, QByteArray("{ garbage not json"));
    LayoutPersistenceService svc(path);
    const DeckLayoutCollection c = svc.loadCollectionOrDefault();
    CHECK(sameAsDefaultCollection(c));
    const auto reparsed = deserializeCollection(readFile(path));
    CHECK(reparsed.has_value() && sameAsDefaultCollection(*reparsed));
}

void test_unknown_version_restores_default_collection() {
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("layout.json"));
    writeFile(path,
              QByteArray("{ \"version\": 9, \"activePageId\": 0, "
                         "\"pages\": [] }"));
    LayoutPersistenceService svc(path);
    const DeckLayoutCollection c = svc.loadCollectionOrDefault();
    CHECK(sameAsDefaultCollection(c));  // unknown version -> full v2 default
}

void test_v1_file_migrated_and_rewritten() {
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("layout.json"));
    // Write a legacy v1 single-layout file (Batch-2 format).
    writeFile(path, serializeLayout(defaultCommandDeckLayout()));
    LayoutPersistenceService svc(path);
    const DeckLayoutCollection c = svc.loadCollectionOrDefault();
    // Migrated to 3 pages, System first with the original placements.
    CHECK(c.pages.size() == 3);
    CHECK(c.pages[0].name == QStringLiteral("System"));
    CHECK(c.pages[0].layout.placements.size()
          == defaultCommandDeckLayout().placements.size());
    CHECK(c.pages[1].name == QStringLiteral("Controls"));
    CHECK(c.pages[2].name == QStringLiteral("Custom"));
    // The file was rewritten as v2: re-reading yields a collection, not a v1.
    const auto reparsed = deserializeCollection(readFile(path));
    CHECK(reparsed.has_value() && reparsed->pages.size() == 3);
    // And the on-disk bytes now declare version 2.
    const QByteArray onDisk = readFile(path);
    CHECK(onDisk.contains("\"version\": 2"));
}

void test_v1_migration_preserves_custom_placements() {
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("layout.json"));
    DeckLayout custom = defaultCommandDeckLayout();
    for (DeckWidgetPlacement& p : custom.placements) {
        if (p.id == WidgetId::Storage) p.enabled = false;
    }
    writeFile(path, serializeLayout(custom));
    LayoutPersistenceService svc(path);
    const DeckLayoutCollection c = svc.loadCollectionOrDefault();
    bool storageDisabled = false;
    for (const DeckWidgetPlacement& p : c.pages[0].layout.placements) {
        if (p.id == WidgetId::Storage && !p.enabled) storageDisabled = true;
    }
    CHECK(storageDisabled);  // user's v1 layout preserved, not discarded
}

void test_valid_v2_loaded_unchanged() {
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("layout.json"));
    DeckLayoutCollection c = defaultCommandDeckCollection();
    c.activePageId = 2;  // non-default active page
    writeFile(path, serializeCollection(c));
    LayoutPersistenceService svc(path);
    const DeckLayoutCollection loaded = svc.loadCollectionOrDefault();
    CHECK(loaded.activePageId == 2);
    CHECK(loaded.pages.size() == 3);
}

void test_active_page_save_and_load() {
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("layout.json"));
    LayoutPersistenceService svc(path);
    const DeckLayoutCollection initial = svc.loadCollectionOrDefault();
    CHECK(initial.activePageId == 0);    // default is written with page 0 active
    CHECK(svc.saveActivePage(1));        // switch to page 1
    const DeckLayoutCollection reloaded = svc.loadCollectionOrDefault();
    CHECK(reloaded.activePageId == 1);   // restored on next load
    // Unknown page id is refused.
    CHECK(!svc.saveActivePage(42));
}

void test_invalid_collection_file_restores_default() {
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("layout.json"));
    // Valid JSON/v2 but duplicate page ids -> isValidCollection false.
    const QByteArray dup =
        "{ \"version\": 2, \"activePageId\": 0, \"pages\": ["
        "  { \"pageId\": 0, \"name\": \"A\", \"placements\": [] },"
        "  { \"pageId\": 0, \"name\": \"B\", \"placements\": [] } ] }";
    writeFile(path, dup);
    LayoutPersistenceService svc(path);
    const DeckLayoutCollection c = svc.loadCollectionOrDefault();
    CHECK(sameAsDefaultCollection(c));
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    test_missing_file_writes_default();
    test_corrupt_file_restores_default();
    test_valid_file_loaded_unchanged();
    test_invalid_geometry_file_restores_default();
    test_save_then_load_round_trip();
    test_missing_file_writes_default_collection();
    test_corrupt_file_restores_default_collection();
    test_unknown_version_restores_default_collection();
    test_v1_file_migrated_and_rewritten();
    test_v1_migration_preserves_custom_placements();
    test_valid_v2_loaded_unchanged();
    test_active_page_save_and_load();
    test_invalid_collection_file_restores_default();
    if (g_failures == 0) {
        std::puts("All LayoutPersistenceService tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d LayoutPersistenceService check(s) failed.\n",
                 g_failures);
    return 1;
}

// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for DeckLayout serialization: round-trip, every enum<->string mapping,
// malformed JSON, unknown ids, and parsed-but-invalid geometry. Uses Qt JSON so
// links Qt6::Core; own main(). No file I/O -- that is the persistence service.

#include <cstdio>

#include <QByteArray>
#include <QCoreApplication>

#include "deck/layout/DeckLayout.hpp"
#include "deck/layout/DeckLayoutCollection.hpp"
#include "deck/layout/DeckLayoutSerializer.hpp"

using namespace darkspark::deck::layout;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)

void test_default_round_trip() {
    const DeckLayout& def = defaultCommandDeckLayout();
    const QByteArray bytes = serializeLayout(def);
    const auto parsed = deserializeLayout(bytes);
    CHECK(parsed.has_value());
    // Structural equality: same page id and same placements in order.
    CHECK(parsed->pageId == def.pageId);
    CHECK(parsed->placements.size() == def.placements.size());
    if (parsed->placements.size() == def.placements.size()) {
        for (std::size_t i = 0; i < def.placements.size(); ++i) {
            CHECK(parsed->placements[i] == def.placements[i]);
        }
    }
    // And it validates.
    CHECK(isValidLayout(*parsed));
}

void test_widget_id_mappings() {
    const WidgetId ids[] = {WidgetId::Cpu,     WidgetId::Gpu,
                            WidgetId::Memory,  WidgetId::Cooling,
                            WidgetId::Storage, WidgetId::Network};
    for (WidgetId id : ids) {
        const QString s = widgetIdToString(id);
        CHECK(!s.isEmpty());
        const auto back = widgetIdFromString(s);
        CHECK(back.has_value() && *back == id);
    }
    // Unknown has no string and does not round-trip from arbitrary tokens.
    CHECK(widgetIdToString(WidgetId::Unknown).isEmpty());
    CHECK(!widgetIdFromString(QStringLiteral("nonsense")).has_value());
    CHECK(!widgetIdFromString(QString()).has_value());
}

void test_region_mappings() {
    for (DeckRegion r : {DeckRegion::Primary, DeckRegion::Secondary}) {
        const QString s = regionToString(r);
        CHECK(!s.isEmpty());
        const auto back = regionFromString(s);
        CHECK(back.has_value() && *back == r);
    }
    CHECK(!regionFromString(QStringLiteral("tertiary")).has_value());
}

void test_size_mode_mappings() {
    const InstrumentSizeMode modes[] = {
        InstrumentSizeMode::Small, InstrumentSizeMode::Medium,
        InstrumentSizeMode::Large, InstrumentSizeMode::Wide};
    for (InstrumentSizeMode m : modes) {
        const QString s = sizeModeToString(m);
        CHECK(!s.isEmpty());
        const auto back = sizeModeFromString(s);
        CHECK(back.has_value() && *back == m);
    }
    CHECK(!sizeModeFromString(QStringLiteral("gigantic")).has_value());
}

void test_malformed_json() {
    CHECK(!deserializeLayout(QByteArray("not json at all")).has_value());
    CHECK(!deserializeLayout(QByteArray("{ broken")).has_value());
    CHECK(!deserializeLayout(QByteArray("[]")).has_value());  // not an object
    CHECK(!deserializeLayout(QByteArray()).has_value());       // empty
}

void test_unknown_version() {
    const QByteArray v2 =
        "{ \"version\": 2, \"pageId\": 0, \"placements\": [] }";
    CHECK(!deserializeLayout(v2).has_value());  // newer version -> fallback
    const QByteArray noVer = "{ \"pageId\": 0, \"placements\": [] }";
    CHECK(!deserializeLayout(noVer).has_value());  // missing version
}

void test_unknown_widget_id_in_json() {
    const QByteArray bytes =
        "{ \"version\": 1, \"pageId\": 0, \"placements\": ["
        "  { \"widgetId\": \"quantumcore\", \"region\": \"secondary\","
        "    \"row\": 0, \"column\": 0, \"rowSpan\": 1, \"columnSpan\": 1,"
        "    \"sizeMode\": \"small\", \"enabled\": true } ] }";
    CHECK(!deserializeLayout(bytes).has_value());  // unknown id -> nullopt
}

void test_wrong_field_types() {
    const QByteArray bytes =
        "{ \"version\": 1, \"pageId\": 0, \"placements\": ["
        "  { \"widgetId\": \"gpu\", \"region\": \"secondary\","
        "    \"row\": \"zero\", \"column\": 0, \"rowSpan\": 1, \"columnSpan\": 1,"
        "    \"sizeMode\": \"small\", \"enabled\": true } ] }";
    CHECK(!deserializeLayout(bytes).has_value());  // row is a string
}

void test_parsed_but_invalid_geometry() {
    // Parses fine (valid enums/types) but fails isValidLayout: negative row.
    const QByteArray neg =
        "{ \"version\": 1, \"pageId\": 0, \"placements\": ["
        "  { \"widgetId\": \"gpu\", \"region\": \"secondary\","
        "    \"row\": -1, \"column\": 0, \"rowSpan\": 1, \"columnSpan\": 1,"
        "    \"sizeMode\": \"small\", \"enabled\": true } ] }";
    const auto parsed = deserializeLayout(neg);
    CHECK(parsed.has_value());              // deserialization succeeds
    CHECK(!isValidLayout(*parsed));         // but geometry is invalid
}

void test_parsed_but_duplicate_and_overlap() {
    const QByteArray dup =
        "{ \"version\": 1, \"pageId\": 0, \"placements\": ["
        "  { \"widgetId\": \"gpu\", \"region\": \"secondary\","
        "    \"row\": 0, \"column\": 0, \"rowSpan\": 1, \"columnSpan\": 1,"
        "    \"sizeMode\": \"small\", \"enabled\": true },"
        "  { \"widgetId\": \"gpu\", \"region\": \"secondary\","
        "    \"row\": 0, \"column\": 1, \"rowSpan\": 1, \"columnSpan\": 1,"
        "    \"sizeMode\": \"small\", \"enabled\": true } ] }";
    const auto parsedDup = deserializeLayout(dup);
    CHECK(parsedDup.has_value() && !isValidLayout(*parsedDup));  // duplicate id

    const QByteArray overlap =
        "{ \"version\": 1, \"pageId\": 0, \"placements\": ["
        "  { \"widgetId\": \"gpu\", \"region\": \"secondary\","
        "    \"row\": 0, \"column\": 0, \"rowSpan\": 1, \"columnSpan\": 1,"
        "    \"sizeMode\": \"small\", \"enabled\": true },"
        "  { \"widgetId\": \"memory\", \"region\": \"secondary\","
        "    \"row\": 0, \"column\": 0, \"rowSpan\": 1, \"columnSpan\": 1,"
        "    \"sizeMode\": \"small\", \"enabled\": true } ] }";
    const auto parsedOv = deserializeLayout(overlap);
    CHECK(parsedOv.has_value() && !isValidLayout(*parsedOv));  // overlap
}

// --- v2 collection + migration ----------------------------------------------

bool sameLayout(const DeckLayout& a, const DeckLayout& b) {
    if (a.pageId != b.pageId) return false;
    if (a.placements.size() != b.placements.size()) return false;

    for (std::size_t i = 0; i < a.placements.size(); ++i) {
        if (!(a.placements[i] == b.placements[i])) return false;
    }

    return true;
}

bool sameCollection(const DeckLayoutCollection& a,
                    const DeckLayoutCollection& b) {
    if (a.activePageId != b.activePageId) return false;
    if (a.pages.size() != b.pages.size()) return false;
    for (std::size_t i = 0; i < a.pages.size(); ++i) {
        if (a.pages[i].pageId != b.pages[i].pageId) return false;
        if (a.pages[i].name != b.pages[i].name) return false;
        if (!sameLayout(a.pages[i].layout, b.pages[i].layout)) return false;
    }
    return true;
}

void test_collection_round_trip() {
    const DeckLayoutCollection& def = defaultCommandDeckCollection();
    const QByteArray bytes = serializeCollection(def);
    const auto parsed = deserializeCollection(bytes);
    CHECK(parsed.has_value());
    CHECK(sameCollection(*parsed, def));
    CHECK(isValidCollection(*parsed));
}

void test_collection_preserves_active_page() {
    DeckLayoutCollection c = defaultCommandDeckCollection();
    c.activePageId = 2;
    const auto parsed = deserializeCollection(serializeCollection(c));
    CHECK(parsed.has_value() && parsed->activePageId == 2);
}

void test_v1_migrates_to_v2() {
    // A legacy v1 single-layout file (Batch-2 shape) migrates to a 3-page v2
    // collection: the v1 layout becomes System (pageId 0), plus empty
    // Controls(1)/Custom(2), activePageId 0.
    const QByteArray v1 = serializeLayout(defaultCommandDeckLayout());
    const auto parsed = deserializeCollection(v1);
    CHECK(parsed.has_value());
    CHECK(parsed->pages.size() == 3);
    CHECK(parsed->activePageId == 0);
    // System page preserves the exact v1 placements.
    CHECK(parsed->pages[0].pageId == 0);
    CHECK(parsed->pages[0].name == QStringLiteral("System"));
    CHECK(parsed->pages[0].layout.placements.size()
          == defaultCommandDeckLayout().placements.size());
    for (std::size_t i = 0; i < defaultCommandDeckLayout().placements.size();
         ++i) {
        CHECK(parsed->pages[0].layout.placements[i]
              == defaultCommandDeckLayout().placements[i]);
    }
    // Controls + Custom added, empty.
    CHECK(parsed->pages[1].name == QStringLiteral("Controls"));
    CHECK(parsed->pages[1].layout.placements.empty());
    CHECK(parsed->pages[2].name == QStringLiteral("Custom"));
    CHECK(parsed->pages[2].layout.placements.empty());
    CHECK(isValidCollection(*parsed));
}

void test_v1_migration_preserves_custom_layout() {
    // A NON-default v1 layout (Storage disabled) must survive migration intact.
    DeckLayout custom = defaultCommandDeckLayout();
    for (DeckWidgetPlacement& p : custom.placements) {
        if (p.id == WidgetId::Storage) p.enabled = false;
    }
    const QByteArray v1 = serializeLayout(custom);
    const auto parsed = deserializeCollection(v1);
    CHECK(parsed.has_value());
    bool storageDisabled = false;
    for (const DeckWidgetPlacement& p : parsed->pages[0].layout.placements) {
        if (p.id == WidgetId::Storage && !p.enabled) storageDisabled = true;
    }
    CHECK(storageDisabled);
}

void test_unknown_version_collection_fallback() {
    const QByteArray v3 =
        "{ \"version\": 3, \"activePageId\": 0, \"pages\": [] }";
    CHECK(!deserializeCollection(v3).has_value());  // future version -> nullopt
    const QByteArray noVer = "{ \"activePageId\": 0, \"pages\": [] }";
    CHECK(!deserializeCollection(noVer).has_value());  // missing version
}

void test_malformed_collection() {
    CHECK(!deserializeCollection(QByteArray("not json")).has_value());
    CHECK(!deserializeCollection(QByteArray("[]")).has_value());
    // v2 with pages not an array.
    const QByteArray badPages =
        "{ \"version\": 2, \"activePageId\": 0, \"pages\": 5 }";
    CHECK(!deserializeCollection(badPages).has_value());
    // v2 page missing name.
    const QByteArray noName =
        "{ \"version\": 2, \"activePageId\": 0, \"pages\": ["
        "  { \"pageId\": 0, \"placements\": [] } ] }";
    CHECK(!deserializeCollection(noName).has_value());
}

void test_collection_unknown_widget_id_fails() {
    const QByteArray bad =
        "{ \"version\": 2, \"activePageId\": 0, \"pages\": ["
        "  { \"pageId\": 0, \"name\": \"System\", \"placements\": ["
        "    { \"widgetId\": \"warpcore\", \"region\": \"secondary\","
        "      \"row\": 0, \"column\": 0, \"rowSpan\": 1, \"columnSpan\": 1,"
        "      \"sizeMode\": \"small\", \"enabled\": true } ] } ] }";
    CHECK(!deserializeCollection(bad).has_value());
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    test_default_round_trip();
    test_widget_id_mappings();
    test_region_mappings();
    test_size_mode_mappings();
    test_malformed_json();
    test_unknown_version();
    test_unknown_widget_id_in_json();
    test_wrong_field_types();
    test_parsed_but_invalid_geometry();
    test_parsed_but_duplicate_and_overlap();
    test_collection_round_trip();
    test_collection_preserves_active_page();
    test_v1_migrates_to_v2();
    test_v1_migration_preserves_custom_layout();
    test_unknown_version_collection_fallback();
    test_malformed_collection();
    test_collection_unknown_widget_id_fails();
    if (g_failures == 0) {
        std::puts("All DeckLayoutSerializer tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d DeckLayoutSerializer check(s) failed.\n", g_failures);
    return 1;
}

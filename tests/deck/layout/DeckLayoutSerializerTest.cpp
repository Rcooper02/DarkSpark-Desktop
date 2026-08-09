// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for DeckLayout serialization: round-trip, every enum<->string mapping,
// malformed JSON, unknown ids, and parsed-but-invalid geometry. Uses Qt JSON so
// links Qt6::Core; own main(). No file I/O -- that is the persistence service.

#include <cstdio>

#include <QByteArray>
#include <QCoreApplication>

#include "deck/layout/DeckLayout.hpp"
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
    if (g_failures == 0) {
        std::puts("All DeckLayoutSerializer tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d DeckLayoutSerializer check(s) failed.\n", g_failures);
    return 1;
}

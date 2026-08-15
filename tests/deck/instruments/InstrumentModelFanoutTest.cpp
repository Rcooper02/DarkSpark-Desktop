// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstdio>
#include <vector>

#include "deck/instruments/InstrumentModelFanout.hpp"

namespace {

int g_failures = 0;

void reportFail(const char* expression, const char* file, int line) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", expression, file, line);
    ++g_failures;
}

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            reportFail(#condition, __FILE__, __LINE__);                        \
        }                                                                       \
    } while (0)

struct FakeModel {
    int value = 0;
};

class FakeInstrument {
public:
    void setModel(const FakeModel& model) {
        model_ = model;
        ++setCount_;
    }

    [[nodiscard]] FakeModel model() const { return model_; }
    [[nodiscard]] int setCount() const { return setCount_; }

private:
    FakeModel model_{};
    int setCount_ = 0;
};

void test_fans_out_to_multiple_views() {
    FakeInstrument first;
    FakeInstrument second;
    const std::vector<FakeInstrument*> targets{&first, &second};

    darkspark::deck::instruments::fanOutInstrumentModel(FakeModel{42}, targets);

    CHECK(first.model().value == 42);
    CHECK(second.model().value == 42);
    CHECK(first.setCount() == 1);
    CHECK(second.setCount() == 1);
}

void test_null_targets_are_ignored() {
    FakeInstrument first;
    const std::vector<FakeInstrument*> targets{nullptr, &first, nullptr};

    darkspark::deck::instruments::fanOutInstrumentModel(FakeModel{7}, targets);

    CHECK(first.model().value == 7);
    CHECK(first.setCount() == 1);
}

void test_subsequent_models_reach_every_view() {
    FakeInstrument first;
    FakeInstrument second;
    const std::vector<FakeInstrument*> targets{&first, &second};

    darkspark::deck::instruments::fanOutInstrumentModel(FakeModel{1}, targets);
    darkspark::deck::instruments::fanOutInstrumentModel(FakeModel{9}, targets);

    CHECK(first.model().value == 9);
    CHECK(second.model().value == 9);
    CHECK(first.setCount() == 2);
    CHECK(second.setCount() == 2);
}

}  // namespace

int main() {
    test_fans_out_to_multiple_views();
    test_null_targets_are_ignored();
    test_subsequent_models_reach_every_view();

    if (g_failures == 0) {
        std::puts("All InstrumentModelFanout tests passed.");
        return 0;
    }

    std::fprintf(stderr, "%d InstrumentModelFanout check(s) failed.\n",
                 g_failures);
    return 1;
}

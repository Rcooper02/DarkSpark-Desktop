// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTMODELFANOUT_HPP
#define DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTMODELFANOUT_HPP

#include <vector>

namespace darkspark::deck::instruments {

/// Apply one presentation model to every live instrument view of that model.
///
/// The fanout is deliberately tiny and ownership-free: Application owns the
/// telemetry/adapters, pages own the instrument widgets, and this helper merely
/// publishes the latest model to every non-null view. That makes it possible for
/// the same subsystem (for example GPU) to appear on multiple Command Deck pages
/// without duplicating telemetry providers or coupling pages to telemetry.
template <typename Instrument, typename Model>
void fanOutInstrumentModel(const Model& model,
                           const std::vector<Instrument*>& targets) {
    for (Instrument* target : targets) {
        if (target != nullptr) {
            target->setModel(model);
        }
    }
}

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTMODELFANOUT_HPP

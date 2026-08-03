// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTSTATE_HPP
#define DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTSTATE_HPP

namespace darkspark::deck::instruments {

/// The interaction/liveness state an instrument is rendering in.
///
/// This seam is reserved from the first prototype even though the prototype
/// only ever renders Idle. Every paint routine accepts the state, so future
/// hover/touch/expansion behavior (stronger glow, brighter labels, travelling
/// energy, subtle depth, hidden-detail reveal, expansion affordance) becomes a
/// state-driven branch rather than a recomposition.
///
/// Design intent: an instrument should never feel dead. Even Idle carries the
/// faintest sense of presence, so that Hover and Touch have somewhere to grow.
/// The progression from calm to active is what makes an instrument feel alive.
enum class InstrumentState {
    Idle,      ///< resting; quiet presence, the baseline the prototype renders
    Hover,     ///< pointer over the instrument; it should look like it wants to respond
    Touch,     ///< actively touched on the panel; strongest acknowledgement
    Expanded   ///< expanded to reveal reserved detail (trend, per-core, status)
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTSTATE_HPP

// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTSIZEMODE_HPP
#define DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTSIZEMODE_HPP

namespace darkspark::deck::instruments {

/// How much space an instrument occupies, and therefore how much information it
/// shows. Size is not a scale factor: Small is a deliberately reduced
/// composition, not a shrunk Large. The amount of information changes with size.
///
/// Small and Large are implemented in the prototype. Medium and Wide are
/// declared so the type is complete and callers can already name them; until
/// they have dedicated compositions they fall back to the Large strategy.
enum class InstrumentSizeMode { Small, Medium, Large, Wide };

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTSIZEMODE_HPP

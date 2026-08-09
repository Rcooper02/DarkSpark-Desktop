// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/layout/InstrumentFactory.hpp"

#include <QWidget>

#include "deck/instruments/CoolingInstrument.hpp"
#include "deck/instruments/CpuInstrument.hpp"
#include "deck/instruments/GpuInstrument.hpp"
#include "deck/instruments/MemoryInstrument.hpp"
#include "deck/instruments/NetworkInstrument.hpp"
#include "deck/instruments/StorageInstrument.hpp"

namespace darkspark::deck::layout {

QWidget* createInstrument(WidgetId id, InstrumentSizeMode size,
                          QWidget* parent) {
    // Exhaustive over WidgetId, no default: adding a widget id is a compile
    // error until it is handled here. Unknown yields nullptr (graceful skip).
    switch (id) {
    case WidgetId::Cpu:
        return new instruments::CpuInstrument(size, parent);
    case WidgetId::Gpu:
        return new instruments::GpuInstrument(size, parent);
    case WidgetId::Memory:
        return new instruments::MemoryInstrument(size, parent);
    case WidgetId::Cooling:
        return new instruments::CoolingInstrument(size, parent);
    case WidgetId::Storage:
        return new instruments::StorageInstrument(size, parent);
    case WidgetId::Network:
        return new instruments::NetworkInstrument(size, parent);
    case WidgetId::Unknown:
        return nullptr;
    }
    return nullptr;
}

}  // namespace darkspark::deck::layout

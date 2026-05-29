#include "test.h"
#include <memory>
#include <QString>
#include "utils/logger.h"

void logAllAdapters()
{
    MIB_IF_TABLE2* rawTable = nullptr;
    if (GetIfTable2(&rawTable) != NO_ERROR) return;

    struct Deleter { void operator()(MIB_IF_TABLE2* p) const { FreeMibTable(p); } };
    std::unique_ptr<MIB_IF_TABLE2, Deleter> table(rawTable);

    spdlog::debug("[LuidResolver] ── Adapter inventory ({} entries) ──────────",
        table->NumEntries);

    for (ULONG i = 0; i < table->NumEntries; ++i) {
        const MIB_IF_ROW2& row = table->Table[i];
        spdlog::debug("[LuidResolver]   [{:>3}]  luid={:#018x}"
            "  alias='{}'"
            "  friendly='{}'",
            row.InterfaceIndex,
            row.InterfaceLuid.Value,
            QString::fromWCharArray(row.Alias).toStdString(),
            QString::fromWCharArray(row.Description).toStdString());
    }

    spdlog::debug("[LuidResolver] ──────────────────────────────────────────────");
}
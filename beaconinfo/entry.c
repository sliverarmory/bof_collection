/* Modified by Sliver Armory contributors to validate memory traversal. */

#include <windows.h>
#include "../beacon.h"

VOID go(IN PCHAR Buffer, IN ULONG Length)
{
    (void)Buffer;
    (void)Length;

    // Store Beacon start and end address in shared struct.
    MEMORY_BASIC_INFORMATION mbi = { 0 };
    if (VirtualQuery(BeaconPrintf, &mbi, sizeof(mbi)) == 0) {
        BeaconPrintf(CALLBACK_ERROR, "VirtualQuery failed for BeaconPrintf: %lu", GetLastError());
        return;
    }
    if (mbi.AllocationBase == NULL || mbi.BaseAddress == NULL || mbi.RegionSize == 0) {
        BeaconPrintf(CALLBACK_ERROR, "VirtualQuery returned an invalid initial region.");
        return;
    }

    ULONG_PTR BeaconAllocationBase = (ULONG_PTR)mbi.AllocationBase;
    ULONG_PTR BeaconAllocationEnd = 0;

    while (BeaconAllocationBase == (ULONG_PTR)mbi.AllocationBase) {
        ULONG_PTR RegionBase = (ULONG_PTR)mbi.BaseAddress;
        if (RegionBase < BeaconAllocationBase || mbi.RegionSize == 0 ||
            RegionBase > MAXULONG_PTR - mbi.RegionSize) {
            BeaconPrintf(CALLBACK_ERROR, "Invalid virtual-memory region returned by VirtualQuery.");
            return;
        }
        BeaconAllocationEnd = RegionBase + mbi.RegionSize;

        // Query the first address after this region to enumerate the next one.
        RtlSecureZeroMemory(&mbi, sizeof(MEMORY_BASIC_INFORMATION));
        if (VirtualQuery((LPCVOID)BeaconAllocationEnd, &mbi, sizeof(mbi)) == 0) {
            BeaconPrintf(CALLBACK_ERROR, "VirtualQuery failed while walking the allocation: %lu", GetLastError());
            return;
        }
        if (BeaconAllocationBase == (ULONG_PTR)mbi.AllocationBase &&
            ((ULONG_PTR)mbi.BaseAddress < BeaconAllocationEnd || mbi.RegionSize == 0)) {
            BeaconPrintf(CALLBACK_ERROR, "VirtualQuery did not advance to the next region.");
            return;
        }
    }
    BeaconPrintf(CALLBACK_OUTPUT, "Beacon start address: 0x%llx\nBeacon end address:   0x%llx\nBeacon size:          0x%llx\nBeaconPrintf is at:   0x%llx", (unsigned long long)BeaconAllocationBase, (unsigned long long)BeaconAllocationEnd, (unsigned long long)(BeaconAllocationEnd - BeaconAllocationBase), (unsigned long long)(ULONG_PTR)BeaconPrintf);
}

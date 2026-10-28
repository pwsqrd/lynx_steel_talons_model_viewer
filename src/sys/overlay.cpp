#include "overlay.h"
#include "sys.h"
#include "chipper/chipper.h"

// Cart position of each overlay, indexed by overlay id. pack_lynx_cart.py
// packs the overlays after every other asset and patches these records into
// the code image in its final pass, so the resident never has to be rebuilt
// when an overlay changes size. The 0xFF fill keeps the table in loaded data
// (an all-zero const may be placed in .bss) and volatile stops the compiler
// folding the placeholder values into the code.
extern "C" __attribute__((used)) const volatile CartAsset g_overlayTable[OVERLAY_COUNT] = {
#define OVERLAY_TABLE_FILL(name) { 0xFF, 0xFFFF },
    OVERLAY_LIST(OVERLAY_TABLE_FILL)
#undef OVERLAY_TABLE_FILL
};

// Emitted by sys/lyx_overlay.ld at the start of every overlay image.
struct OverlayHeader {
    u8  (*main)(void);
    u8* bssStart;
    u8* bssEnd;
};

extern "C" u8 __overlay_start;
extern "C" u8 __zp_bss_resident_end;

/*
=================
sys::overlay::Run
=================
*/
u8 sys::overlay::Run(u8 id)
{
    // Music data may live in the outgoing overlay.
    sys::chipper::StopAll();

    CartAsset asset;
    asset.block = g_overlayTable[id].block;
    asset.pos   = g_overlayTable[id].pos;
    sys::cart::Read(&__overlay_start, asset);

    const OverlayHeader* header = reinterpret_cast<const OverlayHeader*>(&__overlay_start);
    memset(header->bssStart, 0, static_cast<u16>(header->bssEnd - header->bssStart));

    u8* zp = &__zp_bss_resident_end;
    memset(zp, 0, 0x100 - reinterpret_cast<u16>(zp));

    return header->main();
}

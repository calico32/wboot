#include "wboot_graphics.h"
#include "efiglobal.h"
#include "efiprot.h"
#include "linux.h"
#include "wstdlib.h" // IWYU pragma: keep

static UINT32 min(UINT32 a, UINT32 b) {
    return a < b ? a : b;
}

static void
setup_edid_info(edid_info_t *edid, UINT32 gop_size_of_edid, UINT8 *gop_edid) {
    if (!gop_edid || gop_size_of_edid < 128) {
        memset(edid->dummy, 0, sizeof(edid->dummy));
    } else {
        memcpy(edid->dummy, gop_edid, min(gop_size_of_edid, sizeof(edid->dummy)));
    }
}

static EFI_HANDLE find_handle_with_primary_gop(
    UINTN num, const EFI_HANDLE handles[], EFI_GRAPHICS_OUTPUT_PROTOCOL **found_gop
) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *first_gop;
    EFI_HANDLE h;
    EFI_HANDLE first_gop_handle;

    first_gop_handle = NULL;
    first_gop = NULL;

    for (UINTN i = 0; i < num; i++) {
        h = handles[i];
        EFI_STATUS status;

        EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
        EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode;
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
        void *dummy = NULL;

        status = BS->HandleProtocol(h, &gEfiGraphicsOutputProtocolGuid, (void **)&gop);
        if (status != EFI_SUCCESS) {
            continue;
        }

        mode = gop->Mode;
        info = mode->Info;
        if (info->PixelFormat == PixelBltOnly || info->PixelFormat >= PixelFormatMax) {
            continue;
        }

        /*
         * Systems that use the UEFI Console Splitter may
         * provide multiple GOP devices, not all of which are
         * backed by real hardware. The workaround is to search
         * for a GOP implementing the ConOut protocol, and if
         * one isn't found, to just fall back to the first GOP.
         *
         * Once we've found a GOP supporting ConOut,
         * don't bother looking any further.
         */
        status = BS->HandleProtocol(h, &gEfiSimpleTextOutputProtocolGuid, &dummy);
        if (status == EFI_SUCCESS) {
            if (found_gop) {
                *found_gop = gop;
            }
            return h;
        }

        if (!first_gop_handle) {
            first_gop_handle = h;
            first_gop = gop;
        }
    }

    if (found_gop) {
        *found_gop = first_gop;
    }
    return first_gop_handle;
}

static inline UINT32 __ffs(UINT32 x) {
    return __builtin_ffs((int)x) - 1;
}

static inline UINT32 __fls(UINT32 x) {
    return 31 - __builtin_clz(x);
}

static void find_bits(UINT32 mask, UINT8 *pos, UINT8 *size) {
    if (!mask) {
        *pos = *size = 0;
        return;
    }

    /* UEFI spec guarantees that the set bits are contiguous */
    *pos = __ffs(mask);
    *size = __fls(mask) - *pos + 1;
}

static void
setup_screen_info(screen_info_t *si, const EFI_GRAPHICS_OUTPUT_PROTOCOL *gop) {
    const EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode = gop->Mode;
    const EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = mode->Info;

    si->orig_video_isVGA = VIDEO_TYPE_EFI;

    si->lfb_width = info->HorizontalResolution;
    si->lfb_height = info->VerticalResolution;

    si->lfb_base = (UINT32)mode->FrameBufferBase;
    si->ext_lfb_base = (UINT32)(mode->FrameBufferBase >> 32);
    if (si->ext_lfb_base) {
        si->capabilities |= VIDEO_CAPABILITY_64BIT_BASE;
    }
    si->pages = 1;

    if (info->PixelFormat == PixelBitMask) {
        find_bits(info->PixelInformation.RedMask, &si->red_pos, &si->red_size);
        find_bits(info->PixelInformation.GreenMask, &si->green_pos, &si->green_size);
        find_bits(info->PixelInformation.BlueMask, &si->blue_pos, &si->blue_size);
        find_bits(info->PixelInformation.ReservedMask, &si->rsvd_pos, &si->rsvd_size);
        si->lfb_depth = si->red_size + si->green_size + si->blue_size + si->rsvd_size;
        si->lfb_linelength = (info->PixelsPerScanLine * si->lfb_depth) / 8;
    } else {
        if (info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
            si->red_pos = 0;
            si->blue_pos = 16;
        } else /* PIXEL_BGR_RESERVED_8BIT_PER_COLOR */ {
            si->blue_pos = 0;
            si->red_pos = 16;
        }

        si->green_pos = 8;
        si->rsvd_pos = 24;
        si->red_size = 8;
        si->green_size = 8;
        si->blue_size = 8;
        si->rsvd_size = 8;
        si->lfb_depth = 32;
        si->lfb_linelength = info->PixelsPerScanLine * 4;
    }

    si->lfb_size = si->lfb_linelength * si->lfb_height;
    si->capabilities |= VIDEO_CAPABILITY_SKIP_QUIRKS;
}

static UINT8 pixel_bpp(int pixel_format, EFI_PIXEL_BITMASK pixel_info) {
    if (pixel_format == PixelBitMask) {
        UINT32 mask = pixel_info.RedMask |
                      pixel_info.GreenMask |
                      pixel_info.BlueMask |
                      pixel_info.ReservedMask;
        if (!mask) {
            return 0;
        }

        return __fls(mask) - __ffs(mask) + 1;
    }

    return 32;
}

struct match {
    UINT32 mode;
    UINT32 area;
    UINT8 depth;
};

static UINT32 choose_mode(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
    UINT8 (*match)(const EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *, UINT32, void *),
    void *ctx
) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode = gop->Mode;
    UINT32 max_mode = mode->MaxMode;

    for (UINT32 m = 0; m < max_mode; m++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = NULL;
        UINT64 info_size;
        EFI_STATUS status;

        status = gop->QueryMode(gop, m, &info_size, &info);
        if (status != EFI_SUCCESS) {
            continue;
        }

        if (match(info, m, ctx)) {
            return m;
        }
    }

    return (UINT32)(UINT64)ctx;
}

static UINT8
match_auto(const EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info, UINT32 mode, void *ctx) {
    UINT32 area = info->HorizontalResolution * info->VerticalResolution;
    EFI_PIXEL_BITMASK pi = info->PixelInformation;
    int pf = info->PixelFormat;
    UINT8 depth = pixel_bpp(pf, pi);
    struct match *m = ctx;

    if (pf == PixelBltOnly || pf >= PixelFormatMax) {
        return 0;
    }

    if (area > m->area || (area == m->area && depth > m->depth)) {
        *m = (struct match){mode, area, depth};
    }

    return 0;
}

static UINT32 choose_mode_auto(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop) {
    struct match match = {};

    choose_mode(gop, match_auto, &match);

    return match.mode;
}

static const UINT32 res_width = 1024;
static const UINT32 res_height = 768;
static const UINT32 res_format = -1;
static const UINT32 res_depth = 0;

static UINT8
match_res(const EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info, UINT32 mode, void *ctx) {
    EFI_PIXEL_BITMASK pi = info->PixelInformation;
    int pf = info->PixelFormat;

    if (pf == PixelBltOnly || pf >= PixelFormatMax) {
        return 0;
    }

    // printf(
    //     L"Mode %u: Resolution %ux%u, PixelFormat %u, Depth %u\n", mode,
    //     info->HorizontalResolution, info->VerticalResolution, pf, pixel_bpp(pf, pi)
    // );

    return res_width == info->HorizontalResolution &&
           res_height == info->VerticalResolution &&
           (res_format < 0 || res_format == pf) &&
           (!res_depth || res_depth == pixel_bpp(pf, pi));
}

static UINT32 choose_mode_res(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode = gop->Mode;
    UINT32 cur_mode = mode->Mode;

    if (match_res(mode->Info, cur_mode, NULL)) {
        return cur_mode;
    }

    return choose_mode(gop, match_res, (void *)(UINT64)cur_mode);
}

static void set_mode(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop) {
    UINT32 cur_mode = gop->Mode->Mode;
    UINT32 new_mode = choose_mode_res(gop);

    if (new_mode == cur_mode) {
        return;
    }

    EFI_STATUS status = gop->SetMode(gop, new_mode);
    if (EFI_ERROR(status)) {
        printf(
            L"    [set_mode] error setting graphics mode to %u: %s\r\n", new_mode,
            strerror(status)
        );
    }
}

EFI_STATUS wboot_setup_graphics(boot_params_t *boot_params) {
    screen_info_t *si = memset(&boot_params->screen_info, 0, sizeof(*si));
    edid_info_t *edid = memset(&boot_params->edid_info, 0, sizeof(*edid));

    EFI_HANDLE *handles = NULL;
    EFI_HANDLE handle;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_STATUS status;
    UINTN num;

    status = BS->LocateHandleBuffer(
        ByProtocol, &gEfiGraphicsOutputProtocolGuid, NULL, &num, &handles
    );
    if (status != EFI_SUCCESS) {
        return status;
    }

    handle = find_handle_with_primary_gop(num, handles, &gop);
    if (!handle) {
        return EFI_NOT_FOUND;
    }

    set_mode(gop);

    if (si) {
        setup_screen_info(si, gop);
    }

    if (edid) {
        EFI_EDID_DISCOVERED_PROTOCOL *discovered_edid;
        EFI_EDID_ACTIVE_PROTOCOL *active_edid;
        UINT32 gop_size_of_edid = 0;
        UINT8 *gop_edid = NULL;

        status = BS->HandleProtocol(
            handle, &gEfiEdidActiveProtocolGuid, (void **)&active_edid
        );
        if (status == EFI_SUCCESS) {
            gop_size_of_edid = active_edid->SizeOfEdid;
            gop_edid = active_edid->Edid;
        } else {
            status = BS->HandleProtocol(
                handle, &gEfiEdidDiscoveredProtocolGuid, (void **)&discovered_edid
            );
            if (status == EFI_SUCCESS) {
                gop_size_of_edid = discovered_edid->SizeOfEdid;
                gop_edid = discovered_edid->Edid;
            }
        }

        setup_edid_info(edid, gop_size_of_edid, gop_edid);
    }

    return EFI_SUCCESS;
}

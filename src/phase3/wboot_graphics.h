#pragma once

#include "linux.h"
#include "wboot_config.h"
#include <efi.h>

EFI_STATUS wboot_setup_graphics(const wboot_config_t *config, boot_params_t *params);

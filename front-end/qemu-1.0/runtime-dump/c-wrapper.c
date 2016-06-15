#include "c-wrapper.h"

#include <sysemu.h>

void crete_qemu_system_shutdown_request(void)
{
    qemu_system_shutdown_request();
}

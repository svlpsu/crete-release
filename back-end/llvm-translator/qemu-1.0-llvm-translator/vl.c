/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <zlib.h>

/* Needed early for CONFIG_BSD etc. */
#include "config-host.h"

#ifndef _WIN32
#include <libgen.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <netdb.h>
#include <sys/select.h>

#ifdef CONFIG_BSD
#include <sys/stat.h>
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
#include <libutil.h>
#include <sys/sysctl.h>
#else
#include <util.h>
#endif
#else
#ifdef __linux__
#include <pty.h>
#include <malloc.h>

#include <linux/ppdev.h>
#include <linux/parport.h>
#endif
#ifdef __sun__
#include <sys/stat.h>
#include <sys/ethernet.h>
#include <sys/sockio.h>
#include <netinet/arp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h> // must come after ip.h
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <syslog.h>
#include <stropts.h>
#endif
#endif
#endif

#if defined(__OpenBSD__)
#include <util.h>
#endif

#if defined(CONFIG_VDE)
#include <libvdeplug.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef CONFIG_SDL
#if defined(__APPLE__) || defined(main)
#include <SDL.h>
int qemu_main(int argc, char **argv, char **envp);
int main(int argc, char **argv)
{
    return qemu_main(argc, argv, NULL);
}
#undef main
#define main qemu_main
#endif
#endif /* CONFIG_SDL */

#ifdef CONFIG_COCOA
#undef main
#define main qemu_main
#endif /* CONFIG_COCOA */

#include <glib.h>

#include "hw/hw.h"
#include "hw/boards.h"
#include "hw/usb.h"
#include "hw/pcmcia.h"
#include "hw/pc.h"
#include "hw/isa.h"
#include "hw/baum.h"
#include "hw/bt.h"
#include "hw/watchdog.h"
#include "hw/smbios.h"
#include "hw/xen.h"
#include "hw/qdev.h"
#include "hw/loader.h"
#include "bt-host.h"
#include "net.h"
#include "net/slirp.h"
#include "monitor.h"
#include "console.h"
#include "sysemu.h"
#include "gdbstub.h"
#include "qemu-timer.h"
#include "qemu-char.h"
#include "cache-utils.h"
#include "block.h"
#include "blockdev.h"
#include "block-migration.h"
#include "dma.h"
#include "audio/audio.h"
#include "migration.h"
#include "kvm.h"
#include "qjson.h"
#include "qemu-option.h"
#include "qemu-config.h"
#include "qemu-options.h"
#include "qmp-commands.h"
#include "main-loop.h"
#ifdef CONFIG_VIRTFS
#include "fsdev/qemu-fsdev.h"
#endif

#include "disas.h"

#include "qemu_socket.h"

#include "slirp/libslirp.h"

#include "trace.h"
#include "trace/control.h"
#include "qemu-queue.h"
#include "cpus.h"
#include "arch_init.h"

#ifdef CONFIG_LLVM
struct TCGLLVMContext;

extern struct TCGLLVMContext* tcg_llvm_ctx;
extern int generate_llvm;
extern int execute_llvm;
extern const int has_llvm_engine;

struct TCGLLVMContext* tcg_llvm_initialize(void);
void tcg_llvm_initHelper(struct TCGLLVMContext *l);
void tcg_llvm_close(struct TCGLLVMContext *l);
#endif

#if defined(BCT_RT_DUMP)
struct RuntimeEnv;

extern struct RuntimeEnv *runtime_env;

void initialize_std_output(void);

struct RuntimeEnv* runtime_dump_initialize(void);
void runtime_dump_close(struct RuntimeEnv *rt);
void dump_printInfo(struct RuntimeEnv *rt);
void dump_writeRtEnvToFile(struct RuntimeEnv *rt, const char *outputDirectory);

void tcg_linkWithLibrary(struct TCGLLVMContext *l, const char *libraryName);
#endif

#if defined(CRETE_PROFILE) && 0
extern int flag_rt_dump_start;
#endif // CRETE_PROFILE

#if defined(CRETE_CONFIG)
extern void crete_set_data_dir(const char*);
#endif // defnied(CRETE_CONFIG)

#include "ui/qemu-spice.h"


//#define DEBUG_NET
//#define DEBUG_SLIRP

#define DEFAULT_RAM_SIZE 128

#define MAX_VIRTIO_CONSOLES 1

static const char *data_dir;
const char *bios_name = NULL;
enum vga_retrace_method vga_retrace_method = VGA_RETRACE_DUMB;
DisplayType display_type = DT_DEFAULT;
int display_remote = 0;
const char* keyboard_layout = NULL;
ram_addr_t ram_size;
const char *mem_path = NULL;
#ifdef MAP_POPULATE
int mem_prealloc = 0; /* force preallocation of physical target memory */
#endif
int nb_nics;
NICInfo nd_table[MAX_NICS];
int autostart;
static int rtc_utc = 1;
static int rtc_date_offset = -1; /* -1 means no change */
QEMUClock *rtc_clock;
int vga_interface_type = VGA_NONE;
static int full_screen = 0;
#ifdef CONFIG_SDL
static int no_frame = 0;
#endif
int no_quit = 0;
CharDriverState *serial_hds[MAX_SERIAL_PORTS];
CharDriverState *parallel_hds[MAX_PARALLEL_PORTS];
CharDriverState *virtcon_hds[MAX_VIRTIO_CONSOLES];
int win2k_install_hack = 0;
int rtc_td_hack = 0;
int usb_enabled = 0;
int singlestep = 0;
int smp_cpus = 1;
int max_cpus = 0;
int smp_cores = 1;
int smp_threads = 1;
#ifdef CONFIG_VNC
const char *vnc_display;
#endif
int acpi_enabled = 1;
int no_hpet = 0;
int fd_bootchk = 1;
int no_reboot = 0;
int no_shutdown = 0;
int cursor_hide = 1;
int graphic_rotate = 0;
uint8_t irq0override = 1;
const char *watchdog;
QEMUOptionRom option_rom[MAX_OPTION_ROMS];
int nb_option_roms;
int semihosting_enabled = 0;
int old_param = 0;
const char *qemu_name;
int alt_grab = 0;
int ctrl_grab = 0;
unsigned int nb_prom_envs = 0;
const char *prom_envs[MAX_PROM_ENVS];
int boot_menu;
uint8_t *boot_splash_filedata;
int boot_splash_filedata_size;
uint8_t qemu_extra_params_fw[2];

typedef struct FWBootEntry FWBootEntry;

struct FWBootEntry {
    QTAILQ_ENTRY(FWBootEntry) link;
    int32_t bootindex;
    DeviceState *dev;
    char *suffix;
};

QTAILQ_HEAD(, FWBootEntry) fw_boot_order = QTAILQ_HEAD_INITIALIZER(fw_boot_order);

int nb_numa_nodes;
uint64_t node_mem[MAX_NODES];
uint64_t node_cpumask[MAX_NODES];

uint8_t qemu_uuid[16];

static QEMUBootSetHandler *boot_set_handler;
static void *boot_set_opaque;

static NotifierList exit_notifiers =
    NOTIFIER_LIST_INITIALIZER(exit_notifiers);

static NotifierList machine_init_done_notifiers =
    NOTIFIER_LIST_INITIALIZER(machine_init_done_notifiers);

static int tcg_allowed = 1;
int kvm_allowed = 0;
int xen_allowed = 0;
uint32_t xen_domid;
enum xen_mode xen_mode = XEN_EMULATE;
static int tcg_tb_size;

static int default_serial = 1;
static int default_parallel = 1;
static int default_virtcon = 1;
static int default_monitor = 1;
static int default_vga = 1;
static int default_floppy = 1;
static int default_cdrom = 1;
static int default_sdcard = 1;

static struct {
    const char *driver;
    int *flag;
} default_list[] = {
    { .driver = "isa-serial",           .flag = &default_serial    },
    { .driver = "isa-parallel",         .flag = &default_parallel  },
    { .driver = "isa-fdc",              .flag = &default_floppy    },
    { .driver = "ide-cd",               .flag = &default_cdrom     },
    { .driver = "ide-hd",               .flag = &default_cdrom     },
    { .driver = "ide-drive",            .flag = &default_cdrom     },
    { .driver = "scsi-cd",              .flag = &default_cdrom     },
    { .driver = "virtio-serial-pci",    .flag = &default_virtcon   },
    { .driver = "virtio-serial-s390",   .flag = &default_virtcon   },
    { .driver = "virtio-serial",        .flag = &default_virtcon   },
    { .driver = "VGA",                  .flag = &default_vga       },
    { .driver = "cirrus-vga",           .flag = &default_vga       },
    { .driver = "vmware-svga",          .flag = &default_vga       },
    { .driver = "isa-vga",              .flag = &default_vga       },
    { .driver = "qxl-vga",              .flag = &default_vga       },
};

static void res_free(void)
{
    if (boot_splash_filedata != NULL) {
        g_free(boot_splash_filedata);
        boot_splash_filedata = NULL;
    }
}

static int default_driver_check(QemuOpts *opts, void *opaque)
{
    const char *driver = qemu_opt_get(opts, "driver");
    int i;

    if (!driver)
        return 0;
    for (i = 0; i < ARRAY_SIZE(default_list); i++) {
        if (strcmp(default_list[i].driver, driver) != 0)
            continue;
        *(default_list[i].flag) = 0;
    }
    return 0;
}

/***********************************************************/
/* QEMU state */

static RunState current_run_state = RUN_STATE_PRELAUNCH;

typedef struct {
    RunState from;
    RunState to;
} RunStateTransition;

static const RunStateTransition runstate_transitions_def[] = {
    /*     from      ->     to      */
    { RUN_STATE_DEBUG, RUN_STATE_RUNNING },

    { RUN_STATE_INMIGRATE, RUN_STATE_RUNNING },
    { RUN_STATE_INMIGRATE, RUN_STATE_PRELAUNCH },

    { RUN_STATE_INTERNAL_ERROR, RUN_STATE_PAUSED },
    { RUN_STATE_INTERNAL_ERROR, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_IO_ERROR, RUN_STATE_RUNNING },
    { RUN_STATE_IO_ERROR, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_PAUSED, RUN_STATE_RUNNING },
    { RUN_STATE_PAUSED, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_POSTMIGRATE, RUN_STATE_RUNNING },
    { RUN_STATE_POSTMIGRATE, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_PRELAUNCH, RUN_STATE_RUNNING },
    { RUN_STATE_PRELAUNCH, RUN_STATE_FINISH_MIGRATE },
    { RUN_STATE_PRELAUNCH, RUN_STATE_INMIGRATE },

    { RUN_STATE_FINISH_MIGRATE, RUN_STATE_RUNNING },
    { RUN_STATE_FINISH_MIGRATE, RUN_STATE_POSTMIGRATE },

    { RUN_STATE_RESTORE_VM, RUN_STATE_RUNNING },

    { RUN_STATE_RUNNING, RUN_STATE_DEBUG },
    { RUN_STATE_RUNNING, RUN_STATE_INTERNAL_ERROR },
    { RUN_STATE_RUNNING, RUN_STATE_IO_ERROR },
    { RUN_STATE_RUNNING, RUN_STATE_PAUSED },
    { RUN_STATE_RUNNING, RUN_STATE_FINISH_MIGRATE },
    { RUN_STATE_RUNNING, RUN_STATE_RESTORE_VM },
    { RUN_STATE_RUNNING, RUN_STATE_SAVE_VM },
    { RUN_STATE_RUNNING, RUN_STATE_SHUTDOWN },
    { RUN_STATE_RUNNING, RUN_STATE_WATCHDOG },

    { RUN_STATE_SAVE_VM, RUN_STATE_RUNNING },

    { RUN_STATE_SHUTDOWN, RUN_STATE_PAUSED },
    { RUN_STATE_SHUTDOWN, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_WATCHDOG, RUN_STATE_RUNNING },
    { RUN_STATE_WATCHDOG, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_MAX, RUN_STATE_MAX },
};

static bool runstate_valid_transitions[RUN_STATE_MAX][RUN_STATE_MAX];

bool runstate_check(RunState state)
{
    return current_run_state == state;
}

void runstate_init(void)
{
    const RunStateTransition *p;

    memset(&runstate_valid_transitions, 0, sizeof(runstate_valid_transitions));

    for (p = &runstate_transitions_def[0]; p->from != RUN_STATE_MAX; p++) {
        runstate_valid_transitions[p->from][p->to] = true;
    }
}

/* This function will abort() on invalid state transitions */
void runstate_set(RunState new_state)
{
    assert(new_state < RUN_STATE_MAX);

    if (!runstate_valid_transitions[current_run_state][new_state]) {
        fprintf(stderr, "ERROR: invalid runstate transition: '%s' -> '%s'\n",
                RunState_lookup[current_run_state],
                RunState_lookup[new_state]);
        abort();
    }

    current_run_state = new_state;
}

int runstate_is_running(void)
{
    return runstate_check(RUN_STATE_RUNNING);
}

StatusInfo *qmp_query_status(Error **errp)
{
    StatusInfo *info = g_malloc0(sizeof(*info));

    info->running = runstate_is_running();
    info->singlestep = singlestep;
    info->status = current_run_state;

    return info;
}

/***********************************************************/
/* real time host monotonic timer */

/***********************************************************/
/* host time/date access */
void qemu_get_timedate(struct tm *tm, int offset)
{
    time_t ti;
    struct tm *ret;

    time(&ti);
    ti += offset;
    if (rtc_date_offset == -1) {
        if (rtc_utc)
            ret = gmtime(&ti);
        else
            ret = localtime(&ti);
    } else {
        ti -= rtc_date_offset;
        ret = gmtime(&ti);
    }

    memcpy(tm, ret, sizeof(struct tm));
}

int qemu_timedate_diff(struct tm *tm)
{
    time_t seconds;

    if (rtc_date_offset == -1)
        if (rtc_utc)
            seconds = mktimegm(tm);
        else {
            struct tm tmp = *tm;
            tmp.tm_isdst = -1; /* use timezone to figure it out */
            seconds = mktime(&tmp);
	}
    else
        seconds = mktimegm(tm) + rtc_date_offset;

    return seconds - time(NULL);
}

void rtc_change_mon_event(struct tm *tm)
{
    QObject *data;

    data = qobject_from_jsonf("{ 'offset': %d }", qemu_timedate_diff(tm));
    monitor_protocol_event(QEVENT_RTC_CHANGE, data);
    qobject_decref(data);
}

static void configure_rtc_date_offset(const char *startdate, int legacy)
{
    time_t rtc_start_date;
    struct tm tm;

    if (!strcmp(startdate, "now") && legacy) {
        rtc_date_offset = -1;
    } else {
        if (sscanf(startdate, "%d-%d-%dT%d:%d:%d",
                   &tm.tm_year,
                   &tm.tm_mon,
                   &tm.tm_mday,
                   &tm.tm_hour,
                   &tm.tm_min,
                   &tm.tm_sec) == 6) {
            /* OK */
        } else if (sscanf(startdate, "%d-%d-%d",
                          &tm.tm_year,
                          &tm.tm_mon,
                          &tm.tm_mday) == 3) {
            tm.tm_hour = 0;
            tm.tm_min = 0;
            tm.tm_sec = 0;
        } else {
            goto date_fail;
        }
        tm.tm_year -= 1900;
        tm.tm_mon--;
        rtc_start_date = mktimegm(&tm);
        if (rtc_start_date == -1) {
        date_fail:
            fprintf(stderr, "Invalid date format. Valid formats are:\n"
                            "'2006-06-17T16:01:21' or '2006-06-17'\n");
            exit(1);
        }
        rtc_date_offset = time(NULL) - rtc_start_date;
    }
}

static void configure_rtc(QemuOpts *opts)
{
    const char *value;

    value = qemu_opt_get(opts, "base");
    if (value) {
        if (!strcmp(value, "utc")) {
            rtc_utc = 1;
        } else if (!strcmp(value, "localtime")) {
            rtc_utc = 0;
        } else {
            configure_rtc_date_offset(value, 0);
        }
    }
    value = qemu_opt_get(opts, "clock");
    if (value) {
        if (!strcmp(value, "host")) {
            rtc_clock = host_clock;
        } else if (!strcmp(value, "vm")) {
            rtc_clock = vm_clock;
        } else {
            fprintf(stderr, "qemu: invalid option value '%s'\n", value);
            exit(1);
        }
    }
    value = qemu_opt_get(opts, "driftfix");
    if (value) {
        if (!strcmp(value, "slew")) {
            rtc_td_hack = 1;
        } else if (!strcmp(value, "none")) {
            rtc_td_hack = 0;
        } else {
            fprintf(stderr, "qemu: invalid option value '%s'\n", value);
            exit(1);
        }
    }
}

#ifdef CONFIG_LLVM
static void tcg_llvm_cleanup(void)
{
	if(tcg_llvm_ctx) {
        tcg_llvm_close(tcg_llvm_ctx);
        tcg_llvm_ctx = NULL;
    }
}
#endif

#if defined(BCT_RT_DUMP)
static void runtime_dump_cleanup(void)
{
    if(runtime_env) {
        runtime_dump_close(runtime_env);
        runtime_env = NULL;
    }
}
#endif

/***********************************************************/
/* Bluetooth support */
static int nb_hcis;
static int cur_hci;
static struct HCIInfo *hci_table[MAX_NICS];

static struct bt_vlan_s {
    struct bt_scatternet_s net;
    int id;
    struct bt_vlan_s *next;
} *first_bt_vlan;

/* find or alloc a new bluetooth "VLAN" */
static struct bt_scatternet_s *qemu_find_bt_vlan(int id)
{
    struct bt_vlan_s **pvlan, *vlan;
    for (vlan = first_bt_vlan; vlan != NULL; vlan = vlan->next) {
        if (vlan->id == id)
            return &vlan->net;
    }
    vlan = g_malloc0(sizeof(struct bt_vlan_s));
    vlan->id = id;
    pvlan = &first_bt_vlan;
    while (*pvlan != NULL)
        pvlan = &(*pvlan)->next;
    *pvlan = vlan;
    return &vlan->net;
}

static void null_hci_send(struct HCIInfo *hci, const uint8_t *data, int len)
{
}

static int null_hci_addr_set(struct HCIInfo *hci, const uint8_t *bd_addr)
{
    return -ENOTSUP;
}

static struct HCIInfo null_hci = {
    .cmd_send = null_hci_send,
    .sco_send = null_hci_send,
    .acl_send = null_hci_send,
    .bdaddr_set = null_hci_addr_set,
};

struct HCIInfo *qemu_next_hci(void)
{
    if (cur_hci == nb_hcis)
        return &null_hci;

    return hci_table[cur_hci++];
}

static struct HCIInfo *hci_init(const char *str)
{
    char *endp;
    struct bt_scatternet_s *vlan = 0;

    if (!strcmp(str, "null"))
        /* null */
        return &null_hci;
    else if (!strncmp(str, "host", 4) && (str[4] == '\0' || str[4] == ':'))
        /* host[:hciN] */
        return bt_host_hci(str[4] ? str + 5 : "hci0");
    else if (!strncmp(str, "hci", 3)) {
        /* hci[,vlan=n] */
        if (str[3]) {
            if (!strncmp(str + 3, ",vlan=", 6)) {
                vlan = qemu_find_bt_vlan(strtol(str + 9, &endp, 0));
                if (*endp)
                    vlan = 0;
            }
        } else
            vlan = qemu_find_bt_vlan(0);
        if (vlan)
           return bt_new_hci(vlan);
    }

    fprintf(stderr, "qemu: Unknown bluetooth HCI `%s'.\n", str);

    return 0;
}

static int bt_hci_parse(const char *str)
{
    struct HCIInfo *hci;
    bdaddr_t bdaddr;

    if (nb_hcis >= MAX_NICS) {
        fprintf(stderr, "qemu: Too many bluetooth HCIs (max %i).\n", MAX_NICS);
        return -1;
    }

    hci = hci_init(str);
    if (!hci)
        return -1;

    bdaddr.b[0] = 0x52;
    bdaddr.b[1] = 0x54;
    bdaddr.b[2] = 0x00;
    bdaddr.b[3] = 0x12;
    bdaddr.b[4] = 0x34;
    bdaddr.b[5] = 0x56 + nb_hcis;
    hci->bdaddr_set(hci, bdaddr.b);

    hci_table[nb_hcis++] = hci;

    return 0;
}

static void bt_vhci_add(int vlan_id)
{
    struct bt_scatternet_s *vlan = qemu_find_bt_vlan(vlan_id);

    if (!vlan->slave)
        fprintf(stderr, "qemu: warning: adding a VHCI to "
                        "an empty scatternet %i\n", vlan_id);

    bt_vhci_init(bt_new_hci(vlan));
}

static struct bt_device_s *bt_device_add(const char *opt)
{
    struct bt_scatternet_s *vlan;
    int vlan_id = 0;
    char *endp = strstr(opt, ",vlan=");
    int len = (endp ? endp - opt : strlen(opt)) + 1;
    char devname[10];

    pstrcpy(devname, MIN(sizeof(devname), len), opt);

    if (endp) {
        vlan_id = strtol(endp + 6, &endp, 0);
        if (*endp) {
            fprintf(stderr, "qemu: unrecognised bluetooth vlan Id\n");
            return 0;
        }
    }

    vlan = qemu_find_bt_vlan(vlan_id);

    if (!vlan->slave)
        fprintf(stderr, "qemu: warning: adding a slave device to "
                        "an empty scatternet %i\n", vlan_id);

    if (!strcmp(devname, "keyboard"))
        return bt_keyboard_init(vlan);

    fprintf(stderr, "qemu: unsupported bluetooth device `%s'\n", devname);
    return 0;
}

static int bt_parse(const char *opt)
{
    const char *endp, *p;
    int vlan;

    if (strstart(opt, "hci", &endp)) {
        if (!*endp || *endp == ',') {
            if (*endp)
                if (!strstart(endp, ",vlan=", 0))
                    opt = endp + 1;

            return bt_hci_parse(opt);
       }
    } else if (strstart(opt, "vhci", &endp)) {
        if (!*endp || *endp == ',') {
            if (*endp) {
                if (strstart(endp, ",vlan=", &p)) {
                    vlan = strtol(p, (char **) &endp, 0);
                    if (*endp) {
                        fprintf(stderr, "qemu: bad scatternet '%s'\n", p);
                        return 1;
                    }
                } else {
                    fprintf(stderr, "qemu: bad parameter '%s'\n", endp + 1);
                    return 1;
                }
            } else
                vlan = 0;

            bt_vhci_add(vlan);
            return 0;
        }
    } else if (strstart(opt, "device:", &endp))
        return !bt_device_add(endp);

    fprintf(stderr, "qemu: bad bluetooth parameter '%s'\n", opt);
    return 1;
}

/***********************************************************/
/* QEMU Block devices */

#define HD_OPTS "media=disk"
#define CDROM_OPTS "media=cdrom"
#define FD_OPTS ""
#define PFLASH_OPTS ""
#define MTD_OPTS ""
#define SD_OPTS ""

static int drive_init_func(QemuOpts *opts, void *opaque)
{
    int *use_scsi = opaque;

    return drive_init(opts, *use_scsi) == NULL;
}

static int drive_enable_snapshot(QemuOpts *opts, void *opaque)
{
    if (NULL == qemu_opt_get(opts, "snapshot")) {
        qemu_opt_set(opts, "snapshot", "on");
    }
    return 0;
}

static void default_drive(int enable, int snapshot, int use_scsi,
                          BlockInterfaceType type, int index,
                          const char *optstr)
{
    QemuOpts *opts;

    if (type == IF_DEFAULT) {
        type = use_scsi ? IF_SCSI : IF_IDE;
    }

    if (!enable || drive_get_by_index(type, index)) {
        return;
    }

    opts = drive_add(type, index, NULL, optstr);
    if (snapshot) {
        drive_enable_snapshot(opts, NULL);
    }
    if (!drive_init(opts, use_scsi)) {
        exit(1);
    }
}

void qemu_register_boot_set(QEMUBootSetHandler *func, void *opaque)
{
    boot_set_handler = func;
    boot_set_opaque = opaque;
}

int qemu_boot_set(const char *boot_devices)
{
    if (!boot_set_handler) {
        return -EINVAL;
    }
    return boot_set_handler(boot_set_opaque, boot_devices);
}

static void validate_bootdevices(char *devices)
{
    /* We just do some generic consistency checks */
    const char *p;
    int bitmap = 0;

    for (p = devices; *p != '\0'; p++) {
        /* Allowed boot devices are:
         * a-b: floppy disk drives
         * c-f: IDE disk drives
         * g-m: machine implementation dependant drives
         * n-p: network devices
         * It's up to each machine implementation to check if the given boot
         * devices match the actual hardware implementation and firmware
         * features.
         */
        if (*p < 'a' || *p > 'p') {
            fprintf(stderr, "Invalid boot device '%c'\n", *p);
            exit(1);
        }
        if (bitmap & (1 << (*p - 'a'))) {
            fprintf(stderr, "Boot device '%c' was given twice\n", *p);
            exit(1);
        }
        bitmap |= 1 << (*p - 'a');
    }
}

static void restore_boot_devices(void *opaque)
{
    char *standard_boot_devices = opaque;
    static int first = 1;

    /* Restore boot order and remove ourselves after the first boot */
    if (first) {
        first = 0;
        return;
    }

    qemu_boot_set(standard_boot_devices);

    qemu_unregister_reset(restore_boot_devices, standard_boot_devices);
    g_free(standard_boot_devices);
}

void add_boot_device_path(int32_t bootindex, DeviceState *dev,
                          const char *suffix)
{
    FWBootEntry *node, *i;

    if (bootindex < 0) {
        return;
    }

    assert(dev != NULL || suffix != NULL);

    node = g_malloc0(sizeof(FWBootEntry));
    node->bootindex = bootindex;
    node->suffix = suffix ? g_strdup(suffix) : NULL;
    node->dev = dev;

    QTAILQ_FOREACH(i, &fw_boot_order, link) {
        if (i->bootindex == bootindex) {
            fprintf(stderr, "Two devices with same boot index %d\n", bootindex);
            exit(1);
        } else if (i->bootindex < bootindex) {
            continue;
        }
        QTAILQ_INSERT_BEFORE(i, node, link);
        return;
    }
    QTAILQ_INSERT_TAIL(&fw_boot_order, node, link);
}

/*
 * This function returns null terminated string that consist of new line
 * separated device paths.
 *
 * memory pointed by "size" is assigned total length of the array in bytes
 *
 */
char *get_boot_devices_list(uint32_t *size)
{
    FWBootEntry *i;
    uint32_t total = 0;
    char *list = NULL;

    QTAILQ_FOREACH(i, &fw_boot_order, link) {
        char *devpath = NULL, *bootpath;
        int len;

        if (i->dev) {
            devpath = qdev_get_fw_dev_path(i->dev);
            assert(devpath);
        }

        if (i->suffix && devpath) {
            size_t bootpathlen = strlen(devpath) + strlen(i->suffix) + 1;

            bootpath = g_malloc(bootpathlen);
            snprintf(bootpath, bootpathlen, "%s%s", devpath, i->suffix);
            g_free(devpath);
        } else if (devpath) {
            bootpath = devpath;
        } else {
            assert(i->suffix);
            bootpath = g_strdup(i->suffix);
        }

        if (total) {
            list[total-1] = '\n';
        }
        len = strlen(bootpath) + 1;
        list = g_realloc(list, total + len);
        memcpy(&list[total], bootpath, len);
        total += len;
        g_free(bootpath);
    }

    *size = total;

    return list;
}

static void numa_add(const char *optarg)
{
    char option[128];
    char *endptr;
    unsigned long long value, endvalue;
    int nodenr;

    optarg = get_opt_name(option, 128, optarg, ',') + 1;
    if (!strcmp(option, "node")) {
        if (get_param_value(option, 128, "nodeid", optarg) == 0) {
            nodenr = nb_numa_nodes;
        } else {
            nodenr = strtoull(option, NULL, 10);
        }

        if (get_param_value(option, 128, "mem", optarg) == 0) {
            node_mem[nodenr] = 0;
        } else {
            int64_t sval;
            sval = strtosz(option, &endptr);
            if (sval < 0 || *endptr) {
                fprintf(stderr, "qemu: invalid numa mem size: %s\n", optarg);
                exit(1);
            }
            node_mem[nodenr] = sval;
        }
        if (get_param_value(option, 128, "cpus", optarg) == 0) {
            node_cpumask[nodenr] = 0;
        } else {
            value = strtoull(option, &endptr, 10);
            if (value >= 64) {
                value = 63;
                fprintf(stderr, "only 64 CPUs in NUMA mode supported.\n");
            } else {
                if (*endptr == '-') {
                    endvalue = strtoull(endptr+1, &endptr, 10);
                    if (endvalue >= 63) {
                        endvalue = 62;
                        fprintf(stderr,
                            "only 63 CPUs in NUMA mode supported.\n");
                    }
                    value = (2ULL << endvalue) - (1ULL << value);
                } else {
                    value = 1ULL << value;
                }
            }
            node_cpumask[nodenr] = value;
        }
        nb_numa_nodes++;
    }
    return;
}

static void smp_parse(const char *optarg)
{
    int smp, sockets = 0, threads = 0, cores = 0;
    char *endptr;
    char option[128];

    smp = strtoul(optarg, &endptr, 10);
    if (endptr != optarg) {
        if (*endptr == ',') {
            endptr++;
        }
    }
    if (get_param_value(option, 128, "sockets", endptr) != 0)
        sockets = strtoull(option, NULL, 10);
    if (get_param_value(option, 128, "cores", endptr) != 0)
        cores = strtoull(option, NULL, 10);
    if (get_param_value(option, 128, "threads", endptr) != 0)
        threads = strtoull(option, NULL, 10);
    if (get_param_value(option, 128, "maxcpus", endptr) != 0)
        max_cpus = strtoull(option, NULL, 10);

    /* compute missing values, prefer sockets over cores over threads */
    if (smp == 0 || sockets == 0) {
        sockets = sockets > 0 ? sockets : 1;
        cores = cores > 0 ? cores : 1;
        threads = threads > 0 ? threads : 1;
        if (smp == 0) {
            smp = cores * threads * sockets;
        }
    } else {
        if (cores == 0) {
            threads = threads > 0 ? threads : 1;
            cores = smp / (sockets * threads);
        } else {
            threads = smp / (cores * sockets);
        }
    }
    smp_cpus = smp;
    smp_cores = cores > 0 ? cores : 1;
    smp_threads = threads > 0 ? threads : 1;
    if (max_cpus == 0)
        max_cpus = smp_cpus;
}

/***********************************************************/
/* USB devices */

static int usb_device_add(const char *devname)
{
    const char *p;
    USBDevice *dev = NULL;

    if (!usb_enabled)
        return -1;

    /* drivers with .usbdevice_name entry in USBDeviceInfo */
    dev = usbdevice_create(devname);
    if (dev)
        goto done;

    /* the other ones */
#ifndef CONFIG_LINUX
    /* only the linux version is qdev-ified, usb-bsd still needs this */
    if (strstart(devname, "host:", &p)) {
        dev = usb_host_device_open(p);
    } else
#endif
    if (!strcmp(devname, "bt") || strstart(devname, "bt:", &p)) {
        dev = usb_bt_init(devname[2] ? hci_init(p) :
                        bt_new_hci(qemu_find_bt_vlan(0)));
    } else {
        return -1;
    }
    if (!dev)
        return -1;

done:
    return 0;
}

static int usb_device_del(const char *devname)
{
    int bus_num, addr;
    const char *p;

    if (strstart(devname, "host:", &p))
        return usb_host_device_close(p);

    if (!usb_enabled)
        return -1;

    p = strchr(devname, '.');
    if (!p)
        return -1;
    bus_num = strtoul(devname, NULL, 0);
    addr = strtoul(p + 1, NULL, 0);

    return usb_device_delete_addr(bus_num, addr);
}

static int usb_parse(const char *cmdline)
{
    int r;
    r = usb_device_add(cmdline);
    if (r < 0) {
        fprintf(stderr, "qemu: could not add USB device '%s'\n", cmdline);
    }
    return r;
}

void do_usb_add(Monitor *mon, const QDict *qdict)
{
    const char *devname = qdict_get_str(qdict, "devname");
    if (usb_device_add(devname) < 0) {
        error_report("could not add USB device '%s'", devname);
    }
}

void do_usb_del(Monitor *mon, const QDict *qdict)
{
    const char *devname = qdict_get_str(qdict, "devname");
    if (usb_device_del(devname) < 0) {
        error_report("could not delete USB device '%s'", devname);
    }
}

/***********************************************************/
/* PCMCIA/Cardbus */

static struct pcmcia_socket_entry_s {
    PCMCIASocket *socket;
    struct pcmcia_socket_entry_s *next;
} *pcmcia_sockets = 0;

void pcmcia_socket_register(PCMCIASocket *socket)
{
    struct pcmcia_socket_entry_s *entry;

    entry = g_malloc(sizeof(struct pcmcia_socket_entry_s));
    entry->socket = socket;
    entry->next = pcmcia_sockets;
    pcmcia_sockets = entry;
}

void pcmcia_socket_unregister(PCMCIASocket *socket)
{
    struct pcmcia_socket_entry_s *entry, **ptr;

    ptr = &pcmcia_sockets;
    for (entry = *ptr; entry; ptr = &entry->next, entry = *ptr)
        if (entry->socket == socket) {
            *ptr = entry->next;
            g_free(entry);
        }
}

void pcmcia_info(Monitor *mon)
{
    struct pcmcia_socket_entry_s *iter;

    if (!pcmcia_sockets)
        monitor_printf(mon, "No PCMCIA sockets\n");

    for (iter = pcmcia_sockets; iter; iter = iter->next)
        monitor_printf(mon, "%s: %s\n", iter->socket->slot_string,
                       iter->socket->attached ? iter->socket->card_string :
                       "Empty");
}

/***********************************************************/
/* machine registration */

static QEMUMachine *first_machine = NULL;
QEMUMachine *current_machine = NULL;

int qemu_register_machine(QEMUMachine *m)
{
    QEMUMachine **pm;
    pm = &first_machine;
    while (*pm != NULL)
        pm = &(*pm)->next;
    m->next = NULL;
    *pm = m;
    return 0;
}

static QEMUMachine *find_machine(const char *name)
{
    QEMUMachine *m;

    for(m = first_machine; m != NULL; m = m->next) {
        if (!strcmp(m->name, name))
            return m;
        if (m->alias && !strcmp(m->alias, name))
            return m;
    }
    return NULL;
}

static QEMUMachine *find_default_machine(void)
{
    QEMUMachine *m;

    for(m = first_machine; m != NULL; m = m->next) {
        if (m->is_default) {
            return m;
        }
    }
    return NULL;
}

/***********************************************************/
/* main execution loop */

static void gui_update(void *opaque)
{
    uint64_t interval = GUI_REFRESH_INTERVAL;
    DisplayState *ds = opaque;
    DisplayChangeListener *dcl = ds->listeners;

#if defined(CRETE_PROFILE) && 0
//    if(flag_rt_dump_start)
//        interval = 25000;
#endif // CRETE_PROFILE

    dpy_refresh(ds);

    while (dcl != NULL) {
        if (dcl->gui_timer_interval &&
            dcl->gui_timer_interval < interval)
            interval = dcl->gui_timer_interval;
        dcl = dcl->next;
    }
    qemu_mod_timer(ds->gui_timer, interval + qemu_get_clock_ms(rt_clock));
}

struct vm_change_state_entry {
    VMChangeStateHandler *cb;
    void *opaque;
    QLIST_ENTRY (vm_change_state_entry) entries;
};

static QLIST_HEAD(vm_change_state_head, vm_change_state_entry) vm_change_state_head;

VMChangeStateEntry *qemu_add_vm_change_state_handler(VMChangeStateHandler *cb,
                                                     void *opaque)
{
    VMChangeStateEntry *e;

    e = g_malloc0(sizeof (*e));

    e->cb = cb;
    e->opaque = opaque;
    QLIST_INSERT_HEAD(&vm_change_state_head, e, entries);
    return e;
}

void qemu_del_vm_change_state_handler(VMChangeStateEntry *e)
{
    QLIST_REMOVE (e, entries);
    g_free (e);
}

void vm_state_notify(int running, RunState state)
{
    VMChangeStateEntry *e;

    trace_vm_state_notify(running, state);

    for (e = vm_change_state_head.lh_first; e; e = e->entries.le_next) {
        e->cb(e->opaque, running, state);
    }
}

void vm_start(void)
{
    if (!runstate_is_running()) {
        cpu_enable_ticks();
        runstate_set(RUN_STATE_RUNNING);
        vm_state_notify(1, RUN_STATE_RUNNING);
        resume_all_vcpus();
        monitor_protocol_event(QEVENT_RESUME, NULL);
    }
}

/* reset/shutdown handler */

typedef struct QEMUResetEntry {
    QTAILQ_ENTRY(QEMUResetEntry) entry;
    QEMUResetHandler *func;
    void *opaque;
} QEMUResetEntry;

static QTAILQ_HEAD(reset_handlers, QEMUResetEntry) reset_handlers =
    QTAILQ_HEAD_INITIALIZER(reset_handlers);
static int reset_requested;
static int shutdown_requested, shutdown_signal = -1;
static pid_t shutdown_pid;
static int powerdown_requested;
static int debug_requested;
static RunState vmstop_requested = RUN_STATE_MAX;

int qemu_shutdown_requested_get(void)
{
    return shutdown_requested;
}

int qemu_reset_requested_get(void)
{
    return reset_requested;
}

int qemu_shutdown_requested(void)
{
    int r = shutdown_requested;
    shutdown_requested = 0;
    return r;
}

void qemu_kill_report(void)
{
    if (shutdown_signal != -1) {
        fprintf(stderr, "qemu: terminating on signal %d", shutdown_signal);
        if (shutdown_pid == 0) {
            /* This happens for eg ^C at the terminal, so it's worth
             * avoiding printing an odd message in that case.
             */
            fputc('\n', stderr);
        } else {
            fprintf(stderr, " from pid " FMT_pid "\n", shutdown_pid);
        }
        shutdown_signal = -1;
    }
}

int qemu_reset_requested(void)
{
    int r = reset_requested;
    reset_requested = 0;
    return r;
}

int qemu_powerdown_requested(void)
{
    int r = powerdown_requested;
    powerdown_requested = 0;
    return r;
}

static int qemu_debug_requested(void)
{
    int r = debug_requested;
    debug_requested = 0;
    return r;
}

/* We use RUN_STATE_MAX but any invalid value will do */
static bool qemu_vmstop_requested(RunState *r)
{
    if (vmstop_requested < RUN_STATE_MAX) {
        *r = vmstop_requested;
        vmstop_requested = RUN_STATE_MAX;
        return true;
    }

    return false;
}

void qemu_register_reset(QEMUResetHandler *func, void *opaque)
{
    QEMUResetEntry *re = g_malloc0(sizeof(QEMUResetEntry));

    re->func = func;
    re->opaque = opaque;
    QTAILQ_INSERT_TAIL(&reset_handlers, re, entry);
}

void qemu_unregister_reset(QEMUResetHandler *func, void *opaque)
{
    QEMUResetEntry *re;

    QTAILQ_FOREACH(re, &reset_handlers, entry) {
        if (re->func == func && re->opaque == opaque) {
            QTAILQ_REMOVE(&reset_handlers, re, entry);
            g_free(re);
            return;
        }
    }
}

void qemu_system_reset(bool report)
{
    QEMUResetEntry *re, *nre;

    /* reset all devices */
    QTAILQ_FOREACH_SAFE(re, &reset_handlers, entry, nre) {
        re->func(re->opaque);
    }
    if (report) {
        monitor_protocol_event(QEVENT_RESET, NULL);
    }
    cpu_synchronize_all_post_reset();
}

void qemu_system_reset_request(void)
{
    if (no_reboot) {
        shutdown_requested = 1;
    } else {
        reset_requested = 1;
    }
    cpu_stop_current();
    qemu_notify_event();
}

void qemu_system_killed(int signal, pid_t pid)
{
    shutdown_signal = signal;
    shutdown_pid = pid;
    no_shutdown = 0;
    qemu_system_shutdown_request();
}

void qemu_system_shutdown_request(void)
{
    shutdown_requested = 1;
    qemu_notify_event();
}

void qemu_system_powerdown_request(void)
{
    powerdown_requested = 1;
    qemu_notify_event();
}

void qemu_system_debug_request(void)
{
    debug_requested = 1;
    qemu_notify_event();
}

void qemu_system_vmstop_request(RunState state)
{
    vmstop_requested = state;
    qemu_notify_event();
}

qemu_irq qemu_system_powerdown;

static bool main_loop_should_exit(void)
{
    RunState r;
    if (qemu_debug_requested()) {
        vm_stop(RUN_STATE_DEBUG);
    }
    if (qemu_shutdown_requested()) {
        qemu_kill_report();
        monitor_protocol_event(QEVENT_SHUTDOWN, NULL);
        if (no_shutdown) {
            vm_stop(RUN_STATE_SHUTDOWN);
        } else {
            return true;
        }
    }
    if (qemu_reset_requested()) {
        pause_all_vcpus();
        cpu_synchronize_all_states();
        qemu_system_reset(VMRESET_REPORT);
        resume_all_vcpus();
        if (runstate_check(RUN_STATE_INTERNAL_ERROR) ||
            runstate_check(RUN_STATE_SHUTDOWN)) {
            runstate_set(RUN_STATE_PAUSED);
        }
    }
    if (qemu_powerdown_requested()) {
        monitor_protocol_event(QEVENT_POWERDOWN, NULL);
        qemu_irq_raise(qemu_system_powerdown);
    }
    if (qemu_vmstop_requested(&r)) {
        vm_stop(r);
    }
    return false;
}

static void main_loop(void)
{
#if defined(DBG_BO_TCG_WORKFLOW)
	printf("[workflow] vl.c: main_loop() is entered. Process_id = %d\n", qemu_get_thread_id());
#endif

	bool nonblocking;
    int last_io = 0;
#ifdef CONFIG_PROFILER
    int64_t ti;
#endif
    do {
        nonblocking = !kvm_enabled() && last_io > 0;
#ifdef CONFIG_PROFILER
        ti = profile_getclock();
#endif
        last_io = main_loop_wait(nonblocking);
#ifdef CONFIG_PROFILER
        dev_time += profile_getclock() - ti;
#endif
    } while (!main_loop_should_exit());

#if defined(DBG_BO_TCG_WORKFLOW)
	printf("[workflow] vl.c: main_loop() is finished. Process_id = %d\n", qemu_get_thread_id());
#endif

}

static void version(void)
{
    printf("QEMU emulator version " QEMU_VERSION QEMU_PKGVERSION ", Copyright (c) 2003-2008 Fabrice Bellard\n");
}

static void help(int exitcode)
{
    const char *options_help =
#define DEF(option, opt_arg, opt_enum, opt_help, arch_mask)     \
        opt_help
#define DEFHEADING(text) stringify(text) "\n"
#include "qemu-options.def"
#undef DEF
#undef DEFHEADING
#undef GEN_DOCS
        ;
    version();
    printf("usage: %s [options] [disk_image]\n"
           "\n"
           "'disk_image' is a raw hard disk image for IDE hard disk 0\n"
           "\n"
           "%s\n"
           "During emulation, the following keys are useful:\n"
           "ctrl-alt-f      toggle full screen\n"
           "ctrl-alt-n      switch to virtual console 'n'\n"
           "ctrl-alt        toggle mouse and keyboard grab\n"
           "\n"
           "When using -nographic, press 'ctrl-a h' to get some help.\n",
           "qemu",
           options_help);
    exit(exitcode);
}

#define HAS_ARG 0x0001

typedef struct QEMUOption {
    const char *name;
    int flags;
    int index;
    uint32_t arch_mask;
} QEMUOption;

static const QEMUOption qemu_options[] = {
    { "h", 0, QEMU_OPTION_h, QEMU_ARCH_ALL },
#define DEF(option, opt_arg, opt_enum, opt_help, arch_mask)     \
    { option, opt_arg, opt_enum, arch_mask },
#define DEFHEADING(text)
#include "qemu-options.def"
#undef DEF
#undef DEFHEADING
#undef GEN_DOCS
    { NULL },
};
static void select_vgahw (const char *p)
{
    const char *opts;

    default_vga = 0;
    vga_interface_type = VGA_NONE;
    if (strstart(p, "std", &opts)) {
        vga_interface_type = VGA_STD;
    } else if (strstart(p, "cirrus", &opts)) {
        vga_interface_type = VGA_CIRRUS;
    } else if (strstart(p, "vmware", &opts)) {
        vga_interface_type = VGA_VMWARE;
    } else if (strstart(p, "xenfb", &opts)) {
        vga_interface_type = VGA_XENFB;
    } else if (strstart(p, "qxl", &opts)) {
        vga_interface_type = VGA_QXL;
    } else if (!strstart(p, "none", &opts)) {
    invalid_vga:
        fprintf(stderr, "Unknown vga type: %s\n", p);
        exit(1);
    }
    while (*opts) {
        const char *nextopt;

        if (strstart(opts, ",retrace=", &nextopt)) {
            opts = nextopt;
            if (strstart(opts, "dumb", &nextopt))
                vga_retrace_method = VGA_RETRACE_DUMB;
            else if (strstart(opts, "precise", &nextopt))
                vga_retrace_method = VGA_RETRACE_PRECISE;
            else goto invalid_vga;
        } else goto invalid_vga;
        opts = nextopt;
    }
}

static DisplayType select_display(const char *p)
{
    const char *opts;
    DisplayType display = DT_DEFAULT;

    if (strstart(p, "sdl", &opts)) {
#ifdef CONFIG_SDL
        display = DT_SDL;
        while (*opts) {
            const char *nextopt;

            if (strstart(opts, ",frame=", &nextopt)) {
                opts = nextopt;
                if (strstart(opts, "on", &nextopt)) {
                    no_frame = 0;
                } else if (strstart(opts, "off", &nextopt)) {
                    no_frame = 1;
                } else {
                    goto invalid_sdl_args;
                }
            } else if (strstart(opts, ",alt_grab=", &nextopt)) {
                opts = nextopt;
                if (strstart(opts, "on", &nextopt)) {
                    alt_grab = 1;
                } else if (strstart(opts, "off", &nextopt)) {
                    alt_grab = 0;
                } else {
                    goto invalid_sdl_args;
                }
            } else if (strstart(opts, ",ctrl_grab=", &nextopt)) {
                opts = nextopt;
                if (strstart(opts, "on", &nextopt)) {
                    ctrl_grab = 1;
                } else if (strstart(opts, "off", &nextopt)) {
                    ctrl_grab = 0;
                } else {
                    goto invalid_sdl_args;
                }
            } else if (strstart(opts, ",window_close=", &nextopt)) {
                opts = nextopt;
                if (strstart(opts, "on", &nextopt)) {
                    no_quit = 0;
                } else if (strstart(opts, "off", &nextopt)) {
                    no_quit = 1;
                } else {
                    goto invalid_sdl_args;
                }
            } else {
            invalid_sdl_args:
                fprintf(stderr, "Invalid SDL option string: %s\n", p);
                exit(1);
            }
            opts = nextopt;
        }
#else
        fprintf(stderr, "SDL support is disabled\n");
        exit(1);
#endif
    } else if (strstart(p, "vnc", &opts)) {
#ifdef CONFIG_VNC
        display_remote++;

        if (*opts) {
            const char *nextopt;

            if (strstart(opts, "=", &nextopt)) {
                vnc_display = nextopt;
            }
        }
        if (!vnc_display) {
            fprintf(stderr, "VNC requires a display argument vnc=<display>\n");
            exit(1);
        }
#else
        fprintf(stderr, "VNC support is disabled\n");
        exit(1);
#endif
    } else if (strstart(p, "curses", &opts)) {
#ifdef CONFIG_CURSES
        display = DT_CURSES;
#else
        fprintf(stderr, "Curses support is disabled\n");
        exit(1);
#endif
    } else if (strstart(p, "none", &opts)) {
        display = DT_NONE;
    } else {
        fprintf(stderr, "Unknown display type: %s\n", p);
        exit(1);
    }

    return display;
}

static int balloon_parse(const char *arg)
{
    QemuOpts *opts;

    if (strcmp(arg, "none") == 0) {
        return 0;
    }

    if (!strncmp(arg, "virtio", 6)) {
        if (arg[6] == ',') {
            /* have params -> parse them */
            opts = qemu_opts_parse(qemu_find_opts("device"), arg+7, 0);
            if (!opts)
                return  -1;
        } else {
            /* create empty opts */
            opts = qemu_opts_create(qemu_find_opts("device"), NULL, 0);
        }
        qemu_opt_set(opts, "driver", "virtio-balloon");
        return 0;
    }

    return -1;
}

char *qemu_find_file(int type, const char *name)
{
    int len;
    const char *subdir;
    char *buf;

    /* If name contains path separators then try it as a straight path.  */
    if ((strchr(name, '/') || strchr(name, '\\'))
        && access(name, R_OK) == 0) {
        return g_strdup(name);
    }
    switch (type) {
    case QEMU_FILE_TYPE_BIOS:
        subdir = "";
        break;
    case QEMU_FILE_TYPE_KEYMAP:
        subdir = "keymaps/";
        break;
    default:
        abort();
    }
    len = strlen(data_dir) + strlen(name) + strlen(subdir) + 2;
    buf = g_malloc0(len);
    snprintf(buf, len, "%s/%s%s", data_dir, subdir, name);
    if (access(buf, R_OK)) {
        g_free(buf);
        return NULL;
    }
    return buf;
}

static int device_help_func(QemuOpts *opts, void *opaque)
{
    return qdev_device_help(opts);
}

static int device_init_func(QemuOpts *opts, void *opaque)
{
    DeviceState *dev;

    dev = qdev_device_add(opts);
    if (!dev)
        return -1;
    return 0;
}

static int chardev_init_func(QemuOpts *opts, void *opaque)
{
    CharDriverState *chr;

    chr = qemu_chr_new_from_opts(opts, NULL);
    if (!chr)
        return -1;
    return 0;
}

#ifdef CONFIG_VIRTFS
static int fsdev_init_func(QemuOpts *opts, void *opaque)
{
    int ret;
    ret = qemu_fsdev_add(opts);

    return ret;
}
#endif

static int mon_init_func(QemuOpts *opts, void *opaque)
{
    CharDriverState *chr;
    const char *chardev;
    const char *mode;
    int flags;

    mode = qemu_opt_get(opts, "mode");
    if (mode == NULL) {
        mode = "readline";
    }
    if (strcmp(mode, "readline") == 0) {
        flags = MONITOR_USE_READLINE;
    } else if (strcmp(mode, "control") == 0) {
        flags = MONITOR_USE_CONTROL;
    } else {
        fprintf(stderr, "unknown monitor mode \"%s\"\n", mode);
        exit(1);
    }

    if (qemu_opt_get_bool(opts, "pretty", 0))
        flags |= MONITOR_USE_PRETTY;

    if (qemu_opt_get_bool(opts, "default", 0))
        flags |= MONITOR_IS_DEFAULT;

    chardev = qemu_opt_get(opts, "chardev");
    chr = qemu_chr_find(chardev);
    if (chr == NULL) {
        fprintf(stderr, "chardev \"%s\" not found\n", chardev);
        exit(1);
    }

    monitor_init(chr, flags);
    return 0;
}

static void monitor_parse(const char *optarg, const char *mode)
{
    static int monitor_device_index = 0;
    QemuOpts *opts;
    const char *p;
    char label[32];
    int def = 0;

    if (strstart(optarg, "chardev:", &p)) {
        snprintf(label, sizeof(label), "%s", p);
    } else {
        snprintf(label, sizeof(label), "compat_monitor%d",
                 monitor_device_index);
        if (monitor_device_index == 0) {
            def = 1;
        }
        opts = qemu_chr_parse_compat(label, optarg);
        if (!opts) {
            fprintf(stderr, "parse error: %s\n", optarg);
            exit(1);
        }
    }

    opts = qemu_opts_create(qemu_find_opts("mon"), label, 1);
    if (!opts) {
        fprintf(stderr, "duplicate chardev: %s\n", label);
        exit(1);
    }
    qemu_opt_set(opts, "mode", mode);
    qemu_opt_set(opts, "chardev", label);
    if (def)
        qemu_opt_set(opts, "default", "on");
    monitor_device_index++;
}

struct device_config {
    enum {
        DEV_USB,       /* -usbdevice     */
        DEV_BT,        /* -bt            */
        DEV_SERIAL,    /* -serial        */
        DEV_PARALLEL,  /* -parallel      */
        DEV_VIRTCON,   /* -virtioconsole */
        DEV_DEBUGCON,  /* -debugcon */
    } type;
    const char *cmdline;
    QTAILQ_ENTRY(device_config) next;
};
QTAILQ_HEAD(, device_config) device_configs = QTAILQ_HEAD_INITIALIZER(device_configs);

static void add_device_config(int type, const char *cmdline)
{
    struct device_config *conf;

    conf = g_malloc0(sizeof(*conf));
    conf->type = type;
    conf->cmdline = cmdline;
    QTAILQ_INSERT_TAIL(&device_configs, conf, next);
}

static int foreach_device_config(int type, int (*func)(const char *cmdline))
{
    struct device_config *conf;
    int rc;

    QTAILQ_FOREACH(conf, &device_configs, next) {
        if (conf->type != type)
            continue;
        rc = func(conf->cmdline);
        if (0 != rc)
            return rc;
    }
    return 0;
}

static int serial_parse(const char *devname)
{
    static int index = 0;
    char label[32];

    if (strcmp(devname, "none") == 0)
        return 0;
    if (index == MAX_SERIAL_PORTS) {
        fprintf(stderr, "qemu: too many serial ports\n");
        exit(1);
    }
    snprintf(label, sizeof(label), "serial%d", index);
    serial_hds[index] = qemu_chr_new(label, devname, NULL);
    if (!serial_hds[index]) {
        fprintf(stderr, "qemu: could not open serial device '%s': %s\n",
                devname, strerror(errno));
        return -1;
    }
    index++;
    return 0;
}

static int parallel_parse(const char *devname)
{
    static int index = 0;
    char label[32];

    if (strcmp(devname, "none") == 0)
        return 0;
    if (index == MAX_PARALLEL_PORTS) {
        fprintf(stderr, "qemu: too many parallel ports\n");
        exit(1);
    }
    snprintf(label, sizeof(label), "parallel%d", index);
    parallel_hds[index] = qemu_chr_new(label, devname, NULL);
    if (!parallel_hds[index]) {
        fprintf(stderr, "qemu: could not open parallel device '%s': %s\n",
                devname, strerror(errno));
        return -1;
    }
    index++;
    return 0;
}

static int virtcon_parse(const char *devname)
{
    QemuOptsList *device = qemu_find_opts("device");
    static int index = 0;
    char label[32];
    QemuOpts *bus_opts, *dev_opts;

    if (strcmp(devname, "none") == 0)
        return 0;
    if (index == MAX_VIRTIO_CONSOLES) {
        fprintf(stderr, "qemu: too many virtio consoles\n");
        exit(1);
    }

    bus_opts = qemu_opts_create(device, NULL, 0);
    qemu_opt_set(bus_opts, "driver", "virtio-serial");

    dev_opts = qemu_opts_create(device, NULL, 0);
    qemu_opt_set(dev_opts, "driver", "virtconsole");

    snprintf(label, sizeof(label), "virtcon%d", index);
    virtcon_hds[index] = qemu_chr_new(label, devname, NULL);
    if (!virtcon_hds[index]) {
        fprintf(stderr, "qemu: could not open virtio console '%s': %s\n",
                devname, strerror(errno));
        return -1;
    }
    qemu_opt_set(dev_opts, "chardev", label);

    index++;
    return 0;
}

static int debugcon_parse(const char *devname)
{
    QemuOpts *opts;

    if (!qemu_chr_new("debugcon", devname, NULL)) {
        exit(1);
    }
    opts = qemu_opts_create(qemu_find_opts("device"), "debugcon", 1);
    if (!opts) {
        fprintf(stderr, "qemu: already have a debugcon device\n");
        exit(1);
    }
    qemu_opt_set(opts, "driver", "isa-debugcon");
    qemu_opt_set(opts, "chardev", "debugcon");
    return 0;
}

static QEMUMachine *machine_parse(const char *name)
{
    QEMUMachine *m, *machine = NULL;

    if (name) {
        machine = find_machine(name);
    }
    if (machine) {
        return machine;
    }
    printf("Supported machines are:\n");
    for (m = first_machine; m != NULL; m = m->next) {
        if (m->alias) {
            printf("%-10s %s (alias of %s)\n", m->alias, m->desc, m->name);
        }
        printf("%-10s %s%s\n", m->name, m->desc,
               m->is_default ? " (default)" : "");
    }
    exit(!name || *name != '?');
}

static int tcg_init(void)
{
    tcg_exec_init(tcg_tb_size * 1024 * 1024);
    return 0;
}

static struct {
    const char *opt_name;
    const char *name;
    int (*available)(void);
    int (*init)(void);
    int *allowed;
} accel_list[] = {
    { "tcg", "tcg", tcg_available, tcg_init, &tcg_allowed },
    { "xen", "Xen", xen_available, xen_init, &xen_allowed },
    { "kvm", "KVM", kvm_available, kvm_init, &kvm_allowed },
};

static int configure_accelerator(void)
{
    const char *p = NULL;
    char buf[10];
    int i, ret;
    bool accel_initalised = 0;
    bool init_failed = 0;

    QemuOptsList *list = qemu_find_opts("machine");
    if (!QTAILQ_EMPTY(&list->head)) {
        p = qemu_opt_get(QTAILQ_FIRST(&list->head), "accel");
    }

    if (p == NULL) {
        /* Use the default "accelerator", tcg */
        p = "tcg";
    }

    while (!accel_initalised && *p != '\0') {
        if (*p == ':') {
            p++;
        }
        p = get_opt_name(buf, sizeof (buf), p, ':');
        for (i = 0; i < ARRAY_SIZE(accel_list); i++) {
            if (strcmp(accel_list[i].opt_name, buf) == 0) {
                *(accel_list[i].allowed) = 1;
                ret = accel_list[i].init();
                if (ret < 0) {
                    init_failed = 1;
                    if (!accel_list[i].available()) {
                        printf("%s not supported for this target\n",
                               accel_list[i].name);
                    } else {
                        fprintf(stderr, "failed to initialize %s: %s\n",
                                accel_list[i].name,
                                strerror(-ret));
                    }
                    *(accel_list[i].allowed) = 0;
                } else {
                    accel_initalised = 1;
                }
                break;
            }
        }
        if (i == ARRAY_SIZE(accel_list)) {
            fprintf(stderr, "\"%s\" accelerator does not exist.\n", buf);
        }
    }

    if (!accel_initalised) {
        fprintf(stderr, "No accelerator found!\n");
        exit(1);
    }

    if (init_failed) {
        fprintf(stderr, "Back to %s accelerator.\n", accel_list[i].name);
    }

    return !accel_initalised;
}

void qemu_add_exit_notifier(Notifier *notify)
{
    notifier_list_add(&exit_notifiers, notify);
}

void qemu_remove_exit_notifier(Notifier *notify)
{
    notifier_list_remove(&exit_notifiers, notify);
}

static void qemu_run_exit_notifiers(void)
{
    notifier_list_notify(&exit_notifiers, NULL);
}

void qemu_add_machine_init_done_notifier(Notifier *notify)
{
    notifier_list_add(&machine_init_done_notifiers, notify);
}

static void qemu_run_machine_init_done_notifiers(void)
{
    notifier_list_notify(&machine_init_done_notifiers, NULL);
}

static const QEMUOption *lookup_opt(int argc, char **argv,
                                    const char **poptarg, int *poptind)
{
    const QEMUOption *popt;
    int optind = *poptind;
    char *r = argv[optind];
    const char *optarg;

    loc_set_cmdline(argv, optind, 1);
    optind++;
    /* Treat --foo the same as -foo.  */
    if (r[1] == '-')
        r++;
    popt = qemu_options;
    for(;;) {
        if (!popt->name) {
            error_report("invalid option");
            exit(1);
        }
        if (!strcmp(popt->name, r + 1))
            break;
        popt++;
    }
    if (popt->flags & HAS_ARG) {
        if (optind >= argc) {
            error_report("requires an argument");
            exit(1);
        }
        optarg = argv[optind++];
        loc_set_cmdline(argv, optind - 2, 2);
    } else {
        optarg = NULL;
    }

    *poptarg = optarg;
    *poptind = optind;

    return popt;
}

static gpointer malloc_and_trace(gsize n_bytes)
{
    void *ptr = malloc(n_bytes);
    trace_g_malloc(n_bytes, ptr);
    return ptr;
}

static gpointer realloc_and_trace(gpointer mem, gsize n_bytes)
{
    void *ptr = realloc(mem, n_bytes);
    trace_g_realloc(mem, n_bytes, ptr);
    return ptr;
}

static void free_and_trace(gpointer mem)
{
    trace_g_free(mem);
    free(mem);
}

#include "tcg-llvm-offline/tcg-llvm-offline.h"

int main(int argc, char **argv) {
	crete_set_data_dir(argv[0]);
	x86_llvm_translator();
	return 0;
}

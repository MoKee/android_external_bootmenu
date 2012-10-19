/*
 * Copyright (C) 2007-2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <unistd.h>

#include "common.h"
#include "extendedcommands.h"
#include "overclock.h"
#include "minui/minui.h"
#include "bootmenu_ui.h"

#ifdef BOARD_WITH_CPCAP
#include "battery/batt_cpcap.h"
#endif

//#define DEBUG_ALLOC

#define MODES_COUNT 12
const char* modes[] = {
  "bootmenu",
  "2nd-init",
  "2nd-boot",
  "2nd-boot-uart",
  "2nd-system",
  "normal",
  "2nd-init-adb",
  "2nd-boot-adb",
  "2nd-system-adb",
  "normal-adb",
  "recovery",
  "recovery-dev",
  "shell",
};

// user friendly menu labels
#define LABEL_2NDINIT    "2nd-init"
#define LABEL_2NDBOOT    "2nd-boot"
#define LABEL_2NDBOOT_UART    "2nd-boot-uart"
#define LABEL_2NDSYSTEM  "2nd-system"
#define LABEL_NORMAL     "Direct"

#define LABEL_TOGGLE_ADB "ADB:"

static bool boot_with_adb = false;
static int adbd_ready = 0;

/**
 * int_mode()
 *
 */
int int_mode(char * mode) {
  int m;
  for (m=0; m < MODES_COUNT; m++) {
    if (0 == strcmp(modes[m], mode)) {
      return m;
    }
  }
  return 0;
}

/**
 * str_mode()
 *
 */
const char* str_mode(int mode) {
  if (mode >= 0 && mode < MODES_COUNT) {
    return modes[mode];
  }
  return "bootmenu";
}

/**
 * show_menu_boot()
 *
 */
int show_menu_boot(void) {

  #define BOOT_2NDINIT    1
  #define BOOT_2NDBOOT    2
  #define BOOT_2NDBOOT_UART    3
  #define BOOT_2NDSYSTEM  4
  #define BOOT_NORMAL     5

  #define TOGGLE_ADB      6

  #define BOOT_FBTEST     7
  #define BOOT_EVTTEST    8
  #define BOOT_PNGTEST    9
  #define BOOT_TEST       10

  int status, res = 0;
  const char* headers[] = {
        " # Boot -->",
        "",
        NULL
  };
  char** title_headers = prepend_title(headers);

  struct UiMenuItem items[(MODES_COUNT - 3 + 6)] = {
    {MENUITEM_SMALL, "Set Default: [" LABEL_2NDINIT "]", NULL},
    {MENUITEM_SMALL, LABEL_2NDINIT, NULL},
    {MENUITEM_SMALL, LABEL_2NDBOOT, NULL},
    {MENUITEM_SMALL, LABEL_2NDBOOT_UART, NULL},
    {MENUITEM_SMALL, LABEL_2NDSYSTEM, NULL},
    {MENUITEM_SMALL, LABEL_NORMAL, NULL},

    {MENUITEM_SMALL, LABEL_TOGGLE_ADB, NULL},

#ifdef DEBUG_ALLOC
    {MENUITEM_SMALL, "test fb", NULL},
    {MENUITEM_SMALL, "test evt", NULL},
    {MENUITEM_SMALL, "test png", NULL},
    {MENUITEM_SMALL, "test all", NULL},
#endif
    {MENUITEM_SMALL, "<--Go Back", NULL},
    {MENUITEM_NULL, NULL, NULL},
  };

  char opt_def[64];
  char opt_adb[32];
  struct UiMenuResult ret;
  int bootmode;

  for (;;) {
    bootmode = get_default_bootmode();

    sprintf(opt_def, "Set Default: [%s]", str_mode(bootmode) );
    items[0].title = opt_def;

    //Hide unavailables modes
    if (!file_exists((char*) FILE_STOCK)) {
        items[BOOT_NORMAL].title = "";
    }
    if (!file_exists((char*) FILE_2NDSYSTEM)) {
        items[BOOT_2NDSYSTEM].title = "";
    }
    if (!file_exists((char*) FILE_2NDINIT)) {
        items[BOOT_2NDINIT].title = "";
    }
    if (!file_exists((char*) FILE_2NDBOOT)) {
        items[BOOT_2NDBOOT].title = "";
    }
    if (!file_exists((char*) FILE_2NDBOOT_UART)) {
        items[BOOT_2NDBOOT_UART].title = "";
    }
    //ADB Toggle
    sprintf(opt_adb, LABEL_TOGGLE_ADB " %s", boot_with_adb ? "enable":"disable");
    items[TOGGLE_ADB].title = opt_adb;

    ret = get_menu_selection(title_headers, TABS, items, 1, 0);

    if (ret.result == GO_BACK) {
        goto exit_loop;
    }

    //Submenu: select default mode
    if (ret.result == 0) {
        show_config_bootmode();
        continue;
    }

    //Next boot mode (after reboot, no more required)
    /*
    else if (ret.result < BOOT_NORMAL) {
        if (next_bootmode_write( str_mode(ret.result) ) != 0) {
            //write error
            continue;
        }
        sync();
        reboot(RB_AUTOBOOT);
        goto exit_loop;
    }*/

    //Direct boot modes
    else if (ret.result == BOOT_2NDINIT) {
        if (boot_with_adb && usb_connected() && !adb_started())
            exec_script(FILE_ADBD, ENABLE);
        status = snd_init(ENABLE);
        res = (status == 0);
        goto exit_loop;
    }
    else if (ret.result == BOOT_2NDBOOT) {
        if (boot_with_adb && usb_connected() && !adb_started())
            exec_script(FILE_ADBD, ENABLE);
        status = snd_boot(ENABLE);
        res = (status == 0);
        goto exit_loop;
    }
    else if (ret.result == BOOT_2NDBOOT_UART) {
        status = snd_boot_uart(ENABLE);
        res = (status == 0);
        goto exit_loop;
    }
    else if (ret.result == BOOT_2NDSYSTEM) {
        if (!file_exists((char*) FILE_2NDSYSTEM)) {
            LOGE("Script not found :\n%s\n", FILE_2NDSYSTEM);
            continue;
        }
        if (boot_with_adb && usb_connected() && !adb_started())
            exec_script(FILE_ADBD, ENABLE);
        status = snd_system(ENABLE);
        res = (status == 0);
        goto exit_loop;
    }
    else if (ret.result == BOOT_NORMAL) {
        if (boot_with_adb && usb_connected() && !adb_started())
            exec_script(FILE_ADBD, ENABLE);
        status = stk_boot(ENABLE);
        res = (status == 0);
        goto exit_loop;
    }
    else if (ret.result == TOGGLE_ADB) {
        boot_with_adb = (boot_with_adb == 0);
        continue;
    }
    else
    switch (ret.result) {
#ifdef DEBUG_ALLOC
      case BOOT_TEST:
        led_alert("green", 1);
        ui_final();
        ui_init();
        led_alert("green", 0);
        res = 0;
        goto exit_loop;

      case BOOT_FBTEST:
        led_alert("green", 1);
        gr_fb_test();
        ui_stop_redraw();
        ui_resume_redraw();
        led_alert("green", 0);
        res = 0;
        goto exit_loop;

      case BOOT_EVTTEST:
        led_alert("green", 1);
        evt_exit();
        evt_init();
        led_alert("green", 0);
        res = 0;
        goto exit_loop;

      case BOOT_PNGTEST:
        led_alert("green", 1);
        ui_free_bitmaps();
        ui_create_bitmaps();
        led_alert("green", 0);
        res = 0;
        goto exit_loop;
#endif
      default:
        goto exit_loop;
    }

  } //for

exit_loop:

  // free alloc by prepend_title()
  free_menu_headers(title_headers);

  return res;
}

/**
 * show_config_bootmode()
 *
 */
int show_config_bootmode(void) {

  //last mode enabled for default modes (adb disabled)
  #define LAST_MODE 5

  int res = 0;
  const char* headers[3] = {
          " # Boot --> Set Default -->",
          "",
          NULL
  };
  char** title_headers = prepend_title(headers);

  static char options[MODES_COUNT][64];
  struct UiMenuItem menu_opts[MODES_COUNT];
  int i, mode;
  struct UiMenuResult ret;

  for (;;) {

    mode = get_default_bootmode();

    for(i = 0; i < LAST_MODE; ++i) {
      sprintf(options[i], " [%s]", str_mode(i));
      if(mode == i)
        options[i][0] = '*';
      menu_opts[i] = buildMenuItem(MENUITEM_SMALL, options[i], NULL);
    }

    menu_opts[LAST_MODE] = buildMenuItem(MENUITEM_SMALL, "<--Go Back", NULL);
    menu_opts[LAST_MODE+1] = buildMenuItem(MENUITEM_NULL, NULL, NULL);

    ret = get_menu_selection(title_headers, TABS, menu_opts, 1, mode);
    if (ret.result >= LAST_MODE || strlen(menu_opts[ret.result].title) == 0) {
      //back
      res=1;
      break;
    }
    if (ret.result == BOOT_NORMAL) {
      if (!file_exists((char*) FILE_STOCK)) {
        //disable stock boot in CyanogenMod for locked devices
        LOGI("Function disabled in this version\n");
        continue;
      }
    }
    if (ret.result == BOOT_2NDSYSTEM) {
      if (!file_exists((char*) FILE_2NDSYSTEM)) {
        LOGE("Script not found :\n%s\n", FILE_2NDSYSTEM);
        continue;
      }
    }
    if (set_default_bootmode(ret.result) == 0) {
      ui_print("Done..");
      continue;
    }
    else {
      ui_print("Failed to setup default boot mode.");
      break;
    }
  }

  free_menu_headers(title_headers);
  return res;
}


#if STOCK_VERSION

/**
 * show_menu_system()
 *
 */
int show_menu_system(void) {

  #define SYSTEM_OVERCLOCK  0
  #define SYSTEM_ROOT       1
  #define SYSTEM_UNINSTALL  2

  int status;
  int select = 0;
  struct stat buf;

  const char* headers[] = {
        " # System -->",
        "",
        NULL
  };
  char** title_headers = prepend_title(headers);

  struct UiMenuItem items[] = {
    {MENUITEM_SMALL, "Overclock", NULL},
    {MENUITEM_SMALL, "UnRooting", NULL},
    {MENUITEM_SMALL, "Uninstall BootMenu", NULL},
    {MENUITEM_SMALL, "<--Go Back", NULL},
    {MENUITEM_NULL, NULL, NULL},
  };

  for (;;) {

    if ((stat("/system/app/Superuser.apk", &buf) < 0) && (stat("/system/app/superuser.apk", &buf) < 0))
      items[1].title = "Rooting";
    else
      items[1].title = "UnRooting";

    struct UiMenuResult ret = get_menu_selection(title_headers, TABS, items, 1, select);

    switch (ret.result) {

      case SYSTEM_OVERCLOCK:
        status = show_menu_overclock();
        break;
      case SYSTEM_ROOT:
        ui_print("[Un]Rooting....");
        status = exec_script(FILE_ROOT, ENABLE);
        ui_print("Done..\n");
        break;
      case SYSTEM_UNINSTALL:
        ui_print("Uninstall BootMenu....");
        status = exec_script(FILE_UNINSTALL, ENABLE);
        ui_print("Done..\n");
        ui_print("******** Plz reboot now.. ********\n");
        break;
      default:
        return 0;
    }
    select = ret.result;
  }

  free_menu_headers(title_headers);
  return 0;
}
#endif //#if STOCK_VERSION


/**
 * show_menu_tools()
 *
 * ADB shell and usb shares
 */
int show_menu_tools(void) {

#define TOOL_ADB     0
#define TOOL_USB     2
#define TOOL_CDROM   3
#define TOOL_SYSTEM  4
#define TOOL_DATA    5
#define TOOL_NATIVE  6

#define TOOL_UMOUNT  8

#ifndef BOARD_MMC_DEVICE
#define BOARD_MMC_DEVICE "/dev/block/mmcblk1"
#endif

  int status;

  const char* headers[] = {
        " Don't forget to stop the share after use !",
        "",
        " # Tools -->",
        "",
        NULL
  };
  char** title_headers = prepend_title(headers);

  struct UiMenuItem items[] = {
    {MENUITEM_SMALL, "ADB Daemon", NULL},
    {MENUITEM_SMALL, "", NULL},
    {MENUITEM_SMALL, "Share SD Card", NULL},
    {MENUITEM_SMALL, "Share Drivers", NULL},
    {MENUITEM_SMALL, "Share system", NULL},
    {MENUITEM_SMALL, "Share data", NULL},
    {MENUITEM_SMALL, "Share MMC - Dangerous!", NULL},
    {MENUITEM_SMALL, "", NULL},
    {MENUITEM_SMALL, "Stop USB Share", NULL},
    {MENUITEM_SMALL, "<--Go Back", NULL},
    {MENUITEM_NULL, NULL, NULL},
  };

  struct UiMenuResult ret = get_menu_selection(title_headers, TABS, items, 1, 0);

  switch (ret.result) {
    case TOOL_ADB:
      ui_print("ADB Deamon....");
      status = exec_script(FILE_ADBD, ENABLE);
      ui_print("Done..\n");
      break;

    case TOOL_UMOUNT:
      ui_print("Stopping USB share...");
      sync();
      mount_usb_storage("");
      status = set_usb_device_mode("acm");
      ui_print("Done..\n");
      break;

    case TOOL_USB:
      ui_print("USB Mass Storage....");
      status = exec_script(FILE_SDCARD, ENABLE);
      ui_print("Done..\n");
      break;

    case TOOL_CDROM:
      ui_print("USB Drivers....");
      status = exec_script(FILE_CDROM, ENABLE);
      ui_print("Done..\n");
      break;

    case TOOL_SYSTEM:
      ui_print("Sharing System Partition....");
      status = exec_script(FILE_SYSTEM, ENABLE);
      ui_print("Done..\n");
      break;

    case TOOL_DATA:
      ui_print("Sharing Data Partition....");
      status = exec_script(FILE_DATA, ENABLE);
      ui_print("Done..\n");
      break;

    case TOOL_NATIVE:
      ui_print("Set USB device mode...");
      sync();
      mount_usb_storage(BOARD_MMC_DEVICE);
      usleep(500*1000);
      status = set_usb_device_mode("msc_adb");
      usleep(500*1000);
      mount_usb_storage(BOARD_MMC_DEVICE);
      ui_print("Done..\n");
      break;

    default:
      break;
  }

  free_menu_headers(title_headers);
  return 0;
}

/**
 * show_menu_recovery()
 *
 */
int show_menu_recovery(void) {

#ifdef USE_STABLE_RECOVERY
  #define RECOVERY_CUSTOM     0
  #define RECOVERY_STABLE     1
  #define RECOVERY_STOCK      2
#else
  #define RECOVERY_CUSTOM     0
  #define RECOVERY_STOCK      1
#endif

  int status, res=0;
  char** args;
  FILE* f;

  const char* headers[] = {
        " # Recovery -->",
        "",
        NULL
  };
  char** title_headers = prepend_title(headers);

  struct UiMenuItem items[] = {
    {MENUITEM_SMALL, "Custom Recovery", NULL},
#ifdef USE_STABLE_RECOVERY
    {MENUITEM_SMALL, "Stable Recovery", NULL},
#endif
    {MENUITEM_SMALL, "Stock Recovery", NULL},
    {MENUITEM_SMALL, "<--Go Back", NULL},
    {MENUITEM_NULL, NULL, NULL},
  };

  struct UiMenuResult ret = get_menu_selection(title_headers, TABS, items, 1, 0);

  switch (ret.result) {
    case RECOVERY_CUSTOM:
      ui_print("Starting Recovery..\n");
      ui_print("This can take a couple of seconds.\n");
      ui_show_text(DISABLE);
      ui_stop_redraw();
      status = exec_script(FILE_CUSTOMRECOVERY, ENABLE);
      ui_resume_redraw();
      ui_show_text(ENABLE);
      if (!status) res = 1;
      break;

#ifdef USE_STABLE_RECOVERY
    case RECOVERY_STABLE:
      ui_print("Starting Recovery..\n");
      ui_print("This can take a couple of seconds.\n");
      ui_show_text(DISABLE);
      ui_stop_redraw();
      status = exec_script(FILE_STABLERECOVERY, ENABLE);
      ui_resume_redraw();
      ui_show_text(ENABLE);
      if (!status) res = 1;
      break;
#endif

    case RECOVERY_STOCK:
      ui_print("Rebooting to Stock Recovery..\n");

      sync();
      __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, "recovery");

    default:
      break;
  }

  free_menu_headers(title_headers);
  return res;
}

/**
 * snd_init()
 *
 * 2nd-init Profile (reload init with new .rc files)
 */
int snd_init(int ui) {
  int status;
  int i;

  bypass_sign("yes");

  if (ui)
    ui_print("Start " LABEL_2NDINIT " boot....\n");
  else
    LOGI("Start " LABEL_2NDINIT " boot....\n");

  ui_stop_redraw();
#ifdef USE_DUALCORE_DIRTY_HACK
    if(!ui)
      status = snd_exec_script(FILE_2NDINIT, ui);
    else
#endif
      status = exec_script(FILE_2NDINIT, ui);
  ui_resume_redraw();

  if (status) {
    return -1;
    bypass_sign("no");
  }

  if (ui)
    ui_print("Wait 2 seconds....\n");
  else
    LOGI("Wait 2 seconds....\n");

  for(i = 2; i > 0; --i) {
    if (ui)
      ui_print("%d.\n", i);
    else
      LOGI("%d..\n", i);
    usleep(1000000);
  }

  bypass_sign("no");
  return 0;
}

/**
 * snd_boot()
 *
 * For 2nd-boot (boot custom kernel)
 */
int snd_boot(int ui) {
  int status;
  int i;

  bypass_sign("yes");

  if (ui)
    ui_print("Start " LABEL_2NDBOOT " boot....\n");
  else
    LOGI("Start " LABEL_2NDBOOT " boot....\n");

  ui_stop_redraw();
#ifdef USE_DUALCORE_DIRTY_HACK
    if(!ui)
      status = snd_exec_script(FILE_2NDBOOT, ui);
    else
#endif
      status = exec_script(FILE_2NDBOOT, ui);
  ui_resume_redraw();

  if (status) {
    bypass_sign("no");
    return -1;
  }

  if (ui)
    ui_print("Wait 2 seconds....\n");
  else
    LOGI("Wait 2 seconds....\n");

  for(i = 2; i > 0; --i) {
    if (ui)
      ui_print("%d.\n", i);
    else
      LOGI("%d..\n", i);
    usleep(1000000);
  }

  bypass_sign("no");
  return 0;
}

/**
 * snd_boot()
 *
 * For 2nd-boot (boot custom kernel with uart support)
 */
int snd_boot_uart(int ui) {
  int status;
  int i;

  bypass_sign("yes");

  if (ui)
    ui_print("Start " LABEL_2NDBOOT_UART " boot....\n");
  else
    LOGI("Start " LABEL_2NDBOOT_UART " boot....\n");

  ui_stop_redraw();
#ifdef USE_DUALCORE_DIRTY_HACK
    if(!ui)
      status = snd_exec_script(FILE_2NDBOOT_UART, ui);
    else
#endif
      status = exec_script(FILE_2NDBOOT_UART, ui);
  ui_resume_redraw();

  if (status) {
    bypass_sign("no");
    return -1;
  }

  if (ui)
    ui_print("Wait 2 seconds....\n");
  else
    LOGI("Wait 2 seconds....\n");

  for(i = 2; i > 0; --i) {
    if (ui)
      ui_print("%d.\n", i);
    else
      LOGI("%d..\n", i);
    usleep(1000000);
  }

  bypass_sign("no");
  return 0;
}

/**
 * snd_system()
 *
 * Reserved for Dual Boot
 */
int snd_system(int ui) {
  int status;
  int i;

  bypass_sign("yes");

  if (ui)
    ui_print("Start " LABEL_2NDSYSTEM " boot....\n");
  else
    LOGI("Start " LABEL_2NDSYSTEM " boot....\n");

  ui_stop_redraw();
#ifdef USE_DUALCORE_DIRTY_HACK
    if(!ui)
      status = snd_exec_script(FILE_2NDSYSTEM, ui);
    else
#endif
      status = exec_script(FILE_2NDSYSTEM, ui);
  ui_resume_redraw();

  if (status) {
    bypass_sign("no");
    return -1;
  }

  if (ui)
    ui_print("Wait 2 seconds....\n");
  else
    LOGI("Wait 2 seconds....\n");

  for(i = 2; i > 0; --i) {
    if (ui)
      ui_print("%d.\n", i);
    else
      LOGI("%d..\n", i);
    usleep(1000000);
  }

  bypass_sign("no");
  return 0;
}

/**
 * stk_boot()
 *
 * Direct boot (continue normal execution of init)
 */
int stk_boot(int ui) {
  int status;
  int i;

  bypass_sign("yes");

  if (ui)
    ui_print("Start " LABEL_NORMAL " boot....\n");
  else
    LOGI("Start " LABEL_NORMAL " boot....\n");

  status = exec_script(FILE_STOCK, ui);
  if (status) {
    return -1;
    bypass_sign("no");
  }

  usleep(1000000);

  bypass_sign("no");
  return 0;
}

// --------------------------------------------------------

/**
 * get_default_bootmode()
 *
 */
int get_default_bootmode() {
  char mode[32];
  int m;
  FILE* f = fopen(FILE_DEFAULTBOOTMODE, "r");
  if (f != NULL) {
      fscanf(f, "%s", mode);
      fclose(f);

      m = int_mode(mode);
      LOGI("default_bootmode=%d\n", m);

      if (m >=0) return m;
      else return int_mode("bootmenu");

  }
  return -1;
}

/**
 * get_bootmode()
 *
 */
int get_bootmode(int clean,int log) {
  char mode[32];
  int m;
  FILE* f = fopen(FILE_BOOTMODE, "r");
  if (f != NULL) {

      // One-shot bootmode, bootmode.conf is deleted after
      fscanf(f, "%s", mode);
      fclose(f);

      if (clean) {
          // unlink(FILE_BOOTMODE); //buggy unlink ?
          exec_script(FILE_BOOTMODE_CLEAN,DISABLE);
      }

      m = int_mode(mode);
      if (log) LOGI("bootmode=%d\n", m);
      if (m >= 0) return m;
  }

  return get_default_bootmode();
}

/**
 * set_default_bootmode()
 *
 */
int set_default_bootmode(int mode) {

  char log[64];
  char* str = (char*) str_mode(mode);

  if (mode < MODES_COUNT) {

      ui_print("Set %s...\n", str);
      return bootmode_write(str);
  }

  ui_print("ERROR: bad mode %d\n", mode);
  return 1;
}

/**
 * bootmode_write()
 *
 * write default boot mode in config file
 */
int bootmode_write(const char* str) {
  FILE* f = fopen(FILE_DEFAULTBOOTMODE, "w");

  if (f != NULL) {
    fprintf(f, "%s", str);
    fflush(f);
    fclose(f);
    sync();
    //double check
    if (get_bootmode(0,0) == int_mode( (char*)str) ) {
      return 0;
    }
  }

  ui_print("ERROR: unable to write mode %s\n", str);
  return 1;
}

/**
 * next_bootmode_write()
 *
 * write next boot mode in config file
 */
int next_bootmode_write(const char* str) {
  FILE* f = fopen(FILE_BOOTMODE, "w");

  if (f != NULL) {
    fprintf(f, "%s", str);
    fflush(f);
    fclose(f);
    sync();
    ui_print("Next boot mode set to %s\n\nRebooting...\n", str);
    return 0;
  }

  return 1;
}

// --------------------------------------------------------

/**
 * led_alert()
 *
 */
int led_alert(const char* color, int value) {
  char led_path[PATH_MAX];
  sprintf(led_path, "/sys/class/leds/%s/brightness", color);
  FILE* f = fopen(led_path, "w");

  if (f != NULL) {
    fprintf(f, "%d", value);
    fclose(f);
    return 0;
  }
  return 1;
}

/**
 * bypass_sign()
 *
 */
int bypass_sign(const char* mode) {
  FILE* f = fopen(FILE_BYPASS, "w");

  if (f != NULL) {
    fprintf(f, "%s",  mode);
    fclose(f);
    return 0;
  }
  return 1;
}

/**
 * bypass_check()
 *
 */
int bypass_check(void) {
   FILE* f = fopen(FILE_BYPASS, "r");
   char bypass[30];

  if (f != NULL) {
    fscanf(f, "%s", bypass);
    fclose(f);
    if (0 == strcmp(bypass, "yes")) {
      return 0;
    }
  }
  return 1;
}

/**
 * exec_and_wait()
 *
 */
int exec_and_wait(char** argp) {
  pid_t pid;
  sig_t intsave, quitsave;
  sigset_t mask, omask;
  int pstat;

  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigprocmask(SIG_BLOCK, &mask, &omask);
  switch (pid = vfork()) {
  case -1:            /* error */
    sigprocmask(SIG_SETMASK, &omask, NULL);
    return(-1);
  case 0:                /* child */
    sigprocmask(SIG_SETMASK, &omask, NULL);
    execve(argp[0], argp, environ);

    // execve require the full path of binary in argp[0]
    if (errno == 2 && strncmp(argp[0], "/", 1)) {
        char bin[PATH_MAX] = "/system/bin/";
        argp[0] = strcat(bin, argp[0]);
        execve(argp[0], argp, environ);
    }

    fprintf(stdout, "E:Can't run %s (%s)\n", argp[0], strerror(errno));
    _exit(127);
  }

  intsave = (sig_t)  bsd_signal(SIGINT, SIG_IGN);
  quitsave = (sig_t) bsd_signal(SIGQUIT, SIG_IGN);
  pid = waitpid(pid, (int *)&pstat, 0);
  sigprocmask(SIG_SETMASK, &omask, NULL);
  (void)bsd_signal(SIGINT, intsave);
  (void)bsd_signal(SIGQUIT, quitsave);
  return (pid == -1 ? -1 : pstat);
}

/**
 * exec_script()
 *
 */
int exec_script(const char* filename, int ui) {
  int status;
  char** args;

  if (!file_exists((char*) filename)) {
    LOGE("Script not found :\n%s\n", filename);
    return -1;
  }

  LOGI("exec %s\n", filename);

  chmod(filename, 0755);

  args = malloc(sizeof(char*) * 2);
  args[0] = (char *) filename;
  args[1] = NULL;

  status = exec_and_wait(args);

  free(args);

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    if (ui) {
      LOGE("Error in %s\n(Result: %s)\n", filename, strerror(errno));
    }
    else {
      LOGI("E:Error in %s\n(Result: %s)\n", filename, strerror(errno));
    }
    return -1;
  }

  return 0;
}

inline int snd_reboot() {
  sync();
  return reboot(RB_AUTOBOOT);
}

/**
 * snd_exec_script()
 *
 * dirty hack for dual core cpus
 * sometimes 2nd-init doesnt work, when task is not executed on same core
 * (for more infos search for ptrace() problems related to cpu affinity)
 *
 * We need to reboot phone to retry because the phone is not in a proper state
 */
int snd_exec_script(const char* filename, int ui) {
  int status;
  char** args;

  if (!file_exists((char*) filename)) {
    LOGE("Script not found :\n%s\n", filename);
    return snd_reboot();
  }

  LOGI("exec %s\n", filename);

  chmod(filename, 0755);

  args = malloc(sizeof(char*) * 2);
  args[0] = (char *) filename;
  args[1] = NULL;

  status = exec_and_wait(args);

  free(args);

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    if (ui) {
      LOGE("Error in %s\n(Result: %s)\nWill auto restart now\n", filename, strerror(errno));
    }
    else {
      LOGI("E:Error in %s\n(Result: %s)\nWill auto restart now\n", filename, strerror(errno));
    }
    return snd_reboot();
  }

  return 0;
}

/**
 * real_execute()
 *
 * when bootmenu is substitued to a system binary (like logwrapper)
 * we need also to execute the original binary, renamed logwrapper.bin
 *
 */
int real_execute(int r_argc, char** r_argv) {
  char* hijacked_executable = r_argv[0];
  int result = 0;
  int i;

  char real_executable[PATH_MAX];
  char ** argp = (char **)malloc(sizeof(char *) * (r_argc + 1));

  sprintf(real_executable, "%s.bin", hijacked_executable);

  argp[0] = real_executable;
  for (i = 1; i < r_argc; i++) {
      argp[i] = r_argv[i];
  }
  argp[r_argc] = NULL;

  result = exec_and_wait(argp);

  free(argp);

  if (!WIFEXITED(result) || WEXITSTATUS(result) != 0)
    return -1;
  else
    return 0;
}

/**
 * file_exists()
 *
 */
int file_exists(char * file)
{
  struct stat file_info;
  memset(&file_info,0,sizeof(file_info));
  return (int) (0 == stat(file, &file_info));
}

int log_dumpfile(char * file)
{
  char buffer[MAX_COLS];
  int lines = 0;
  FILE* f = fopen(file, "r");
  if (f == NULL) return 0;

  while (fgets(buffer, MAX_COLS, f) != NULL) {
    ui_print("%s", buffer);
    lines++;

    // limit max read size...
    if (lines > MAX_ROWS*100) break;
  }
  fclose(f);

  return lines;
}

/**
 * usb_connected()
 *
 */
int usb_connected() {
  int state;
  FILE* f;

  //usb should be 1 and ac 0
  f = fopen(SYS_USB_CONNECTED, "r");
  if (f != NULL) {
    fscanf(f, "%d", &state);
    fclose(f);
    if (state) {
      f = fopen(SYS_POWER_CONNECTED, "r");
      if (f != NULL) {
        fscanf(f, "%d", &state);
        fclose(f);
        return (state == 0);
      }
    }
  }
  return 0;
}

int adb_started() {
  int res = 0;

  #ifndef FILE_ADB_STATE
  #define FILE_ADB_STATE "/tmp/usbd_current_state"
  #endif

  FILE* f = fopen(FILE_ADB_STATE, "r");
  if (f != NULL) {
    char mode[32] = "";
    fscanf(f, "%s", mode);
    res = (0 == strcmp("usb_mode_charge_adb", mode));
    fclose(f);
  }

  bool con = usb_connected();
  if (con && res) {
    adbd_ready = true;
  } else {
    // must be restarted, if usb was disconnected
    adbd_ready = false;
    f = fopen(FILE_ADB_STATE, "w");
    if (f != NULL) {
      fprintf(f, "\n");
      fflush(f);
      fclose(f);
    }
  }

  return adbd_ready;
}

/**
 * bettery_level()
 *
 */
int battery_level() {
  int state = 0;
  FILE* f = fopen(SYS_BATTERY_LEVEL, "r");
  if (f != NULL) {
    fscanf(f, "%d", &state);
    fclose(f);
  }
#ifdef BOARD_WITH_CPCAP
  state = cpcap_batt_percent();
#endif
  return state;
}


/**
 * Native USB ADB Mode Switch
 *
 */
int set_usb_device_mode(const char* mode) {

  #ifndef BOARD_USB_MODESWITCH
  #define BOARD_USB_MODESWITCH  "/dev/usb_device_mode"
  #endif

  FILE* f = fopen(BOARD_USB_MODESWITCH, "w");
  if (f != NULL) {

    fprintf(f, "%s", mode);
    fclose(f);

    LOGI("set usb mode=%s\n", mode);
    return 0;

  } else {
    fprintf(stdout, "E:Can't open " BOARD_USB_MODESWITCH " (%s)\n", strerror(errno));
    return errno;
  }
}

int mount_usb_storage(const char* part) {

  #ifndef BOARD_UMS_LUNFILE
  #define BOARD_UMS_LUNFILE  "/sys/devices/platform/usb_mass_storage/lun0/file"
  #endif

  FILE* f = fopen(BOARD_UMS_LUNFILE, "w");
  if (f != NULL) {

    fprintf(f, "%s", part);
    fclose(f);
    return 0;

  } else {
    ui_print("E:Unable to write to lun file (%s)", strerror(errno));
    return errno;
  }
}


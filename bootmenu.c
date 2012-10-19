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
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/reboot.h>
#include <time.h>

#include <unistd.h>

#include "common.h"
#include "extendedcommands.h"
#include "overclock.h"
#include "minui/minui.h"
#include "bootmenu_ui.h"

enum {
  BUTTON_ERROR,
  BUTTON_PRESSED,
  BUTTON_TIMEOUT,
};

/* Main menu items */
#define ITEM_REBOOT      0
#define ITEM_BOOT        1
#define ITEM_SYSTEM      2
#define ITEM_OVERCLOCK   2
#define ITEM_RECOVERY    3
#define ITEM_TOOLS       4
#define ITEM_POWEROFF    5

#define ITEM_LAST        5

struct UiMenuItem MENU_ITEMS[] = {
  {MENUITEM_SMALL, "Reboot", NULL},
  {MENUITEM_SMALL, "Boot", NULL},
#if STOCK_VERSION
  {MENUITEM_SMALL, "System", ""},
#elif !defined(NO_OVERCLOCK)
  {MENUITEM_SMALL, "CPU Settings", ""},
#else
  {MENUITEM_SMALL, "", ""},
#endif
  {MENUITEM_SMALL, "Recovery", ""},
  {MENUITEM_SMALL, "Tools", ""},
  {MENUITEM_SMALL, "Shutdown", ""},
  {MENUITEM_NULL, NULL, NULL},
};

static char** main_headers = NULL;
static float progress_value = 0.0;

/**
 * prepend_title()
 *
 * Add fixed bootmenu header before menu items
 *
 * Note: Hard to tweak, should maybe be recoded
 *       without malloc..
 */
char** prepend_title(const char** headers) {

  char* title[] = {
      "Android Bootmenu v" BOOTMENU_VERSION,
      "",
      NULL
  };

  // count the number of lines in our title, plus the
  // caller-provided headers.
  int count = 0;
  char** p;
  for (p = title; *p; ++p, ++count);
  for (p = (char**) headers; *p; ++p, ++count);

  char** new_headers = malloc((count+1) * sizeof(char*));
  char** h = new_headers;
  for (p = title; *p; ++p, ++h) *h = *p;
  for (p = (char**) headers; *p; ++p, ++h) *h = *p;
  *h = NULL;

  return new_headers;
}

void free_menu_headers(char **headers) {
  char** p = headers;
  for (p = headers; *p; ++p) *p = NULL;
  if (headers != NULL) {
    free(headers);
    headers = NULL;
  }
}

/**
 * get_menu_selection()
 *
 */
struct UiMenuResult get_menu_selection(char** headers, char** tabs, struct UiMenuItem* items, int menu_only,
                       int initial_selection) {
  // throw away keys pressed previously, so user doesn't
  // accidentally trigger menu items.
  ui_clear_key_queue();

  ui_start_menu(headers, tabs, items, initial_selection);
  int selected = initial_selection;
  struct UiMenuResult ret;
  struct ui_touchresult tret;
  ret.result = -1;
  ret.type = -1;

  while (ret.result < 0) {

    struct ui_input_event eventresult;
    int visible = ui_text_visible();
    int action = 0;

    ui_wait_input(&eventresult);

    switch(eventresult.utype) {
      case UINPUTEVENT_TYPE_KEY:
        action = device_handle_key(eventresult.code, visible);

        if (action < 0) {
          if(action==HIGHLIGHT_UP || action==HIGHLIGHT_DOWN || action==SELECT_ITEM) {
            if(is_menuSelection_enabled()!=1) {
              enableMenuSelection(1);
              break;
            }
          }

          switch (action) {
            case HIGHLIGHT_UP:
              --selected;
              selected = ui_menu_select(selected);
              break;
            case HIGHLIGHT_DOWN:
              ++selected;
              selected = ui_menu_select(selected);
              break;
            case SELECT_ITEM:
              ret.result = selected;
              ret.type = RESULT_LIST;
              break;
            case ACTION_CANCEL:
              ret.result = GO_BACK;
              ret.type = RESULT_LIST;
              break;
            case NO_ACTION:
              break;
            case ACTION_NEXTTAB:
              ret.result = ui_setTab_next();
              ret.type = RESULT_TAB;
              break;
          }
        } else if (!menu_only) {
          ret.result = action;
        }
        break;

      case UINPUTEVENT_TYPE_TOUCH_START:
      case UINPUTEVENT_TYPE_TOUCH_DRAG:
      case UINPUTEVENT_TYPE_TOUCH_RELEASE:
        enableMenuSelection(0);
        tret = ui_handle_touch(eventresult);

        switch(tret.type) {
          case TOUCHRESULT_TYPE_ONCLICK_LIST:
            ret.result = tret.item;
            ret.type = RESULT_LIST;
          break;
        }
        break;

    } //switch

  }
  ui_end_menu();

  return ret;
}

/**
 * compare_string()
 *
 */
static int compare_string(const void* a, const void* b) {
  return strcmp(*(const char**)a, *(const char**)b);
}

/**
 * prompt_and_wait()
 *
 */
static void prompt_and_wait() {

  int select = 0;

  for (;;) {
    struct UiMenuResult menuret = get_menu_selection(main_headers, TABS, MENU_ITEMS, 0, select);

    // device-specific code may take some action here.  It may
    // return one of the core actions handled in the switch
    // statement below.

    if (menuret.result >= 0 && menuret.result <= ITEM_LAST) {

      switch (menuret.result) {
      case ITEM_REBOOT:
        sync();
        reboot(RB_AUTOBOOT);
        return;
      case ITEM_BOOT:
        if (show_menu_boot()) return;
        break;
#if STOCK_VERSION
      case ITEM_SYSTEM:
        if (show_menu_system()) return;
        break;
#elif !defined(NO_OVERCLOCK)
      case ITEM_OVERCLOCK:
        if (show_menu_overclock()) return;
        break;
#endif
      case ITEM_RECOVERY:
        if (show_menu_recovery()) return;
        break;
      case ITEM_TOOLS:
        if (show_menu_tools()) return;
        break;
      case ITEM_POWEROFF:
        sync();
        __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, NULL);
        return;
      }
      select = menuret.result;
    }
  }
}

/**
 * ui_finish()
 *
 */
static void ui_finish(void) {
  LOGI("Exiting....\n");
  ui_final();
}

/**
 * wait_key()
 *
 */
static int wait_key(int key) {
  int i;
  int result = 0;

  evt_init();
  //ui_clear_key_queue();
  for(i=0; i < 100; i++) {
    if(ui_key_pressed(key)) {
      led_alert("blue", DISABLE);
      result = 1;
      break;
    }
    else {
      usleep(15000); //15ms * 100
    }
  }
  evt_exit();
  return result;
}

/**
 * Start of UI
 */
static int run_bootmenu_ui(int mode) {

  int adb_started = 0;

  // initialize ui
  ui_init();
  //ui_set_background(BACKGROUND_DEFAULT);
  ui_show_text(ENABLE);
  LOGI("Start Android BootMenu....\n");
  ui_reset_progress();

  main_headers = prepend_title((const char**)MENU_HEADERS);

  /*
  ui_start_menu(main_headers, TABS, MENU_ITEMS, 0);
  ui_wait_key();
  ui_end_menu();
  */

  //get_menu_selection(main_headers, TABS, MENU_ITEMS, 0, 0);

  /* can be buggy, adb could lock filesystem
  if (!adb_started && usb_connected()) {
    ui_print("Usb connected, starting adb...\n\n");
    exec_script(FILE_ADBD, DISABLE);
  }
  */

  if (mode == int_mode("shell")) {
    ui_print("\n");
    ui_print("Current mode: %s\n", str_mode(mode));
    if (!usb_connected()) {
      ui_print(" But USB is not connected !\n");
    }
  }

  checkup_report();
  //ui_reset_progress();

  //test: fill the log
  log_dumpfile("/proc/cpuinfo");

  prompt_and_wait();
  free_menu_headers(main_headers);

  ui_finish();
  return 0;
}


/**
 * run_bootmenu()
 *
 */
static int run_bootmenu(void) {
  int defmode, mode, status = BUTTON_ERROR;
  int adb_started = 0;
  time_t start = time(NULL);

  LOGI("Starting bootmenu on %s", ctime(&start));

  if (bypass_check()) {

    // init rootfs and mount cache
    exec_script(FILE_PRE_MENU, DISABLE);

    led_alert("blue", ENABLE);

    defmode = get_default_bootmode();

    // get and clean one shot bootmode (or default)
    mode = get_bootmode(1,1);

    if (mode == int_mode("bootmenu")
     || mode == int_mode("recovery")
     || mode == int_mode("shell")) {
        // dont wait if these modes are asked
    } else {
        status = (wait_key(KEY_VOLUMEDOWN) ? BUTTON_PRESSED : BUTTON_TIMEOUT);
    }

    // only start adb if usb is connected
    if (usb_connected()) {
      if (mode == int_mode("2nd-init-adb")
       || mode == int_mode("2nd-boot-adb")
       || mode == int_mode("2nd-system-adb")) {
         exec_script(FILE_ADBD, DISABLE);
         adb_started = 1;
      }
    }

    // on timeout
    if (status != BUTTON_PRESSED) {

      if (mode == int_mode("bootmenu")) {
          led_alert("blue", DISABLE);
          status = BUTTON_PRESSED;
      }
      else if (mode == int_mode("2nd-init") || mode == int_mode("2nd-init-adb")) {
          led_alert("blue", DISABLE);
          led_alert("green", ENABLE);
          snd_init(DISABLE);
          led_alert("green", DISABLE);
          status = BUTTON_TIMEOUT;
      }
      else if (mode == int_mode("2nd-boot") || mode == int_mode("2nd-boot-adb")) {
          led_alert("blue", DISABLE);
          led_alert("red", ENABLE);
          snd_boot(DISABLE);
          led_alert("red", DISABLE);
          status = BUTTON_TIMEOUT;
      }
      else if (mode == int_mode("2nd-boot-uart")) {
          led_alert("blue", DISABLE);
          led_alert("red", ENABLE);
          snd_boot_uart(DISABLE);
          led_alert("red", DISABLE);
          status = BUTTON_TIMEOUT;
      }
      else if (mode == int_mode("2nd-system") || mode == int_mode("2nd-system-adb")) {
          led_alert("blue", DISABLE);
          led_alert("red", ENABLE);
          led_alert("green", ENABLE);
          snd_system(DISABLE);
          led_alert("red", DISABLE);
          led_alert("green", DISABLE);
          status = BUTTON_TIMEOUT;
      }
      else if (mode == int_mode("recovery-dev")) {
          led_alert("blue", DISABLE);
          exec_script(FILE_CUSTOMRECOVERY, DISABLE);
          status = BUTTON_TIMEOUT;
      }
      else if (mode == int_mode("recovery")) {
          led_alert("blue", DISABLE);
          exec_script(FILE_STABLERECOVERY, DISABLE);
          status = BUTTON_TIMEOUT;
      }
      else if (mode == int_mode("shell")) {
          led_alert("blue", DISABLE);
          exec_script(FILE_ADBD, DISABLE);
          status = BUTTON_PRESSED;
      }
      else if (mode == int_mode("normal") || mode == int_mode("normal-adb")) {
          led_alert("blue", DISABLE);
          stk_boot(DISABLE);
          status = BUTTON_TIMEOUT;
      }

    }

    if (status == BUTTON_PRESSED ) {

        led_alert("button-backlight", ENABLE);

        run_bootmenu_ui(mode);
    }

  }
  return EXIT_SUCCESS;
}


/**
 * main()
 *
 * Here is the hijack init.rc part, logwrapper is a symlink pointing
 * to this bootmenu binary, we trap some of logged commands from init.rc
 *
 */
int main(int argc, char **argv) {
  int result;

  if (argc == 2 && 0 == strcmp(argv[1], "postbootmenu")) {

    /* init.rc call: "exec bootmenu postbootmenu" */

    exec_script(FILE_OVERCLOCK, DISABLE);
    result = exec_script(FILE_POST_MENU, DISABLE);
    bypass_sign("no");
    sync();
    return result;
  }
  else if (NULL != strstr(argv[0], "bootmenu")) {

    /* Direct UI, without key test */

#ifndef UNLOCKED_DEVICE
    fprintf(stdout, "Run BootMenu..\n");
    exec_script(FILE_PRE_MENU, DISABLE);
    int mode = get_bootmode(0,0);
    result = run_bootmenu_ui(mode);
#else
    // unlocked devices can exec bootmenu directly in init.rc
    result = run_bootmenu();
    bypass_sign("no");
#endif

    sync();
    return result;
  }
  else if (argc >= 3 && 0 == strcmp(argv[2], "userdata")) {

    /* init.rc call: "exec logwrapper mount.sh userdata" */

    result = run_bootmenu();
    real_execute(argc, argv);
    bypass_sign("no");
    sync();
    return result;
  }
  else if (argc >= 3 && 0 == strcmp(argv[2], "pds")) {

    /* kept for stock rom compatibility, please use postbootmenu parameter */

    real_execute(argc, argv);
    exec_script(FILE_OVERCLOCK, DISABLE);
    result = exec_script(FILE_POST_MENU, DISABLE);
    bypass_sign("no");
    sync();
    return result;
  }
  else {
    return real_execute(argc, argv);
  }

  return 0;
}


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

#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "overclock.h"
#include "extendedcommands.h"
#include "minui/minui.h"
#include "bootmenu_ui.h"

struct overclock_config
{
 const char *name;
 int value;
};

struct overclock_config overclock[] = {
  { "enable", 1 },
  { "scaling", 0 },
  { "sched", 1 },
  { "clk1", 300 },
  { "clk2", 600 },
  { "clk3", 800 },
  { "clk4", 1000 },
  { "vsel1", 33 },
  { "vsel2", 48 },
  { "vsel3", 58 },
  { "vsel4", 62 },
  { NULL, 0 },
};

int
get_overclock_value(char* name) {
  struct overclock_config *config;

  for (config = overclock; config->name != NULL; ++config) {
      if (!strcmp(config->name, name)) {
        return config->value;
      }
    }
  return -1;
}

int
set_overclock_value(char* name, int value) {
  struct overclock_config *config;

  for (config = overclock; config->name != NULL; ++config) {
      if (!strcmp(config->name, name)) {
        config->value = value;
        return config->value;
      }
    }
  return -1;
}

int
get_overclock_config(void) {
  FILE *fp;
  char name[255];
  struct overclock_config *config;

  if ((fp = fopen(FILE_OVERCLOCK_CONF, "r")) == NULL) {
    return 1;
  }

  while((fscanf(fp, "%s", name)) != EOF) {

    for (config = overclock; config->name != NULL; ++config) {
      if (!strcmp(config->name, name)) {
        fscanf(fp, "%d", &config->value);
      }
    }
  }
  fclose(fp);
  return 0;
}

int
set_overclock_config(void) {
  FILE *fp;
  struct overclock_config *config;

  if ((fp = fopen(FILE_OVERCLOCK_CONF, "w")) == NULL) {
    return 1;
  }

  for (config = overclock; config->name != NULL; ++config) {
    fprintf(fp, "%s", config->name);
    fprintf(fp, " %d\n", config->value);
  }
  fclose(fp);
  return 0;
}


int
menu_overclock_status(int intl_value) {

#define OVERCLOCK_STATUS_DISABLE      0
#define OVERCLOCK_STATUS_ENABLE       1

  static char** title_headers = NULL;

  if (title_headers == NULL) {
    char* headers[] = { " # --> Set Enable/Disable -->",
                        "",
                        NULL };
    title_headers = prepend_title((const char**)headers);
  }

  char* items[2][2] =  {
                         { "*[Disable]", " [Disable]" },
                         { "*[Enable]", " [Enable]" },
                       };

  int mode = intl_value;

  for (;;) {
    struct UiMenuItem options[4];
    int i;

    for (i = 0; i < 2; ++i) {
      if (mode == i)
        options[i] = buildMenuItem(MENUITEM_SMALL, items[i][0], NULL);
      else
        options[i] = buildMenuItem(MENUITEM_SMALL, items[i][1], NULL);
    }

    options[2] = buildMenuItem(MENUITEM_SMALL, "<--Go Back", NULL);
    options[3] = buildMenuItem(MENUITEM_NULL, NULL, NULL);

    struct UiMenuResult ret = get_menu_selection(title_headers, TABS, options, 1, mode);

    switch (ret.result) {
      case OVERCLOCK_STATUS_ENABLE:
        mode = 1;
        break;

      case OVERCLOCK_STATUS_DISABLE:
        mode = 0;
        break;

      default:
        return mode;
    }
  }

  return mode;
}

# define MENU_SYSTEM ""
# define MENU_OVERCLOCK "CPU settings"

int
menu_overclock_sched(void) {

#define OVERCLOCK_SCALING_CFQ         0
#define OVERCLOCK_SCALING_DEADLINE    1
#define OVERCLOCK_SCALING_SIO         2

#define SCHED_COUNT 3

  static char** title_headers = NULL;

  if (title_headers == NULL) {
    char* headers[] = {
      " #" MENU_SYSTEM MENU_OVERCLOCK " Sched -->",
      "",
      NULL
    };
    title_headers = prepend_title((const char**)headers);
  }


  char* items[SCHED_COUNT][2] = {
    { "*[CFQ]", " [CFQ]" },
    { "*[Deadline]",  " [Deadline]" },
    { "*[SIO]",     " [SIO]" },
  };

  for (;;) {

    struct UiMenuItem options[SCHED_COUNT+2];
    int i;
    int mode = get_overclock_value("sched");

    for (i = 0; i < SCHED_COUNT; ++i) {
      if (mode == i)
        options[i] = buildMenuItem(MENUITEM_SMALL, items[i][0], NULL);
      else
        options[i] = buildMenuItem(MENUITEM_SMALL, items[i][1], NULL);
    }
    options[SCHED_COUNT]   = buildMenuItem(MENUITEM_SMALL, "<--Go Back", NULL);
    options[SCHED_COUNT+1] = buildMenuItem(MENUITEM_NULL, NULL, NULL);

    struct UiMenuResult ret = get_menu_selection(title_headers, TABS, options, 1, mode);

    switch (ret.result) {
      case OVERCLOCK_SCALING_CFQ:
        set_overclock_value("sched", 0);
        ui_print("Set CFQ.\n");
        break;

      case OVERCLOCK_SCALING_DEADLINE:
        set_overclock_value("sched", 1);
        ui_print("Set Deadline.\n");
        break;

      case OVERCLOCK_SCALING_SIO:
        set_overclock_value("sched", 2);
        ui_print("Set SIO.\n");
        break;

      default:
        return 0;
    }
  }

  return 0;
}

int
menu_overclock_scaling(void) {

#define OVERCLOCK_SCALING_Interactive    0
#define OVERCLOCK_SCALING_Ondemand       1
#define OVERCLOCK_SCALING_Performance    2
#define OVERCLOCK_SCALING_Powersave      3
#define OVERCLOCK_SCALING_Boosted        4
#define OVERCLOCK_SCALING_Smartass       5

  static char** title_headers = NULL;

  if (title_headers == NULL) {
    char* headers[] = {
      " #" MENU_SYSTEM MENU_OVERCLOCK " Scaling -->",
      "",
      NULL
    };
    title_headers = prepend_title((const char**)headers);
  }

#define GOV_COUNT 6
  char* items[GOV_COUNT][2] = {
    { "*[Interactive]",  " [Interactive]" },
    { "*[Ondemand]",     " [Ondemand]" },
    { "*[Performance]",  " [Performance]" },
    { "*[Powersave]",    " [Powersave]" },
    { "*[Boosted]",      " [Boosted]" },
    { "*[Smartass]",     " [Smartass]" },
  };

  for (;;) {

    struct UiMenuItem options[GOV_COUNT+2];
    int i;
    int mode = get_overclock_value("scaling");

    for (i = 0; i < GOV_COUNT; ++i) {
      if (mode == i)
        options[i] = buildMenuItem(MENUITEM_SMALL, items[i][0], NULL);
      else
        options[i] = buildMenuItem(MENUITEM_SMALL, items[i][1], NULL);
    }
    options[GOV_COUNT]   = buildMenuItem(MENUITEM_SMALL, "<--Go Back", NULL);
    options[GOV_COUNT+1] = buildMenuItem(MENUITEM_NULL, NULL, NULL);

    struct UiMenuResult ret = get_menu_selection(title_headers, TABS, options, 1, mode);

    switch (ret.result) {
      case OVERCLOCK_SCALING_Interactive:
        set_overclock_value("scaling", 0);
        ui_print("Set Interactive.\n");
        break;

      case OVERCLOCK_SCALING_Ondemand:
        set_overclock_value("scaling", 1);
        ui_print("Set Ondemand.\n");
        break;

      case OVERCLOCK_SCALING_Performance:
        set_overclock_value("scaling", 2);
        ui_print("Set Performance.\n");
        break;

      case OVERCLOCK_SCALING_Powersave:
        set_overclock_value("scaling", 3);
        ui_print("Set Powersave.\n");
        break;

      case OVERCLOCK_SCALING_Boosted:
        set_overclock_value("scaling", 4);
        ui_print("Set Boostedass.\n");
        break;

      case OVERCLOCK_SCALING_Smartass:
        set_overclock_value("scaling", 5);
        ui_print("Set Smartass.\n");
        break;

      default:
        return 0;
    }
  }

  return 0;
}


int
menu_set_value(char* name, int intl_value, int min_value, int max_value, int step) {

#define SETVALUE_TITLE     0
#define SETVALUE_SEP       1
#define SETVALUE_ADD       2
#define SETVALUE_SUB       3
#define SETVALUE_BACK      4

  static char** title_headers = NULL;
  int select = 0;

  if (title_headers == NULL) {
    char* headers[] = { " # --> Set Value -->",
                        "",
                        NULL };
    title_headers = prepend_title((const char**)headers);
  }

  struct UiMenuItem items[6];
    items[0] = buildMenuItem(MENUITEM_SMALL, (char*)malloc(sizeof(char)*64), NULL);
    items[1] = buildMenuItem(MENUITEM_SMALL, "----------------------", NULL);
    items[2] = buildMenuItem(MENUITEM_SMALL, (char*)malloc(sizeof(char)*64), NULL);
    items[3] = buildMenuItem(MENUITEM_SMALL, (char*)malloc(sizeof(char)*64), NULL);
    items[4] = buildMenuItem(MENUITEM_SMALL, "<--Go Back", NULL);
    items[5] = buildMenuItem(MENUITEM_NULL, NULL, NULL);

  int value = intl_value;

  for (;;) {
    if (value < min_value) value = min_value;
    if (value > max_value) value = max_value;

    sprintf(items[0].title, "%s: [%d]", name, value);
    sprintf(items[2].title, "[+%d %s]", step, name);
    sprintf(items[3].title, "[-%d %s]", step, name);

    struct UiMenuResult ret = get_menu_selection(title_headers, TABS, items, 1, select);

    switch (ret.result) {
      case SETVALUE_ADD:
        value += step; break;
      case SETVALUE_SUB:
        value -= step; break;

      case SETVALUE_BACK:
        free(items[0].title); free(items[2].title); free(items[3].title);
        return value;

      default:
        break;
    }
    select = ret.result;
  }

  free(items[0].title); free(items[2].title); free(items[3].title);
  return value;
}

int
show_menu_overclock(void) {

#define OVERCLOCK_STATUS                  0
#define OVERCLOCK_SCALING                 2
#define OVERCLOCK_SCHED                   3

#define OVERCLOCK_CLOCK1                  4
#define OVERCLOCK_CLOCK2                  5
#define OVERCLOCK_CLOCK3                  6
#define OVERCLOCK_CLOCK4                  7
#define OVERCLOCK_VSEL1                   8
#define OVERCLOCK_VSEL2                   9
#define OVERCLOCK_VSEL3                   10
#define OVERCLOCK_VSEL4                   11

#define OVERCLOCK_DEFAULT                 12
#define OVERCLOCK_SAVE                    13
#define OVERCLOCK_GOBACK                  14

#define OC_MALLOC_FIRST 2
#define OC_MALLOC_LAST  15

  static char** title_headers = NULL;
  int i, select = 0;

  if (title_headers == NULL) {
    char* headers[] = {
      " #" MENU_SYSTEM MENU_OVERCLOCK,
      "",
      NULL 
    };
    title_headers = prepend_title((const char**)headers);
  }

  get_overclock_config();
  struct UiMenuItem items[16];

  items[OVERCLOCK_STATUS] = buildMenuItem(MENUITEM_SMALL, NULL, NULL);
  items[1] = buildMenuItem(MENUITEM_SMALL, "----------------------", NULL);
  items[OVERCLOCK_SCALING] = buildMenuItem(MENUITEM_SMALL, NULL, NULL);
  items[OVERCLOCK_SCHED] = buildMenuItem(MENUITEM_SMALL, NULL, NULL);

  for (i = OC_MALLOC_FIRST; i <= OC_MALLOC_LAST; i++) {
    items[i] = buildMenuItem(MENUITEM_SMALL, (char*)malloc(sizeof(char)*48), NULL);
  }

  items[OVERCLOCK_DEFAULT] = buildMenuItem(MENUITEM_SMALL, "Set defaults", NULL);
  items[OVERCLOCK_SAVE] = buildMenuItem(MENUITEM_SMALL, "Save", NULL);
  items[OVERCLOCK_GOBACK] = buildMenuItem(MENUITEM_SMALL, "<--Go Back", NULL);
  items[15] = buildMenuItem(MENUITEM_NULL, NULL, NULL);

  for (;;) {

    switch (get_overclock_value("enable")) {
      case 0: items[0].title = "+Status: [Disable]"; break;
      case 1: items[0].title = "+Status: [Enable]"; break;

      default: items[0].title = "+Status: [Unknown]"; break;
    }

    switch (get_overclock_value("scaling")) {
      case 0: items[OVERCLOCK_SCALING].title = "+Scaling: [Interactive]"; break;
      case 1: items[OVERCLOCK_SCALING].title = "+Scaling: [Ondemand]"; break;
      case 2: items[OVERCLOCK_SCALING].title = "+Scaling: [Performance]"; break;
      case 3: items[OVERCLOCK_SCALING].title = "+Scaling: [Powersave]"; break;
      case 4: items[OVERCLOCK_SCALING].title = "+Scaling: [Boosted]"; break;
      case 5: items[OVERCLOCK_SCALING].title = "+Scaling: [Smartass]"; break;

      default: items[OVERCLOCK_SCALING].title = " Scaling: [Unknown]"; break;
    }

    switch (get_overclock_value("sched")) {
      case 0: items[OVERCLOCK_SCHED].title = "+Sched: [CFQ]"; break;
      case 1: items[OVERCLOCK_SCHED].title = "+Sched: [Deadline]"; break;
      case 2: items[OVERCLOCK_SCHED].title = "+Sched: [SIO]"; break;

      default: items[OVERCLOCK_SCHED].title = " Sched: [Unknown]"; break;
    }

    sprintf(items[OVERCLOCK_CLOCK1].title, "+Clk1: [%d]", get_overclock_value("clk1"));
    sprintf(items[OVERCLOCK_CLOCK2].title, "+Clk2: [%d]", get_overclock_value("clk2"));
    sprintf(items[OVERCLOCK_CLOCK3].title, "+Clk3: [%d]", get_overclock_value("clk3"));
    sprintf(items[OVERCLOCK_CLOCK4].title, "+Clk4: [%d]", get_overclock_value("clk4"));
    sprintf(items[OVERCLOCK_VSEL1].title, "+Vsel1: [%d]", get_overclock_value("vsel1"));
    sprintf(items[OVERCLOCK_VSEL2].title, "+Vsel2: [%d]", get_overclock_value("vsel2"));
    sprintf(items[OVERCLOCK_VSEL3].title, "+Vsel3: [%d]", get_overclock_value("vsel3"));
    sprintf(items[OVERCLOCK_VSEL4].title, "+Vsel4: [%d]", get_overclock_value("vsel4"));
    struct UiMenuResult ret = get_menu_selection(title_headers, TABS, items, 1, select);

    switch (ret.result) {
      case OVERCLOCK_STATUS:
        set_overclock_value("enable", menu_overclock_status(get_overclock_value("enable"))); break;

      case OVERCLOCK_SCALING:
        menu_overclock_scaling(); break;

      case OVERCLOCK_SCHED:
         menu_overclock_sched(); break;

      case OVERCLOCK_CLOCK1:
        set_overclock_value("clk1", menu_set_value("Clk1", get_overclock_value("clk1"), 200, 2000, 10)); break;

      case OVERCLOCK_CLOCK2:
        set_overclock_value("clk2", menu_set_value("Clk2", get_overclock_value("clk2"), 200, 2000, 10)); break;

      case OVERCLOCK_CLOCK3:
        set_overclock_value("clk3", menu_set_value("Clk3", get_overclock_value("clk3"), 200, 2000, 10)); break;

      case OVERCLOCK_CLOCK4:
        set_overclock_value("clk4", menu_set_value("Clk4", get_overclock_value("clk4"), 200, 2000, 10)); break;

      case OVERCLOCK_VSEL1:
        set_overclock_value("vsel1", menu_set_value("Vsel1", get_overclock_value("vsel1"), 10, 300, 1)); break;

      case OVERCLOCK_VSEL2:
        set_overclock_value("vsel2", menu_set_value("Vsel2", get_overclock_value("vsel2"), 10, 300, 1)); break;

      case OVERCLOCK_VSEL3:
        set_overclock_value("vsel3", menu_set_value("Vsel3", get_overclock_value("vsel3"), 10, 300, 1)); break;

      case OVERCLOCK_VSEL4:
        set_overclock_value("vsel4", menu_set_value("Vsel4", get_overclock_value("vsel4"), 10, 300, 1)); break;

      case OVERCLOCK_SAVE:
        ui_print("Saving.... ");
        set_overclock_config();
        ui_print("Done.\n");
        break;

      case OVERCLOCK_DEFAULT:
        ui_print("Set defaults...");
	set_overclock_value("enable", 1);
	set_overclock_value("scaling", 0);
	set_overclock_value("sched", 1);
	set_overclock_value("clk1", 300);
	set_overclock_value("clk2", 600);
	set_overclock_value("clk3", 800);
	set_overclock_value("clk4", 1000);
	set_overclock_value("vsel1", 33);
	set_overclock_value("vsel2", 48);
	set_overclock_value("vsel3", 58);
	set_overclock_value("vsel4", 62);
        set_overclock_config();
        break;

      default:
        return 0;
    }
    select = ret.result;
  }

  //release mallocs
  for (select=OC_MALLOC_FIRST; select<=OC_MALLOC_LAST; select++) {
    free(items[select].title);
  }

  return 0;
}

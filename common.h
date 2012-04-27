/*
 * Copyright (C) 2007-2012 The Android Open Source Project
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

#ifndef RECOVERY_COMMON_H
#define RECOVERY_COMMON_H

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <linux/types.h>

#define RESULT_TAB  1
#define RESULT_LIST 2

struct UiMenuResult {
  int type;
  int result;
};

#define MENUITEM_FULL 1
#define MENUITEM_SMALL 2
#define MENUITEM_MINUI_STANDARD 3
#define MENUITEM_NULL -1
struct UiMenuItem {
  int type;
  char *title;
  char *description;
};

#define UINPUTEVENT_TYPE_KEY 0
#define UINPUTEVENT_TYPE_TOUCH_START 1
#define UINPUTEVENT_TYPE_TOUCH_DRAG 2
#define UINPUTEVENT_TYPE_TOUCH_RELEASE 3
struct ui_input_event {
  struct timeval time;
  __u16 type;
  __u16 code;
  __s32 value;
  int utype;
  int posx;
  int posy;
};

#define TOUCHRESULT_TYPE_EMPTY -1
#define TOUCHRESULT_TYPE_ONCLICK_LIST 0
#define TOUCHRESULT_TYPE_ONCLICK_TAB 1
struct ui_touchresult {
  int type;
  int item;
};

// Initialize the graphics system.
void ui_init();
void ui_final();

void evt_init();
void evt_exit();

// Stop/resume redraw thread
void ui_stop_redraw(void);
void ui_resume_redraw(void);

// Use KEY_* codes from <linux/input.h> or KEY_DREAM_* from "minui/minui.h".
int ui_wait_key();            // waits for a key/button press, returns the code
int ui_wait_input(struct ui_input_event*);// waits for a input event
int ui_key_pressed(int key);  // returns >0 if the code is currently pressed
int ui_text_visible();        // returns >0 if text log is currently visible
void ui_show_text(int visible);
void ui_clear_key_queue();

// Write a message to the on-screen log shown with Alt-L (also to stderr).
// The screen is small, and users may need to report these messages to support,
// so keep the output short and not too cryptic.
void ui_print(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
// same without args
void ui_print_str(char *str);

// Display some header text followed by a menu of items, which appears
// at the top of the screen (in place of any scrolling ui_print()
// output, if necessary).
void ui_start_menu(char** headers, char** tabs, struct UiMenuItem* items, int initial_selection);
// Set the menu highlight to the given index, and return it (capped to
// the range [0..numitems).
int ui_menu_select(int sel);
// End menu mode, resetting the text overlay so that ui_print()
// statements will be displayed.
void ui_end_menu();

struct UiMenuItem buildMenuItem(int type, char *title, char *description);

// Tabs funcs
void ui_set_activeTab(int);
int ui_get_activeTab(void);

// Set the icon (normally the only thing visible besides the progress bar).
enum {
  BACKGROUND_ICON_NONE,
  BACKGROUND_DEFAULT,
  BACKGROUND_ALT,
  NUM_BACKGROUND_ICONS
};
void ui_set_background(int icon);

// Show a progress bar and define the scope of the next operation:
//   portion - fraction of the progress bar the next operation will use
//   seconds - expected time interval (progress bar moves at this minimum rate)
void ui_show_progress(float portion, int seconds);
void ui_set_progress(float fraction);  // 0.0 - 1.0 within the defined scope

// Default allocation of progress bar segments to operations
static const int VERIFICATION_PROGRESS_TIME = 60;
static const float VERIFICATION_PROGRESS_FRACTION = 0.25;
static const float DEFAULT_FILES_PROGRESS_FRACTION = 0.4;
static const float DEFAULT_IMAGE_PROGRESS_FRACTION = 0.1;

// Show a rotating "barberpole" for ongoing operations.  Updates automatically.
void ui_show_indeterminate_progress();

// Hide and reset the progress bar.
void ui_reset_progress();

#define LOGE(...) ui_print("E:" __VA_ARGS__)
#define LOGW(...) fprintf(stdout, "W:" __VA_ARGS__)
#define LOGI(...) fprintf(stdout, "I:" __VA_ARGS__)

#if 0
#define LOGV(...) fprintf(stdout, "V:" __VA_ARGS__)
#define LOGD(...) fprintf(stdout, "D:" __VA_ARGS__)
#else
#define LOGV(...) do {} while (0)
#define LOGD(...) do {} while (0)
#endif

#define STRINGIFY(x) #x
#define EXPAND(x) STRINGIFY(x)

static void finish(void);

enum { 
  DISABLE,
  ENABLE
};

//turn on/off a led
int led_alert(const char* color, int value);

int  ui_create_bitmaps();
void ui_free_bitmaps();

//checkup
int checkup_report(void);

#endif  // RECOVERY_COMMON_H

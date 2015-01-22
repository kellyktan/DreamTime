#include <pebble.h>

#define SLEEP_DELAY_DEMO 2 //840
#define SLEEP_CYCLE_DEMO 5 //5400
#define SNOOZE_TIME_DEMO 5 //300
  
#define SLEEP_DELAY 840 //840 - 14 minutes
#define SLEEP_CYCLE 5400 //5400 - 90 minutes
#define SNOOZE_TIME 300 //300 - 5 minutes
#define DAY 86400 //24*60*60
  
// Vibe Patterns
static const uint32_t const wakeup_sequence[] = { 200, 100, 400, 100, 200, 100, 400, 200,
                                                  200, 100, 400, 100, 200, 100, 400, 200,
                                                  200, 100, 400, 100, 200, 100, 400, 200,
                                                  200, 100, 400, 100, 200, 100, 400 };
static const uint32_t const sleep_sequence[] = { 200, 100, 400 };


static int window_count;
static bool demo = false;

//Main Menu window
static Window *main_menu_window;
static TextLayer *main_text_layer;
static TextLayer *main_set_text_layer;
static TextLayer *main_now_text_layer;
static bool set_time;

//Wakeup specific time window
static Window *hour_window;
static TextLayer *hour_text_layer;
static int hour_count = 1;
static Window *minute_window;
static TextLayer *minute_text_layer;
static int minute_count = 0;
static Window *ampm_window;
static TextLayer *ampm_text_layer;
static TextLayer *am_text_layer;
static TextLayer *pm_text_layer;
static bool am = true;

// Wakeup selection window
static Window *s_menu_window;
static MenuLayer *s_menu_layer;
static TextLayer *s_error_text_layer;

// Wakeup info window with cancel button
static Window *sleep_window;
static TextLayer *sleep_text_layer;
static TextLayer *wakeup_time_text_layer;
static char *wakeup_time_text;
static char *wakeup_time_text2;
static TextLayer *s_cancel_text_layer;

// Wakeup wakeup is done window
static Window *s_wakeup_window;
static TextLayer *s_wakeup_text_layer;
static TextLayer *s_snooze_text_layer;

static WakeupId s_wakeup_id = -1;
static time_t s_wakeup_timestamp = 0;
static int frames = 0;

typedef struct {
  int sleep_cycles;
} SleepInfo;

// Array of different teas for tea timer
// {<Tea Name>, <Brew time in minutes>}
SleepInfo sleep_array[] = {
  {1},
  {2},
  {3},
  {4},
  {5},
  {6},
  {7}
};

enum {
  PERSIST_WAKEUP // Persistent storage key for wakeup_id
};

static time_t get_next_time(int hour, int minute, bool am) {
  // something something 11 hours
  const time_t now = time(NULL);
  struct tm *new_time = localtime(&now);
  int new_hour = hour;
  if (am && new_hour == 12) {
    new_hour = 0;
  } else if (!am && !(new_hour == 12)) {
    new_hour += 12;
  }
  int actual = new_time->tm_hour*60*60 + new_time->tm_min*60 + new_time->tm_sec;
  int want = new_hour*60*60 + minute*60;
  if (actual - want < 0) {
    return now + want - actual;
  } else if (actual - want > 0) {
    return now + (DAY - actual) + want;
  } else {
    return now + DAY;
  }
}

static void set_num_text(int num, TextLayer *text_layer) {
  if (num == 0) {
    text_layer_set_text(text_layer, "0");
    return;
  } else {
    int index = 126;
    while (num != 0) {
      wakeup_time_text2[index] = (num % 10) + (int)'0';
      num /= 10;
      index--;
    }
    text_layer_set_text(text_layer, wakeup_time_text2 + index + 1);
  }
}

/********************************* Main Menu **************************************/

static void back_handler(ClickRecognizerRef recognizer, void *context) {
  // leave app
  window_count = 0;
  window_stack_pop_all(true);
  demo = false;
}

static void set_handler(ClickRecognizerRef recognizer, void *context) {
  set_time = true;
  // go to set time
  window_count++;
  window_stack_push(hour_window, true);
}

// Cancel the current wakeup event on the countdown screen
static void now_handler(ClickRecognizerRef recognizer, void *context) {
  set_time = false;
  // go to now time
  window_count++;
  window_stack_push(s_menu_window, true);
  demo = false;
}

static void demo_handler(ClickRecognizerRef recognizer, void *context) {
  demo = true;
  set_time = false;
  window_count++;
  window_stack_push(s_menu_window, true);
}

static void main_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, back_handler);
  window_single_click_subscribe(BUTTON_ID_UP, set_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, now_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, demo_handler);
}

static void main_menu_window_load(Window *window) {
  window_set_click_config_provider(window, main_click_config_provider);

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  main_text_layer = text_layer_create((GRect) { .origin = {0, 62}, .size = {bounds.size.w, 20}});
  text_layer_set_background_color(main_text_layer, GColorClear);
  text_layer_set_text_color(main_text_layer, GColorBlack);
  text_layer_set_text_alignment(main_text_layer, GTextAlignmentCenter);
  text_layer_set_text(main_text_layer, "DreamTime");
  layer_add_child(window_layer, text_layer_get_layer(main_text_layer));
    
  // Set time
  main_set_text_layer = text_layer_create((GRect) { .origin = {85, 0}, .size = {144, 28}});
  text_layer_set_text(main_set_text_layer, "Set Time");
  text_layer_set_text_alignment(main_set_text_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(main_set_text_layer));
  
  // Sleep now
  main_now_text_layer = text_layer_create((GRect) { .origin = {75, 130}, .size = {96, 28}});
  text_layer_set_text(main_now_text_layer, "Sleep Now");
  text_layer_set_text_alignment(main_now_text_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(main_now_text_layer));
}

static void main_menu_window_unload(Window *window) {
  text_layer_destroy(main_text_layer);
  text_layer_destroy(main_set_text_layer);
  text_layer_destroy(main_now_text_layer);
}

/********************************** MENU ******************************************/

static void menu_layer_select_callback(
    struct MenuLayer *s_menu_layer, MenuIndex *cell_index, void *callback_context) {
  // If we were displaying s_error_text_layer, remove it and return
  if (!layer_get_hidden(text_layer_get_layer(s_error_text_layer))) {
    layer_set_hidden(text_layer_get_layer(s_error_text_layer), true);
    return;
  }
  
  // Wakeup time is a timestamp in the future
  time_t wakeup_time;
  if (set_time) {
    wakeup_time = get_next_time(hour_count, minute_count, am) - SLEEP_DELAY - sleep_array[cell_index->row].sleep_cycles * SLEEP_CYCLE;
  }
  else if (demo) {
    // so time(NULL) + delay_time_in_seconds = wakeup_time*/
    wakeup_time = time(NULL) + SLEEP_DELAY_DEMO + sleep_array[cell_index->row].sleep_cycles * SLEEP_CYCLE_DEMO;
  } else {
    wakeup_time = time(NULL) + SLEEP_DELAY + sleep_array[cell_index->row].sleep_cycles * SLEEP_CYCLE;
  }

  // Use the tea_array index as the wakeup reason, so on wakeup trigger
  // we know which tea is brewed
  s_wakeup_id = wakeup_schedule(wakeup_time, cell_index->row, true);

  // If we couldn't schedule the wakeup event, display error_text overlay
  if (s_wakeup_id <= 0) {
    layer_set_hidden(text_layer_get_layer(s_error_text_layer), false);
    return;
  }

  // Store the handle so we can cancel if necessary, or look it up next launch
  persist_write_int(PERSIST_WAKEUP, s_wakeup_id);

  // Switch to countdown window
  while (window_count > 0) {
    window_stack_pop(false);
    window_count--;
  }
  window_count++;
  window_stack_push(sleep_window, false);
}

static uint16_t menu_layer_get_sections_count_callback(
  struct MenuLayer *menulayer, uint16_t section_index, void *callback_context) {
  int count = sizeof(sleep_array) / sizeof(SleepInfo);
  return count;
}

static void draw_row_handler(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context) {
  char* wakeup_str = malloc(sizeof(char) * 128);
  time_t wakeup_time;
  if (set_time) {
    wakeup_time = get_next_time(hour_count, minute_count, am) - SLEEP_DELAY 
      - sleep_array[cell_index->row].sleep_cycles * SLEEP_CYCLE;
  } else if (demo) {
    // so time(NULL) + delay_time_in_seconds = wakeup_time
    wakeup_time = time(NULL) + SLEEP_DELAY_DEMO + sleep_array[cell_index->row].sleep_cycles * SLEEP_CYCLE_DEMO;    
  }
  else {
    // so time(NULL) + delay_time_in_seconds = wakeup_time
    wakeup_time = time(NULL) + SLEEP_DELAY + sleep_array[cell_index->row].sleep_cycles * SLEEP_CYCLE;
  }
  struct tm *tmptr = localtime(&wakeup_time);
  strftime(wakeup_str, 128, "%I:%M %p", tmptr);

  menu_cell_basic_draw(ctx, cell_layer, wakeup_str, NULL, NULL);
  free(wakeup_str);
}

static void menu_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, 
    (MenuLayerCallbacks){
    .get_num_rows = menu_layer_get_sections_count_callback,
    .draw_row = draw_row_handler,
    .select_click = menu_layer_select_callback
    }); 
  menu_layer_set_click_config_onto_window(s_menu_layer,	window);
  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));

  s_error_text_layer = text_layer_create((GRect) { .origin = {0, 44}, .size = {bounds.size.w, 60}});
  text_layer_set_text(s_error_text_layer, "Cannot\nschedule");
  text_layer_set_text_alignment(s_error_text_layer, GTextAlignmentCenter);
  text_layer_set_font(s_error_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_color(s_error_text_layer, GColorWhite);
  text_layer_set_background_color(s_error_text_layer, GColorBlack);
  layer_set_hidden(text_layer_get_layer(s_error_text_layer), true);
  layer_add_child(window_layer, text_layer_get_layer(s_error_text_layer));
}

static void menu_window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
  text_layer_destroy(s_error_text_layer);
}

static void timer_handler(void *data) {
  if (s_wakeup_timestamp == 0) {
    // get the wakeup timestamp for showing a countdown
    wakeup_query(s_wakeup_id, &s_wakeup_timestamp);
  }
  struct tm *tmptr = localtime(&s_wakeup_timestamp);
  strftime(wakeup_time_text, 128, "%I:%M %p", tmptr);
  layer_mark_dirty(text_layer_get_layer(wakeup_time_text_layer));
  app_timer_register(1000, timer_handler, data);
}

/******************************* Insert Time ************************************/

// HOUR

static void hour_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  hour_count = 1;
  window_count--;
  window_stack_pop(true);
  set_time = false;
}

static void hour_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  hour_count++;
  if (hour_count > 12) {
    hour_count = 1;
  }
  set_num_text(hour_count, hour_text_layer);
}

static void hour_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  hour_count--;
  if (hour_count == 0) {
    hour_count = 12;
  }
  set_num_text(hour_count, hour_text_layer);
}

static void hour_next_click_handler(ClickRecognizerRef recognizer, void *context) {
  // go to minute selector
  window_count++;
  window_stack_push(minute_window, true);
}

static void hour_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, hour_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, hour_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, hour_back_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, hour_next_click_handler);
}

static void hour_window_load(Window *window) {
  window_set_click_config_provider(window, hour_click_config_provider);
  
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  sleep_text_layer = text_layer_create((GRect) { .origin = {0, 32}, .size = {bounds.size.w, 20}});
  text_layer_set_text(sleep_text_layer, "Set Hour");
  text_layer_set_text_alignment(sleep_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(sleep_text_layer));

  hour_text_layer = text_layer_create((GRect) {.origin = {0, 72}, .size = {bounds.size.w, 20}});
  text_layer_set_text(hour_text_layer, "1");
  text_layer_set_text_alignment(hour_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(hour_text_layer));
}

static void hour_window_unload(Window *window) {
  text_layer_destroy(hour_text_layer);
}

// MINUTE

static void minute_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  minute_count = 0;
  window_count--;
  window_stack_pop(true);
  set_num_text(hour_count, hour_text_layer);
}

static void minute_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  minute_count++;
  if (minute_count >= 60) {
    minute_count = 0;
  }
  set_num_text(minute_count, minute_text_layer);
}

static void minute_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  minute_count--;
  if (minute_count < 0) {
    minute_count = 59;
  }
  set_num_text(minute_count, minute_text_layer);
}

static void minute_next_click_handler(ClickRecognizerRef recognizer, void *context) {
  // go to ampm selector
  window_count++;
  window_stack_push(ampm_window, true);
}

static void minute_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, minute_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, minute_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, minute_back_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, minute_next_click_handler);
}

static void minute_window_load(Window *window) {
  window_set_click_config_provider(window, minute_click_config_provider);
  
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  sleep_text_layer = text_layer_create((GRect) { .origin = {0, 32}, .size = {bounds.size.w, 20}});
  text_layer_set_text(sleep_text_layer, "Set Minute");
  text_layer_set_text_alignment(sleep_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(sleep_text_layer));

  minute_text_layer = text_layer_create((GRect) {.origin = {0, 72}, .size = {bounds.size.w, 20}});
  text_layer_set_text(minute_text_layer, "0");
  text_layer_set_text_alignment(minute_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(minute_text_layer));
}

static void minute_window_unload(Window *window) {
  text_layer_destroy(minute_text_layer);
}

// AM/PM

static void ampm_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  am = true;
  window_count--;
  window_stack_pop(true);
  set_num_text(minute_count, minute_text_layer);
}

static void am_click_handler(ClickRecognizerRef recognizer, void *context) {
  am = true;
  text_layer_set_text(ampm_text_layer, "AM");
}

static void pm_click_handler(ClickRecognizerRef recognizer, void *context) {
  am = false;
  text_layer_set_text(ampm_text_layer, "PM");
}

static void ampm_next_click_handler(ClickRecognizerRef recognizer, void *context) {
  // go to minute selector
  window_count++;
  window_stack_push(s_menu_window, true);
}

static void ampm_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, am_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, pm_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, ampm_back_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, ampm_next_click_handler);
}

static void ampm_window_load(Window *window) {
  window_set_click_config_provider(window, ampm_click_config_provider);
  
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  sleep_text_layer = text_layer_create((GRect) { .origin = {0, 32}, .size = {bounds.size.w, 20}});
  text_layer_set_text(sleep_text_layer, "Set AM/PM");
  text_layer_set_text_alignment(sleep_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(sleep_text_layer));

  ampm_text_layer = text_layer_create((GRect) {.origin = {0, 72}, .size = {bounds.size.w, 20}});
  text_layer_set_text(ampm_text_layer, "AM");
  text_layer_set_text_alignment(ampm_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(ampm_text_layer));
  
  // AM button text
  am_text_layer = text_layer_create((GRect) { .origin = {115, 0}, .size = {144, 28}});
  text_layer_set_text(am_text_layer, "AM");
  text_layer_set_text_alignment(am_text_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(am_text_layer));
  
  // PM button text
  pm_text_layer = text_layer_create((GRect) { .origin = {115, 130}, .size = {96, 28}});
  text_layer_set_text(pm_text_layer, "PM");
  text_layer_set_text_alignment(pm_text_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(pm_text_layer));
}

static void ampm_window_unload(Window *window) {
  text_layer_destroy(ampm_text_layer);
  text_layer_destroy(am_text_layer);
  text_layer_destroy(pm_text_layer);
}

/********************************* Sleep ************************************/

static void countdown_back_handler(ClickRecognizerRef recognizer, void *context) {
  window_count = 0;
  window_stack_pop_all(true); // Exit app while waiting for tea to brew
}

// Cancel the current wakeup event on the countdown screen
static void countdown_cancel_handler(ClickRecognizerRef recognizer, void *context) {
  wakeup_cancel(s_wakeup_id);
  s_wakeup_id = -1;
  persist_delete(PERSIST_WAKEUP);
  /*while(window_count > 0) {
    window_stack_pop(true); // Go back to tea selection window
    window_count--;
  }*/
  window_stack_push(main_menu_window, false);
  hour_count = 1;
  minute_count = 0;
  am = true;
  demo = false;
}

static void countdown_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, countdown_back_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, countdown_cancel_handler);
}

static void countdown_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  window_set_click_config_provider(window, countdown_click_config_provider);

  sleep_text_layer = text_layer_create((GRect) { .origin = {0, 32}, .size = {bounds.size.w, 20}});
  if (set_time) {
    text_layer_set_text(sleep_text_layer, "Sleep at");
  } else if (demo) {
    text_layer_set_text(sleep_text_layer, "Demo wakeup at");
  } else {
    text_layer_set_text(sleep_text_layer, "Wakeup at");
  }
  text_layer_set_text_alignment(sleep_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(sleep_text_layer));

  wakeup_time_text_layer = text_layer_create((GRect) {.origin = {0, 72}, .size = {bounds.size.w, 20}});
  text_layer_set_text(wakeup_time_text_layer, wakeup_time_text);
  text_layer_set_text_alignment(wakeup_time_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(wakeup_time_text_layer));

  // Place a cancel "X" next to the bottom button to cancel wakeup timer
  s_cancel_text_layer = text_layer_create((GRect) { .origin = {95, 130}, .size = {96, 28}});
  text_layer_set_text(s_cancel_text_layer, "Cancel");
  text_layer_set_text_alignment(s_cancel_text_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(s_cancel_text_layer));

  s_wakeup_timestamp = 0;
  app_timer_register(0, timer_handler, NULL);
}

static void countdown_window_unload(Window *window) {
  text_layer_destroy(wakeup_time_text_layer);
  text_layer_destroy(s_cancel_text_layer);
  text_layer_destroy(sleep_text_layer);
}

/******************************* Wakeup *************************************/

static void wakeup_click_handler(ClickRecognizerRef recognizer, void *context) {
  vibes_cancel();
  wakeup_cancel(s_wakeup_id);
  s_wakeup_id = -1;
  persist_delete(PERSIST_WAKEUP);
  /*while(window_count > 0) {
    window_stack_pop(true); // Go back to tea selection window
    window_count--;
  }*/
  window_stack_push(main_menu_window, false);
  hour_count = 1;
  minute_count = 0;
  am = true;
  demo = false;
}

static void snooze_click_handler(ClickRecognizerRef recognizer, void *context) {
  vibes_cancel();
  wakeup_cancel(s_wakeup_id);
  s_wakeup_id = -1;
  persist_delete(PERSIST_WAKEUP);
  
  // Wakeup time is a timestamp in the future
  // so time(NULL) + delay_time_in_seconds = wakeup_time
  time_t wakeup_time;
  if (demo) {
    wakeup_time = time(NULL) + SNOOZE_TIME_DEMO;
  } else {
    wakeup_time = time(NULL) + SNOOZE_TIME;
  }

  // Use the tea_array index as the wakeup reason, so on wakeup trigger
  // we know which tea is brewed
  s_wakeup_id = wakeup_schedule(wakeup_time, 0, true);

  // If we couldn't schedule the wakeup event, display error_text overlay
  if (s_wakeup_id <= 0) {
    layer_set_hidden(text_layer_get_layer(s_error_text_layer), false);
    return;
  }

  // Store the handle so we can cancel if necessary, or look it up next launch
  persist_write_int(PERSIST_WAKEUP, s_wakeup_id);

  // Switch to countdown window
  /*while(window_count > 0) {
    window_stack_pop(false);
    window_count--;
  }*/
  window_count++;
  window_stack_push(sleep_window, false);
  hour_count = 1;
  minute_count = 0;
  am = true;
}

static void wakeup_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, snooze_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, wakeup_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, wakeup_click_handler);
}

static void wakeup_window_load(Window *window) {
  window_set_click_config_provider(window, wakeup_click_config_provider);
  
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  s_wakeup_text_layer = text_layer_create((GRect) { .origin = {0, 62}, .size = {bounds.size.w, 20}});
  text_layer_set_background_color(s_wakeup_text_layer, GColorClear);
  text_layer_set_text_color(s_wakeup_text_layer, GColorBlack);
  text_layer_set_text_alignment(s_wakeup_text_layer, GTextAlignmentCenter);
  text_layer_set_text(s_wakeup_text_layer, "Wakey Wakey!");
  layer_add_child(window_layer, text_layer_get_layer(s_wakeup_text_layer));
  
  // Place a cancel "X" next to the bottom button to cancel wakeup timer
  s_cancel_text_layer = text_layer_create((GRect) { .origin = {95, 130}, .size = {96, 28}});
  text_layer_set_text(s_cancel_text_layer, "Cancel");
  text_layer_set_text_alignment(s_cancel_text_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(s_cancel_text_layer));
  
  // Place a cancel "X" next to the bottom button to snooze
  s_snooze_text_layer = text_layer_create((GRect) { .origin = {100, 0}, .size = {144, 28}});
  text_layer_set_text(s_snooze_text_layer, "Snooze");
  text_layer_set_text_alignment(s_snooze_text_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(s_snooze_text_layer));
}

static void wakeup_window_unload(Window *window) {
  text_layer_destroy(s_wakeup_text_layer);
  text_layer_destroy(s_cancel_text_layer);
  text_layer_destroy(s_snooze_text_layer);
}

static void wakeup_handler(WakeupId id, int32_t reason) {
  //Delete persistent storage value
  persist_delete(PERSIST_WAKEUP);
  
  if (!set_time) {
    // vibes
    VibePattern wakeup_vibes = {
      .durations = wakeup_sequence,
      .num_segments = ARRAY_LENGTH(wakeup_sequence),
    };
    vibes_enqueue_custom_pattern(wakeup_vibes);
    
    window_count++;
    window_stack_push(s_wakeup_window, false);
  } else {
    set_time = false;
    // vibes
    VibePattern wakeup_vibes = {
      .durations = sleep_sequence,
      .num_segments = ARRAY_LENGTH(sleep_sequence),
    };
    vibes_enqueue_custom_pattern(wakeup_vibes);
    wakeup_cancel(s_wakeup_id);
    s_wakeup_id = -1;

    time_t wakeup_time = get_next_time(hour_count, minute_count, am);
    hour_count = 1;
    minute_count = 0;
    am = true;

    // Use the tea_array index as the wakeup reason, so on wakeup trigger
    // we know which tea is brewed
    s_wakeup_id = wakeup_schedule(wakeup_time, 0, true);

    // If we couldn't schedule the wakeup event, display error_text overlay
    if (s_wakeup_id <= 0) {
      layer_set_hidden(text_layer_get_layer(s_error_text_layer), false);
      return;
    }

    // Store the handle so we can cancel if necessary, or look it up next launch
    persist_write_int(PERSIST_WAKEUP, s_wakeup_id);

    // Switch to countdown window
    while(window_count > 0) {
      window_stack_pop(false);
      window_count--;
    }
    window_count++;
    window_stack_push(sleep_window, false);
  }
}

/**************************** GENERAL **************************************/

static void init(void) {
  wakeup_time_text = malloc(128);
  wakeup_time_text2 = malloc(128);
  bool wakeup_scheduled = false;

  // Check if we have already scheduled a wakeup event
  // so we can transition to the countdown window
  if (persist_exists(PERSIST_WAKEUP)) {
    s_wakeup_id = persist_read_int(PERSIST_WAKEUP);
    // query if event is still valid, otherwise delete
    if (wakeup_query(s_wakeup_id, NULL)) {
      wakeup_scheduled = true;
    } else {
      persist_delete(PERSIST_WAKEUP);
      s_wakeup_id = -1;
    }
  }
  
  main_menu_window = window_create();
  window_set_window_handlers(main_menu_window, (WindowHandlers){
    .load = main_menu_window_load,
    .unload = main_menu_window_unload,
  });

  s_menu_window = window_create();
  window_set_window_handlers(s_menu_window, (WindowHandlers){
    .load = menu_window_load,
    .unload = menu_window_unload,
  });
  
  hour_window = window_create();
  window_set_window_handlers(hour_window, (WindowHandlers){
    .load = hour_window_load,
    .unload = hour_window_unload,
  });
  
  minute_window = window_create();
  window_set_window_handlers(minute_window, (WindowHandlers){
    .load = minute_window_load,
    .unload = minute_window_unload,
  });
  
  ampm_window = window_create();
  window_set_window_handlers(ampm_window, (WindowHandlers){
    .load = ampm_window_load,
    .unload = ampm_window_unload,
  });

  sleep_window = window_create();
  window_set_window_handlers(sleep_window, (WindowHandlers){
    .load = countdown_window_load,
    .unload = countdown_window_unload,
  });

  s_wakeup_window = window_create();
  window_set_window_handlers(s_wakeup_window, (WindowHandlers){
    .load = wakeup_window_load,
    .unload = wakeup_window_unload,
  });

  // Check to see if we were launched by a wakeup event
  if (launch_reason() == APP_LAUNCH_WAKEUP) {
    // If woken by wakeup event, get the event display "wakeup"
    WakeupId id = 0;
    int32_t reason = 0;
    if (wakeup_get_launch_event(&id, &reason)) {
      window_count = 0;
      window_stack_push(main_menu_window, false);
      wakeup_handler(id, reason);
    }
  } else if (wakeup_scheduled) {
    window_count = 0;
    window_stack_push(main_menu_window, false);
    window_count++;
    window_stack_push(sleep_window, false);
  } else {
    set_time = false;
    window_count = 0;
    window_stack_push(main_menu_window, false);
  }

  // subscribe to wakeup service to get wakeup events while app is running
  wakeup_service_subscribe(wakeup_handler);
}

static void deinit(void) {
  window_destroy(main_menu_window);
  free(wakeup_time_text);
  free(wakeup_time_text2);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_menu_window);

  app_event_loop();
  deinit();
}
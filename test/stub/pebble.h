// Strict stand-in for the SDK's pebble.h: only what main.c uses, with the
// SDK's signatures, and no libc headers pulled in (freestanding).
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define PBL_TOUCH 1
#define PBL_COLOR 1

typedef struct { int16_t x, y; } GPoint;
typedef struct { int16_t w, h; } GSize;
typedef struct { GPoint origin; GSize size; } GRect;
typedef union { uint8_t argb; } GColor8;
typedef GColor8 GColor;
typedef struct { int16_t top, right, bottom, left; } GEdgeInsets;
#define GPoint(x, y) ((GPoint){(x), (y)})
#define GRect(x, y, w, h) ((GRect){{(x), (y)}, {(w), (h)}})
#define GEdgeInsets1(v) ((GEdgeInsets){(v), (v), (v), (v)})
#define GEdgeInsets(...) GEdgeInsets1(__VA_ARGS__)
#define GColorFromHEX(h) ((GColor8){.argb = (uint8_t)(0xC0 | (((h) >> 18) & 0x30) | (((h) >> 12) & 0x0C) | (((h) >> 6) & 0x03))})
#define GColorBlack GColorFromHEX(0x000000)
#define GColorWhite GColorFromHEX(0xFFFFFF)
#define GColorDarkGray GColorFromHEX(0x555555)
#define GColorLightGray GColorFromHEX(0xAAAAAA)
#define GColorCobaltBlue GColorFromHEX(0x0055AA)
#define GColorChromeYellow GColorFromHEX(0xFFAA00)
#define GColorImperialPurple GColorFromHEX(0x550055)
#define GColorJaegerGreen GColorFromHEX(0x00AA55)
#define GColorShockingPink GColorFromHEX(0xFF55FF)
#define GColorPastelYellow GColorFromHEX(0xFFFFAA)
#define GColorSunsetOrange GColorFromHEX(0xFF5555)
#define GColorFolly GColorFromHEX(0xFF0055)
#define GColorDarkCandyAppleRed GColorFromHEX(0xAA0000)
#define GColorOxfordBlue GColorFromHEX(0x000055)
#define GColorDukeBlue GColorFromHEX(0x0000AA)
#define GColorCyan GColorFromHEX(0x00FFFF)
#define GColorGreen GColorFromHEX(0x00FF00)
#define GColorMalachite GColorFromHEX(0x00FF55)
#define GColorBlue GColorFromHEX(0x0000FF)
#define GColorMelon GColorFromHEX(0xFFAAAA)

typedef enum { GCornerNone = 0, GCornersAll = 15 } GCornerMask;
typedef enum { GTextOverflowModeWordWrap, GTextOverflowModeTrailingEllipsis, GTextOverflowModeFill } GTextOverflowMode;
typedef enum { GTextAlignmentLeft, GTextAlignmentCenter, GTextAlignmentRight } GTextAlignment;
typedef struct GContext GContext;
typedef struct FontInfo *GFont;
typedef struct GTextAttributes GTextAttributes;
typedef struct Layer Layer;
typedef struct Window Window;
typedef void *ClickRecognizerRef;
typedef void (*ClickHandler)(ClickRecognizerRef recognizer, void *context);
typedef void (*ClickConfigProvider)(void *context);
typedef void (*LayerUpdateProc)(Layer *layer, GContext *ctx);
typedef void (*WindowHandler)(Window *window);
typedef struct { WindowHandler load, appear, disappear, unload; } WindowHandlers;
typedef enum { BUTTON_ID_BACK, BUTTON_ID_UP, BUTTON_ID_SELECT, BUTTON_ID_DOWN } ButtonId;
typedef struct AppTimer AppTimer;
typedef void (*AppTimerCallback)(void *data);
typedef enum { TouchEvent_Touchdown, TouchEvent_Liftoff, TouchEvent_PositionUpdate } TouchEventType;
typedef struct { TouchEventType type; int16_t x; int16_t y; bool non_navigational; } TouchEvent;
typedef void (*TouchServiceHandler)(const TouchEvent *event, void *context);
typedef int status_t;

#define FONT_KEY_GOTHIC_14_BOLD "RESOURCE_ID_GOTHIC_14_BOLD"
#define FONT_KEY_GOTHIC_18 "RESOURCE_ID_GOTHIC_18"
#define FONT_KEY_GOTHIC_18_BOLD "RESOURCE_ID_GOTHIC_18_BOLD"
#define FONT_KEY_GOTHIC_24_BOLD "RESOURCE_ID_GOTHIC_24_BOLD"
#define FONT_KEY_GOTHIC_28_BOLD "RESOURCE_ID_GOTHIC_28_BOLD"

GFont fonts_get_system_font(const char *font_key);
void graphics_context_set_fill_color(GContext *ctx, GColor color);
void graphics_context_set_stroke_color(GContext *ctx, GColor color);
void graphics_context_set_text_color(GContext *ctx, GColor color);
void graphics_context_set_stroke_width(GContext *ctx, uint8_t stroke_width);
void graphics_fill_rect(GContext *ctx, GRect rect, uint16_t corner_radius, GCornerMask corner_mask);
void graphics_draw_rect(GContext *ctx, GRect rect);
void graphics_draw_round_rect(GContext *ctx, GRect rect, uint16_t radius);
void graphics_draw_line(GContext *ctx, GPoint p0, GPoint p1);
void graphics_fill_circle(GContext *ctx, GPoint p, uint16_t radius);
void graphics_draw_circle(GContext *ctx, GPoint p, uint16_t radius);
void graphics_draw_text(GContext *ctx, const char *text, const GFont font, const GRect box,
                        const GTextOverflowMode overflow_mode, const GTextAlignment alignment,
                        GTextAttributes *text_attributes);
GSize graphics_text_layout_get_content_size(const char *text, const GFont font, const GRect box,
                                            const GTextOverflowMode overflow_mode,
                                            const GTextAlignment alignment);
GRect grect_inset(GRect rect, GEdgeInsets insets);

Layer *layer_create(GRect frame);
void layer_destroy(Layer *layer);
void layer_set_update_proc(Layer *layer, LayerUpdateProc update_proc);
void layer_add_child(Layer *parent, Layer *child);
void layer_mark_dirty(Layer *layer);
GRect layer_get_bounds(const Layer *layer);

Window *window_create(void);
void window_destroy(Window *window);
void window_set_background_color(Window *window, GColor background_color);
void window_set_click_config_provider(Window *window, ClickConfigProvider click_config_provider);
void window_set_window_handlers(Window *window, WindowHandlers handlers);
Layer *window_get_root_layer(const Window *window);
void window_stack_push(Window *window, bool animated);
Window *window_stack_pop(bool animated);
void window_single_click_subscribe(ButtonId button_id, ClickHandler handler);
void window_long_click_subscribe(ButtonId button_id, uint16_t delay_ms, ClickHandler down_handler, ClickHandler up_handler);

void app_timer_cancel(AppTimer *timer_handle);
AppTimer *app_timer_register(uint32_t timeout_ms, AppTimerCallback callback, void *callback_data);
void app_event_loop(void);

bool persist_exists(const uint32_t key);
int persist_get_size(const uint32_t key);
int32_t persist_read_int(const uint32_t key);
status_t persist_write_int(const uint32_t key, const int32_t value);
int persist_read_data(const uint32_t key, void *buffer, const size_t buffer_size);
int persist_write_data(const uint32_t key, const void *data, const size_t size);

void touch_service_subscribe(TouchServiceHandler handler, void *context);
void touch_service_unsubscribe(void);
bool touch_service_is_enabled(void);
void app_touch_navigation_enable(bool enable);

void *malloc(size_t size);
void *calloc(size_t n, size_t size);
void free(void *ptr);
typedef struct { const uint32_t *durations; uint32_t num_segments; } VibePattern;
void vibes_enqueue_custom_pattern(VibePattern pattern);
void light_enable(bool enable);
void light_enable_interaction(void);
char *strchr(const char *s, int c);
size_t strlen(const char *s);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
int snprintf(char *str, size_t size, const char *format, ...);

// AppMessage (settings from the phone). Real message keys come from package.json.
typedef union { int32_t int32; uint32_t uint32; } TupleValue;
typedef struct Tuple { uint32_t key; uint8_t type; uint16_t length; TupleValue value[1]; } Tuple;
typedef struct DictionaryIterator DictionaryIterator;
Tuple *dict_find(const DictionaryIterator *iter, const uint32_t key);
typedef void (*AppMessageInboxReceived)(DictionaryIterator *iterator, void *context);
AppMessageInboxReceived app_message_register_inbox_received(AppMessageInboxReceived cb);
int app_message_open(const uint32_t size_inbound, const uint32_t size_outbound);
// (MESSAGE_KEY_* are variables the build generates; the simulator defines them in sim.c.)

// Resources and logging (used by the overlay loader and test)
typedef void *ResHandle;
ResHandle resource_get_handle(uint32_t resource_id);
size_t resource_size(ResHandle h);
size_t resource_load_byte_range(ResHandle h, uint32_t start_offset, uint8_t *buffer, size_t num_bytes);
#define RESOURCE_ID_OVL_TEST 1
#define RESOURCE_ID_OVL_MATH 2
#define RESOURCE_ID_OVL_CAS 3
#define RESOURCE_ID_OVL_GRAPH 4
#define RESOURCE_ID_OVL_FMT 5
#define RESOURCE_ID_OVL_TOOLS 6
#define RESOURCE_ID_OVL_CPLX 7
#define RESOURCE_ID_OVL_CALCU 8
typedef enum { APP_LOG_LEVEL_ERROR = 1, APP_LOG_LEVEL_WARNING = 50, APP_LOG_LEVEL_INFO = 100, APP_LOG_LEVEL_DEBUG = 200 } AppLogLevel;
void app_log(uint8_t log_level, const char *src_filename, int src_line_number, const char *fmt, ...);
#define APP_LOG(level, fmt, ...) app_log(level, __FILE__, __LINE__, fmt, ## __VA_ARGS__)

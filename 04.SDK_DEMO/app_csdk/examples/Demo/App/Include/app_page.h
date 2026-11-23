/**********************************************************************************************************
 * File Name     : app_page.h
 * Description   : Page lifecycle and page stack
 *
 *   Unified page layout: header 60px + content area 710px (both attached under the SDK page body,
 *   the SDK has already deducted the 30px status bar and 54px bottom navigation bar from the page body).
 **********************************************************************************************************/

#ifndef __APP_PAGE_H__
#define __APP_PAGE_H__

#include "lvgl.h"
#include "app_event.h"

#define FONT_SIZE_NORMAL                                    22
#define FONT_SIZE_SMALL                                     18

#define APP_PAGE_MAX_DEPTH      6

typedef struct app_page app_page_t;

typedef void (*app_page_create_cb)(app_page_t *page);
typedef void (*app_page_destroy_cb)(app_page_t *page);
typedef bool (*app_page_event_cb)(app_page_t *page, const app_event_t *ev);

struct app_page
{
    const char *id;                 /* page identifier, for debugging only */
    const char *title;              /* header title text */
    lv_obj_t   *root;               /* page root object (created by page_mgr) */
    lv_obj_t   *header;             /* header (created by page_mgr) */
    lv_obj_t   *body;               /* content area (the page attaches its own widgets into it) */
    app_page_create_cb  on_create;  /* page widget creation */
    app_page_event_cb   on_event;   /* event handler, returns true if consumed */
    app_page_destroy_cb on_destroy; /* release page private resources */
    void *user_data;
};

void app_page_init(void);

/* Push and show; returns true on success. The top page is hidden instead of destroyed. */
bool app_page_push(app_page_t *page);

/* Pop; the root page cannot be popped (returns false). */
bool app_page_pop(void);

/* Pop all pages back to the root page */
void app_page_pop_to_root(void);

/* Clear the page stack (does not delete widgets). Used to rebuild after LvUilCls() has already cleared the page widgets */
void app_page_reset(void);

/* Destroy the entire page stack (including the root page) and recreate the root page in the current language.
 * Called after a language switch so that all already rendered texts immediately use the new language. */
void app_page_rebuild(void);

app_page_t *app_page_current(void);
uint8_t app_page_depth(void);

/* Dispatch the event to the top page; unconsumed key events are handled globally */
void app_page_dispatch(const app_event_t *ev);

#endif /* __APP_PAGE_H__ */

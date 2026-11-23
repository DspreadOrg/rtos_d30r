//-----------------------------------------------------------------------------
// @file    : i18n_fa.c
// @brief   : Persian / Farsi (fa) language resource
// @details : The English source text is the lookup key, see LibLanguage.h.
//            A missing entry falls back to the English key. Strings which are
//            identical in Persian (acronyms, numbers) are intentionally not
//            listed. LVGL must be built with LV_USE_ARABIC_PERSIAN_CHARS to
//            shape the Arabic glyphs correctly.
// @author  :
// @version :
// @date    :
// @history :
//-----------------------------------------------------------------------------
#include "LibLanguage.h"
#include "app_i18n.h"

// Persian needs an Arabic/Persian capable TTF (Latin fonts such as
// AlibabaSans cannot draw it). Deploy the file to LittleFS and keep the
// name in app_i18n.h in sync with the resource image.
#define LANG_FONT_FA             APP_FONT_FILE_PERSIAN

//-----------------------------------------------------------------------------
// Local variable
//-----------------------------------------------------------------------------
static const LIB_LANG_ENTRY_T s_faEntries[] =
{
    // Page title
    { "MAIN MENU",                  "منوی اصلی" },
    { "SETTINGS",                   "تنظیمات" },
    { "TOOLS",                      "ابزارها" },
    { "POWER",                      "برق" },
    { "LANGUAGE",                   "زبان" },
    { "ABOUT",                      "درباره" },
    { "DEVICE INFO",                "اطلاعات دستگاه" },
    { "DATE & TIME",                "تاریخ و زمان" },
    { "NETWORK",                    "شبکه" },
    { "PRINT TEST",                 "تست چاپ" },
    { "SCAN",                       "اسکن" },
    { "TEST",                       "تست" },
    { "E-SIGNATURE",                "امضای الکترونیکی" },
    { "TMS UPDATE",                 "به‌روزرسانی TMS" },
    { "PAYMENT",                    "پرداخت" },
    { "MAINTENANCE",                "تعمیرات" },
    { "WIFI",                       "وای‌فای" },
    { "WiFi NETWORKS",              "شبکه‌های وای‌فای" },
    { "WiFi DETAILS",               "جزئیات وای‌فای" },
    { "EMV PAYMENT",                "پرداخت EMV" },

    // Home page
    { "Payment",                    "پرداخت" },
    { "Card Payment",               "پرداخت با کارت" },
    { "Print",                      "چاپ" },
    { "Receipt / Test",             "رسید / تست" },
    { "Network",                    "شبکه" },
    { "4G / WiFi Setup",            "تنظیم 4G / وای‌فای" },
    { "Remote Update",              "به‌روزرسانی از راه دور" },
    { "Settings",                   "تنظیمات" },
    { "Display / Time / Lang",      "نمایش / زمان / زبان" },
    { "Device",                     "دستگاه" },
    { "PN / SN / Ver",              "PN / SN / نسخه" },
    { "Test",                       "تست" },
    { "About",                      "درباره" },
    { "Version Info",               "اطلاعات نسخه" },
    { "More",                       "بیشتر" },
    { "Tools",                      "ابزارها" },
    { "Press 1-9 or tap a card to enter", "برای ورود ۱-۹ را فشار دهید یا کارت را لمس کنید" },

    // Menu pages
    { "AP List",                    "فهرست نقاط دسترسی" },
    { "Scan and connect",           "جستجو و اتصال" },
    { "Connection Details",         "جزئیات اتصال" },
    { "IP / mask / gateway",        "IP / ماسک / درگاه" },
    { "WiFi",                       "وای‌فای" },
    { "Wireless setup",             "تنظیم بی‌سیم" },
    { "Network Settings",           "تنظیمات شبکه" },
    { "4G / WiFi mode",             "حالت 4G / وای‌فای" },
    { "Tap an item, or use UP / DOWN + ENTER", "یک مورد را لمس کنید یا از بالا / پایین + ENTER استفاده کنید" },

    // Tools page
    { "Maintenance Mode",           "حالت تعمیرات" },
    { "Memory Monitor",             "پایش حافظه" },
    { "Firmware download",          "دانلود فریم‌ور" },
    { "Toggle RAM overlay",         "تغییر نمایش RAM" },

    // Power page
    { "Power Off",                  "خاموش کردن" },
    { "Restart",                    "راه‌اندازی مجدد" },
    { "Cancel",                     "لغو" },
    { "Use UP / DOWN and ENTER, or tap an item", "از بالا / پایین و ENTER استفاده کنید یا موردی را لمس کنید" },

    // Language page
    { "Selected",                   "انتخاب‌شده" },
    { "Saved. Restart the device to apply.", "ذخیره شد. برای اعمال، دستگاه را دوباره راه‌اندازی کنید." },
    { "Tap a language to select",   "برای انتخاب، یک زبان را لمس کنید" },
    { "Failed to switch language",  "تغییر زبان ناموفق بود" },
    { "Input",                      "ورودی" },

    // Settings page
    { "Brightness",                 "روشنایی" },
    { "Buzzer",                     "بوق" },
    { "Sleep Time",                 "زمان خواب" },
    { "Date & Time",                "تاریخ و زمان" },
    { "Language",                   "زبان" },
    { "ON",                         "روشن" },
    { "OFF",                        "خاموش" },

    // About page
    { "OS",                         "سیستم‌عامل" },
    { "Application",                "برنامه" },
    { "Security",                   "امنیت" },
    { "WiFi Module",                "ماژول وای‌فای" },
    { "N/A",                        "نامشخص" },

    // Device info page
    { "Model",                      "مدل" },
    { "Serial No",                  "شماره سریال" },
    { "Battery",                    "باتری" },
    { "Voltage",                    "ولتاژ" },
    { "Storage",                    "حافظه" },
    { "Charging",                   "در حال شارژ" },
    { "Tamper",                     "دستبرد" },
    { "Normal",                     "نرمال" },
    { "Triggered",                  "رخ داده" },

    // Tamper alert page
    { "TAMPER ALERT",               "هشدار دستبرد" },
    { "Tamper detected!",           "دستبرد شناسایی شد!" },
    { "Tamper Code",                "کد دستبرد" },
    { "Check the device enclosure for tampering.",
      "بدنه دستگاه را از نظر دستبرد بررسی کنید." },
    { "Press OK or BACK to dismiss", "برای بستن، OK یا BACK را فشار دهید" },
    { "OK",                         "تأیید" },
    { "Unlock",                     "قفل گشایی" },

    // Date time page
    { "Year",                       "سال" },
    { "Month",                      "ماه" },
    { "Day",                        "روز" },
    { "Hour",                       "ساعت" },
    { "Minute",                     "دقیقه" },
    { "Second",                     "ثانیه" },

    // Network page
    { "Status",                     "وضعیت" },
    { "IP Address",                 "نشانی IP" },
    { "MAC Address",                "نشانی MAC" },
    { "No SIM",                     "بدون سیم‌کارت" },
    { "No service",                 "بدون سرویس" },
    { "Connected",                  "متصل" },
    { "Connecting",                 "در حال اتصال" },
    { "Disconnected",               "قطع شده" },
    { "Unknown",                    "نامشخص" },
    { "Switching...",               "در حال تغییر..." },
    { "Set mode failed",            "تنظیم حالت ناموفق" },
    { "Reconnecting...",            "اتصال مجدد..." },
    { "Reconnect WiFi",             "اتصال مجدد وای‌فای" },
    { "WiFi AP List",               "فهرست نقاط دسترسی وای‌فای" },
    { "WiFi Details",               "جزئیات وای‌فای" },
    { "Tap 4G / WiFi to switch. Enter applies the focused item.",
      "برای تغییر 4G / وای‌فای را لمس کنید. ENTER مورد انتخاب‌شده را اعمال می‌کند." },

    // WiFi AP page
    { "Scanning...",                "در حال جستجو..." },
    { "Connecting to %.20s...",     "در حال اتصال به %.20s..." },
    { "Password: %.24s",            "رمز عبور: %.24s" },
    { "Connect failed",             "اتصال ناموفق" },
    { "Network forgotten",          "شبکه فراموش شد" },
    { "Cancelled",                  "لغو شد" },
    { "CONNECTED",                  "متصل" },
    { "No AP found",                "هیچ نقطه دسترسی یافت نشد" },
    { "Rescan",                     "جستجوی مجدد" },
    { "Forget",                     "فراموش کردن" },

    // WiFi info page
    { "Subnet Mask",                "ماسک زیرشبکه" },
    { "Gateway",                    "درگاه" },

    // Print page
    { "Printer",                    "چاپگر" },
    { "Result",                     "نتیجه" },
    { "Print Grade",                "کیفیت چاپ" },
    { "Print Test Page",            "چاپ صفحه تست" },
    { "Make sure paper is loaded before printing.", "قبل از چاپ از قرار گرفتن کاغذ مطمئن شوید." },
    { "Idle",                       "آماده" },
    { "Printing...",                "در حال چاپ..." },
    { "Completed",                  "تکمیل شد" },
    { "Out of paper",               "کاغذ تمام شده" },

    // TMS page
    { "TMS Status",                 "وضعیت TMS" },
    { "GPRS Module",                "ماژول GPRS" },
    { "Security Chip",              "تراشه امنیتی" },
    { "File System",                "سیستم فایل" },
    { "Firmware",                   "فریم‌ور" },
    { "Check for Update",           "بررسی به‌روزرسانی" },
    { "Server",                     "سرور" },
    { "Requires network. Keep the device powered on.",
      "به شبکه نیاز دارد. دستگاه را روشن نگه دارید." },
    { "Ready",                      "آماده" },
    { "Server host is empty",       "میزبان سرور خالی است" },
    { "Server saved",               "سرور ذخیره شد" },
    { "TMS Server (host:port)",     "سرور TMS (میزبان:درگاه)" },
    { "Checking...",                "در حال بررسی..." },

    // Scan page
    { "Camera open failed",         "باز کردن دوربین ناموفق" },
    { "Scanned",                    "اسکن شد" },
    { "Tap Rescan for another scan, bottom BACK to return",
      "برای اسکن دیگر «اسکن مجدد» را لمس کنید؛ BACK برای بازگشت" },

    // Test page
    { "MSR read failed",            "خواندن MSR ناموفق" },
    { "Multiple cards",             "چند کارت" },
    { "Reading...",                 "در حال خواندن..." },
    { "MSR open failed",            "باز کردن MSR ناموفق" },
    { "Swipe card...",              "کارت را بکشید..." },
    { "ICC open failed",            "باز کردن ICC ناموفق" },
    { "Insert IC card...",          "کارت IC را وارد کنید..." },
    { "PICC open failed",           "باز کردن PICC ناموفق" },
    { "Tap card...",                "کارت را لمس کنید..." },
    { "Select a card test",         "یک تست کارت انتخاب کنید" },
    { "1  Magnetic Stripe",         "۱  نوار مغناطیسی" },
    { "2  IC Card",                 "۲  کارت IC" },
    { "3  Contactless",             "۳  بدون تماس" },
    { "4  QR Scan",                 "۴  اسکن QR" },
    { "5  E-Signature",             "۵  امضا" },
    { "Stop",                       "توقف" },

    // Payment page
    { "Enter amount first",         "ابتدا مبلغ را وارد کنید" },
    { "Enter amount",               "مبلغ را وارد کنید" },
    { "Reading card...",            "در حال خواندن کارت..." },
    { "Multiple cards, remove one", "چند کارت؛ یکی را بردارید" },
    { "Card reader init failed",    "راه‌اندازی کارت‌خوان ناموفق" },
    { "Swipe / Insert / Tap card",  "کارت را بکشید / وارد کنید / لمس کنید" },
    { "CLR",                        "پاک" },
    { "DEL",                        "حذف" },
    { "PAY",                        "پرداخت" },
    { "Touch keypad to enter amount, PAY to start. BACK to exit",
      "با صفحه‌کلید مبلغ را وارد کنید؛ PAY برای شروع. BACK برای خروج." },

    // Signature page
    { "Sign in the box, then press Print", "در کادر امضا کنید، سپس Print را بزنید" },
    { "Please sign first",          "ابتدا امضا کنید" },
    { "Printed",                    "چاپ شد" },
    { "Sign here",                  "اینجا امضا کنید" },
    { "Clear",                      "پاک کردن" },

    // Maintenance page
    { "Maintenance mode is running...", "حالت تعمیرات در حال اجراست..." },
    { "Device is now in firmware download mode.",
      "دستگاه اکنون در حالت دانلود فریم‌ور است." },
    { "Start the download tool on PC.", "ابزار دانلود را روی رایانه شروع کنید." },
    { "Starting maintenance mode...", "در حال شروع حالت تعمیرات..." },
    { "Tap BACK to exit maintenance mode", "برای خروج از حالت تعمیرات BACK را لمس کنید" },

    // Placeholder page
    { "Under Construction",         "در حال ساخت" },
    { "Press BACK or tap the bottom bar to return",
      "برای بازگشت BACK را فشار دهید یا نوار پایین را لمس کنید" },

    // EMV page
    { "PIN  - - - -",               "PIN  - - - -" },
    { "Processing online...",       "در حال پردازش آنلاین..." },
    { "Processing...",              "در حال پردازش..." },
    { "OFFLINE PIN (last try), 4-12 digits", "PIN آفلاین (آخرین تلاش)، ۴ تا ۱۲ رقم" },
    { "OFFLINE PIN, 4-12 digits, empty OK = bypass",
      "PIN آفلاین، ۴ تا ۱۲ رقم، خالی = رد شدن" },
    { "MSR PIN, 4-12 digits",       "PIN MSR، ۴ تا ۱۲ رقم" },
    { "ONLINE PIN, 4-12 digits, empty OK = bypass",
      "PIN آنلاین، ۴ تا ۱۲ رقم، خالی = رد شدن" },
    { "Enter 4-12 digits or leave empty", "۴ تا ۱۲ رقم وارد کنید یا خالی بگذارید" },
    { "APPROVED",                   "تأیید شد" },
    { "DECLINED",                   "رد شد" },
    { "Print OK",                   "چاپ موفق" },
    { "Print failed",               "چاپ ناموفق" },
    { "4-12 digits",                "۴ تا ۱۲ رقم" },
    { "SELECT APPLICATION",         "برنامه را انتخاب کنید" },
    { "Confirm",                    "تأیید" },
    { "CARD",                       "کارت" },
    { "EXPIRY",                     "انقضا" },
    { "APPLICATION",                "برنامه" },
    { "EMV KERNEL",                 "هسته EMV" },
    { "Done",                       "پایان" },
    { "Start EMV failed",           "شروع EMV ناموفق" },

    // Status bar
    { "NO SIM",                     "بدون سیم‌کارت" },
    { "No Svc",                     "بدون سرویس" },
};

static const LIB_LANG_TABLE_T s_faTable =
{
    s_faEntries,
    (unsigned short)(sizeof(s_faEntries) / sizeof(s_faEntries[0])),
};

//-----------------------------------------------------------------------------
// Global variable
//-----------------------------------------------------------------------------
const LIB_LANG_DESC_T g_lang_fa =
{
    /* The name on the language page is drawn with the font of the "current language". In English mode the font is
       AlibabaSans (no Persian glyphs), so writing "فارسی" directly would show as blank/boxes,
       therefore the Latin name "Farsi" is used here; after switching to Persian the whole UI is
       rendered with farsi.ttf. */
    "Farsi",                    /* pName     : displayed in the language page */
    "fa",                       /* pCode     : must be unique */
    2,                          /* ucParam   : value written to E_LANG_TYPE */
    1,                          /* ucRtl     : 1 - right to left */
    LANG_FONT_FA,               /* pFontName : Arabic/Persian capable TTF */
    &s_faTable,                 /* pTable    : translation table */
};

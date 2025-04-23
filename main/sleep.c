/* 
 * Sleep management module for ESP32-CAM
 * Handles deep sleep configuration, wakeup sources, and sleep timing
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "sdkconfig.h"
#include "soc/soc_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/rtc_io.h"
#include "soc/rtc.h"
#include "sleep.h"
#include "config.h"
#include "utils.h"
#include "misc.h"
#include "wifi.h"
#include "cat1.h"
#include "camera.h"
#include "mqtt.h"
#include "pir.h"
#include "net_module.h"

#define TAG "-->SLEEP"  // Logging tag

#define SLEEP_WAIT_TIMEOUT_MS (30*60*1000) // 30 minute timeout
#define uS_TO_S_FACTOR 1000000ULL          // Microseconds to seconds conversion

/**
 * Sleep module state structure
 */
typedef struct mdSleep {
    EventGroupHandle_t eventGroup;  // Event group for sleep synchronization
} mdSleep_t;

// RTC memory preserved variables
static RTC_DATA_ATTR enum wakeupTodo g_wakeupTodo = 0;  // Action to perform after wakeup
static RTC_DATA_ATTR time_t g_lastCapTime = 0;          // Timestamp of last capture

static mdSleep_t g_sleep = {0};  // Global sleep state

/**
 * Find the most recent time interval for scheduled wakeups
 * @param timedCount Number of scheduled time nodes
 * @param timedNodes Array of scheduled time configurations
 * @return Seconds until next scheduled wakeup
 */
static uint32_t find_most_recent_time_interval(uint8_t timedCount, timedCapNode_t *timedNodes)
{
    int Hour, Minute, Second;
    struct tm timeinfo;
    uint8_t i = 0;
    time_t now;
    time_t tmp;
    time_t now2sunday;
    time_t intervalSeconds = 0;

    time(&now);
    localtime_r(&now, &timeinfo);
    // Calculate seconds since last Sunday 00:00:00
    now2sunday = ((timeinfo.tm_wday * 24 + timeinfo.tm_hour) * 60 + timeinfo.tm_min) * 60 + timeinfo.tm_sec;
    
    for (i = 0; i < timedCount; i++) {
        if (sscanf(timedNodes[i].time, "%02d:%02d:%02d", &Hour, &Minute, &Second) != 3) {
            ESP_LOGE(TAG, "invalid date %s", timedNodes[i].time);
            continue;
        }
        
        if (timedNodes[i].day < 7) { // Day of week specified
            tmp = ((timedNodes[i].day * 24 + Hour) * 60 + Minute) * 60 + Second;
            if (tmp < now2sunday) { // Time is in past, schedule for next week
                tmp += 7 * 24 * 60 * 60; // Add one week
            }
        } else { // Daily schedule
            tmp = ((timeinfo.tm_wday * 24 + Hour) * 60 + Minute) * 60 + Second;
            if (tmp < now2sunday) { // Time is in past, schedule for next day
                tmp += 1 * 24 * 60 * 60; // Add one day
            }
        }
        
        if (intervalSeconds == 0) {
            intervalSeconds = tmp - now2sunday;
        } else {
            intervalSeconds = MIN(intervalSeconds, (tmp - now2sunday)); // Find nearest wakeup time
        }
    }
    return timedCount ? MAX(intervalSeconds, 1) : 0; // Ensure minimum 1 second interval
}

/**
 * Calculate next wakeup time in seconds
 * @return Seconds until next wakeup
 */
static uint32_t calc_wakeup_time_seconds()
{
    capAttr_t capture;
    timedCapNode_t scheTimeNode;
    uint32_t sche_wakeup_sec = 0;
    uint32_t cfg_wakeup_sec = 0;
    time_t lastCapTime = sleep_get_last_capture_time();
    time_t now = time(NULL);

    scheTimeNode.day = 7;
    cfg_get_schedule_time(scheTimeNode.time);
    cfg_get_cap_attr(&capture);
    
    if (capture.bScheCap == 0) {
        // Schedule mode disabled - only check for scheduled tasks
        cfg_wakeup_sec = 0;
        sleep_set_wakeup_todo(WAKEUP_TODO_SCHEDULE);
        return find_most_recent_time_interval(1, &scheTimeNode);
    } else if (capture.scheCapMode == 1) {
        // Interval-based capture mode
        if (capture.intervalValue == 0) {
            cfg_wakeup_sec = 0; // No interval set
        } else {
            // Convert interval to seconds based on unit
            if (capture.intervalUnit == 0) { // Minutes
                cfg_wakeup_sec = capture.intervalValue * 60;
            } else if (capture.intervalUnit == 1) { // Hours
                cfg_wakeup_sec = capture.intervalValue * 60 * 60;
            } else if (capture.intervalUnit == 2) { // Days
                cfg_wakeup_sec = capture.intervalValue * 60 * 60 * 24;
            }

            // Handle missed captures
            if (lastCapTime) {
                if (now >= lastCapTime + cfg_wakeup_sec) {
                    cfg_wakeup_sec = 1; // Capture immediately if missed window
                } else {
                    cfg_wakeup_sec = lastCapTime + cfg_wakeup_sec - now; // Time until next capture
                }
            }
            
            // Force immediate capture if last snapshot failed
            if (camera_is_snapshot_fail()) {
                cfg_wakeup_sec = 1; 
            }
        }
    } else if (capture.scheCapMode == 0) {
        // Time-based capture mode
        cfg_wakeup_sec = find_most_recent_time_interval(capture.timedCount, capture.timedNodes);
    }

    // Determine whether to wake for snapshot or schedule
    sche_wakeup_sec = find_most_recent_time_interval(1, &scheTimeNode);
    if (cfg_wakeup_sec == 0 || sche_wakeup_sec < cfg_wakeup_sec) {
        sleep_set_wakeup_todo(WAKEUP_TODO_SCHEDULE);
        // Add random delay to prevent all devices waking simultaneously
        return sche_wakeup_sec + (rand() % 60); 
    } else {
        sleep_set_wakeup_todo(WAKEUP_TODO_SNAPSHOT);
        return cfg_wakeup_sec;
    }
}

/**
 * Enter deep sleep mode
 * Configures wakeup sources and enters low-power state
 */
void sleep_start(void)
{
    time_t now;
    time(&now);
    misc_show_time("now sleep at", now);
    
    // Calculate and set timer wakeup
    int wakeup_time_sec = calc_wakeup_time_seconds();
    if (wakeup_time_sec > 0) {
        esp_sleep_enable_timer_wakeup(wakeup_time_sec * uS_TO_S_FACTOR);
        misc_show_time("wake will at", now + wakeup_time_sec);
        ESP_LOGI(TAG, "Enabling TIMER wakeup on %ds", wakeup_time_sec);
    }

    // Configure button wakeup
    ESP_LOGI(TAG, "Enabling EXT0 wakeup on pin GPIO%d", BTN_WAKEUP_PIN);
    rtc_gpio_pullup_en(BTN_WAKEUP_PIN);
    rtc_gpio_pulldown_dis(BTN_WAKEUP_PIN);
    esp_sleep_enable_ext0_wakeup(BTN_WAKEUP_PIN, BTN_WAKEUP_LEVEL);
    // misc_io_cfg(BTN_WAKEUP_PIN, 1, 1);
    // esp_sleep_enable_ext0_wakeup(ALARMIN_WAKEUP_PIN, ALARMIN_WAKEUP_LEVEL);

#if PIR_ENABLE
    esp_sleep_enable_ext1_wakeup(BIT64(PIR_WAKEUP_PIN), PIR_IN_ACTIVE);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    rtc_gpio_pullup_dis(PIR_WAKEUP_PIN);
    rtc_gpio_pulldown_en(PIR_WAKEUP_PIN);
#else
    rtc_gpio_pullup_en(ALARMIN_WAKEUP_PIN);
    rtc_gpio_pulldown_dis(ALARMIN_WAKEUP_PIN);
    esp_sleep_enable_ext1_wakeup(BIT64(ALARMIN_WAKEUP_PIN), ALARMIN_WAKEUP_LEVEL);
#endif

    // Configure pullup/downs via RTCIO to tie wakeup pins to inactive level during deepsleep.
    // EXT0 resides in the same power domain (RTC_PERIPH) as the RTC IO pullup/downs.
    // No need to keep that power domain explicitly, unlike EXT1.
    // rtc_gpio_pullup_dis(BTN_WAKEUP_PIN);
    // rtc_gpio_pulldown_en(BTN_WAKEUP_PIN);
    /* If there are no external pull-up/downs, tie wakeup pins to inactive level with internal pull-up/downs via RTC IO
     * during deepsleep. However, RTC IO relies on the RTC_PERIPH power domain. Keeping this power domain on will
     * increase some power comsumption. */
    mqtt_stop();
    wifi_close();
    cat1_close();
#if PIR_ENABLE
    pir_init(1);
    if (sleep_is_pir_goto_restart()) {
#else
    if (sleep_is_alramin_goto_restart()) {
#endif
        ESP_LOGI(TAG, "Alarm in deep sleep, restart");
        esp_restart();
    } else {
        ESP_LOGI(TAG, "Entering deep sleep");
        esp_deep_sleep_start();
    }
}

/**
 * Determine wakeup source
 * @return Type of wakeup that occurred
 */
wakeupType_e sleep_wakeup_case()
{
    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_EXT0: {
            ESP_LOGI(TAG, "Wake up button");
            return WAKEUP_BUTTON;
        }
        case ESP_SLEEP_WAKEUP_EXT1: {
            uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
            ESP_LOGI(TAG, "Alarm in Wake up from GPIO %d", __builtin_ffsll(wakeup_pin_mask) - 1);
            return WAKEUP_ALARMIN;
        }
        case ESP_SLEEP_WAKEUP_TIMER: {
            ESP_LOGI(TAG, "Wake up from timer");
            return WAKEUP_TIMER;
        }
        case ESP_SLEEP_WAKEUP_GPIO: {
            ESP_LOGI(TAG, "Wake up from GPIO");
            return WAKEUP_UNDEFINED;
        }
        case ESP_SLEEP_WAKEUP_UNDEFINED: {
            ESP_LOGI(TAG, "Wake up from UNDEFINED");
            return WAKEUP_UNDEFINED;
        }
        default: {
            ESP_LOGI(TAG, "Not a deep sleep reset");
            return WAKEUP_UNDEFINED;
        }
    }
}
/**
 * Initialize sleep module
 */
void sleep_open()
{
    memset(&g_sleep, 0, sizeof(g_sleep));
    g_sleep.eventGroup = xEventGroupCreate();
}

/**
 * Wait for specified event bits before sleeping
 * @param bits Event bits to wait for
 * @param bWaitAll True to wait for all bits, false for any bit
 */
void sleep_wait_event_bits(sleepBits_e bits, bool bWaitAll)
{
    ESP_LOGI(TAG, "WAIT for event bits to sleep ... ");
    EventBits_t uxBits = xEventGroupWaitBits(g_sleep.eventGroup, bits, \
                                             true, bWaitAll, \
                                             pdMS_TO_TICKS(SLEEP_WAIT_TIMEOUT_MS));
    ESP_LOGI(TAG, "sleep right now, bits=%lu", uxBits);
    sleep_start();
}

/**
 * Set sleep event bits
 * @param bits Event bits to set
 */
void sleep_set_event_bits(sleepBits_e bits)
{
    xEventGroupSetBits(g_sleep.eventGroup, bits);
}

/**
 * Clear sleep event bits
 * @param bits Event bits to clear
 */
void sleep_clear_event_bits(sleepBits_e bits)
{
    xEventGroupClearBits(g_sleep.eventGroup, bits);
}

/**
 * Get action to perform after wakeup
 * @return Scheduled wakeup action
 */
wakeupTodo_e sleep_get_wakeup_todo()
{
    ESP_LOGI(TAG, "sleep_get_wakeup_todo %d", g_wakeupTodo);
    return g_wakeupTodo;
}

/**
 * Set action to perform after wakeup
 * @param todo Action to perform
 */
void sleep_set_wakeup_todo(wakeupTodo_e todo)
{
    ESP_LOGI(TAG, "sleep_set_wakeup_todo %d", todo);
    g_wakeupTodo = todo;
}

/**
 * Set timestamp of last capture
 * @param time Timestamp to store
 */
void sleep_set_last_capture_time(time_t time)
{
    g_lastCapTime = time;
}

/**
 * Get timestamp of last capture
 * @return Last capture timestamp
 */
time_t sleep_get_last_capture_time(void)
{
    return g_lastCapTime;
}

/**
 * Check if alarm input should trigger restart
 * @return 1 if should restart, 0 otherwise
 */
uint32_t sleep_is_alramin_goto_restart()
{
    return rtc_gpio_get_level(ALARMIN_WAKEUP_PIN) == ALARMIN_WAKEUP_LEVEL;
}

uint32_t sleep_is_pir_goto_restart()
{
    return rtc_gpio_get_level(PIR_WAKEUP_PIN) == PIR_WAKEUP_LEVEL;
}

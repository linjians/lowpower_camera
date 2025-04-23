#ifndef __CAMERA_H__
#define __CAMERA_H__

#include "system.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Camera event flags for synchronization
 */
typedef enum cameraEvent {
    CAMERA_START_BIT = BIT(0),  ///< Camera started event flag
    CAMERA_STOP_BIT = BIT(1),   ///< Camera stopped event flag
} cameraEvent_e;

/**
 * Initialize camera module
 * @param in Input queue handle (can be NULL)
 * @param out Output queue handle (can be NULL)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t camera_open(QueueHandle_t in, QueueHandle_t out);

/**
 * Deinitialize camera module
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t camera_close();

/**
 * Start camera capture
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t camera_start();

/**
 * Stop camera capture
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t camera_stop();

/**
 * Wait for camera event
 * @param event Event to wait for (CAMERA_START_BIT or CAMERA_STOP_BIT)
 * @param timeout_ms Maximum time to wait in milliseconds
 */
void camera_wait(cameraEvent_e event, uint32_t timeout_ms);

/**
 * Take snapshot with specified type and retry count
 * @param type Snapshot type (from snapType_e)
 * @param count Number of retry attempts
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t camera_snapshot(snapType_e type, uint8_t count);

/**
 * Configure image capture settings
 * @param image Image attributes to apply
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t camera_set_image(imgAttr_t *image);

/**
 * Control camera flash LED
 * @param light LED attributes to apply
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t camera_flash_led_ctrl(lightAttr_t *light);

/**
 * Check if last snapshot failed
 * @return true if snapshot failed, false otherwise
 */
bool camera_is_snapshot_fail();

#ifdef __cplusplus
}
#endif

#endif /* __CAMERA_H__ */

/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_modem_config.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_modem_dce_wrap esp_modem_dce_t;

typedef struct esp_modem_PdpContext_t {
    size_t context_id;
    const char *protocol_type;
    const char *apn;
} esp_modem_PdpContext_t;

/**
 * @defgroup ESP_MODEM_C_API ESP_MODEM C API
 * @brief Set of basic C API for ESP-MODEM
 */
/** @addtogroup ESP_MODEM_C_API
 * @{
 */

/**
 * @brief DCE mode: This enum is used to set desired operation mode of the DCE
 */
typedef enum esp_modem_dce_mode {
    ESP_MODEM_MODE_COMMAND,  /**< Default mode after modem startup, used for sending AT commands */
    ESP_MODEM_MODE_DATA,     /**< Used for switching to PPP mode for the modem to connect to a network */
    ESP_MODEM_MODE_CMUX,     /**< Multiplexed terminal mode */
    ESP_MODEM_MODE_CMUX_MANUAL,         /**< CMUX manual mode */
    ESP_MODEM_MODE_CMUX_MANUAL_EXIT,    /**< Exit CMUX manual mode */
    ESP_MODEM_MODE_CMUX_MANUAL_SWAP,    /**< Swap terminals in CMUX manual mode */
    ESP_MODEM_MODE_CMUX_MANUAL_DATA,    /**< Set DATA mode in CMUX manual mode */
    ESP_MODEM_MODE_CMUX_MANUAL_COMMAND, /**< Set COMMAND mode in CMUX manual mode */
} esp_modem_dce_mode_t;

/**
 * @brief DCE devices: Enum list of supported devices
 */
typedef enum esp_modem_dce_device {
    ESP_MODEM_DCE_GENETIC,  /**< The most generic device */
    ESP_MODEM_DCE_SIM7600,
    ESP_MODEM_DCE_SIM7070,
    ESP_MODEM_DCE_SIM7000,
    ESP_MODEM_DCE_BG96,
    ESP_MODEM_DCE_SIM800,
    ESP_MODEM_DCE_EC800E,
} esp_modem_dce_device_t;

/**
 * @brief Terminal errors
 */
typedef enum esp_modem_terminal_error {
    ESP_MODEM_TERMINAL_BUFFER_OVERFLOW,
    ESP_MODEM_TERMINAL_CHECKSUM_ERROR,
    ESP_MODEM_TERMINAL_UNEXPECTED_CONTROL_FLOW,
    ESP_MODEM_TERMINAL_DEVICE_GONE,
    ESP_MODEM_TERMINAL_UNKNOWN_ERROR,
} esp_modem_terminal_error_t;

/**
 * @brief Terminal error callback
 */
typedef void (*esp_modem_terminal_error_cbt)(esp_modem_terminal_error_t);

/**
 * @brief Create a generic DCE handle for new modem API
 *
 * @param dte_config DTE configuration (UART config for now)
 * @param dce_config DCE configuration
 * @param netif Network interface handle for the data mode
 *
 * @return DCE pointer on success, NULL on failure
 */
esp_modem_dce_t *esp_modem_new(const esp_modem_dte_config_t *dte_config, const esp_modem_dce_config_t *dce_config, esp_netif_t *netif);

/**
 * @brief Create a DCE handle using the supplied device
 *
 * @param module Specific device for creating this DCE
 * @param dte_config DTE configuration (UART config for now)
 * @param dce_config DCE configuration
 * @param netif Network interface handle for the data mode
 *
 * @return DCE pointer on success, NULL on failure
 */
esp_modem_dce_t *esp_modem_new_dev(esp_modem_dce_device_t module, const esp_modem_dte_config_t *dte_config, const esp_modem_dce_config_t *dce_config, esp_netif_t *netif);

/**
 * @brief Destroys modem's DCE handle
 *
 * @param dce DCE to destroy
 */
void esp_modem_destroy(esp_modem_dce_t *dce);

/**
 * @brief Set DTE's error callback
 *
 * @param dce Modem DCE handle
 * @param[in] err_cb Error callback
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t esp_modem_set_error_cb(esp_modem_dce_t *dce, esp_modem_terminal_error_cbt err_cb);

/**
 * @brief Set operation mode for this DCE
 * @param dce Modem DCE handle
 * @param mode Desired MODE
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t esp_modem_set_mode(esp_modem_dce_t *dce, esp_modem_dce_mode_t mode);

esp_err_t esp_modem_command(esp_modem_dce_t *dce, const char *command, esp_err_t(*got_line_cb)(uint8_t *data, size_t len), uint32_t timeout_ms);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

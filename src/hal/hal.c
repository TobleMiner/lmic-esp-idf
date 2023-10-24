/*******************************************************************************
 * Copyright (c) 2015 Matthijs Kooijman
 * Copyright (c) 2019 Tobias Schramm
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Thi is the HAL to run LMIC on top of the ESP-IDF environment
 *******************************************************************************/

#include "hal.h"
#include "../include/lmic.h"

#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "lmic"

// Declared here, to be defined an initialized by the application
extern const lmic_pinmap lmic_pins;

// -----------------------------------------------------------------------------
// I/O

static void hal_io_init() {
    int i;
    // ESP_LOGI(TAG, "Starting IO initialization");

    if (lmic_pins.rst != LMIC_UNUSED_PIN) {
        gpio_set_direction(lmic_pins.rst, GPIO_MODE_OUTPUT);
        gpio_set_intr_type(lmic_pins.rst, GPIO_INTR_DISABLE);
    }

    for (i = 0; i < NUM_DIO; i++) {
        if (lmic_pins.dio[i] != LMIC_UNUSED_PIN) {
            gpio_set_direction(lmic_pins.dio[i], GPIO_MODE_INPUT);
            gpio_set_intr_type(lmic_pins.dio[i], GPIO_INTR_DISABLE);
        }
    }

    gpio_set_direction(lmic_pins.nss, GPIO_MODE_OUTPUT);
    gpio_set_intr_type(lmic_pins.nss, GPIO_INTR_DISABLE);

    // ESP_LOGI(TAG, "Finished IO initialization");
}

// val == 1  => tx 1
void hal_pin_rxtx(u1_t val) {
    if (lmic_pins.rxtx != LMIC_UNUSED_PIN)
        gpio_set_level(lmic_pins.rxtx, val);
}

// set radio NSS pin to given value
void hal_pin_nss(u1_t val) { gpio_set_level(lmic_pins.nss, val); }

// set radio RST pin to given value (or keep floating!)
void hal_pin_rst(u1_t val) {
    if (lmic_pins.rst == LMIC_UNUSED_PIN)
        return;

    if (val == 0 || val == 1) { // drive pin
        gpio_set_direction(lmic_pins.rst, GPIO_MODE_OUTPUT);
        gpio_set_level(lmic_pins.rst, val);
    } else { // keep pin floating
        gpio_set_direction(lmic_pins.rst, GPIO_MODE_INPUT);
    }
}

static bool dio_states[NUM_DIO] = {0};

static void hal_io_check() {
    uint8_t i;
    for (i = 0; i < NUM_DIO; ++i) {
        if (lmic_pins.dio[i] == LMIC_UNUSED_PIN)
            continue;

        if (dio_states[i] != gpio_get_level(lmic_pins.dio[i])) {
            dio_states[i] = !dio_states[i];
            if (dio_states[i])
                radio_irq_handler(i);
        }
    }
}

// -----------------------------------------------------------------------------
// SPI

spi_device_handle_t spi_handle;

static void hal_spi_init() {
    // ESP_LOGI(TAG, "Starting SPI initialization");
    esp_err_t ret;

    // init master
    spi_bus_config_t buscfg = {
        .miso_io_num = lmic_pins.spi[0],
        .mosi_io_num = lmic_pins.spi[1],
        .sclk_io_num = lmic_pins.spi[2],
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    // init device
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 100000,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 7,
    };

    ret = spi_bus_initialize(LMIC_SPI, &buscfg, 1);
    assert(ret == ESP_OK);

    ret = spi_bus_add_device(LMIC_SPI, &devcfg, &spi_handle);
    assert(ret == ESP_OK);

    // ESP_LOGI(TAG, "Finished SPI initialization");
}

// perform SPI transaction with radio
u1_t hal_spi(u1_t data) {
    uint8_t rxData = 0;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.rxlength = 8;
    t.tx_buffer = &data;
    t.rx_buffer = &rxData;
    esp_err_t ret = spi_device_transmit(spi_handle, &t);
    assert(ret == ESP_OK);

    return (u1_t)rxData;
}

static void hal_spi_check_irq() {
    hal_pin_nss(0);
    hal_spi(0x12); // RegIrqFlags
    u1_t val = hal_spi(0x00);
    hal_pin_nss(1);
    if (val != 0) {
        radio_irq_handler(0);
    }
}

// -----------------------------------------------------------------------------
// TIME

gptimer_handle_t gptimer;

static void hal_time_init() {
    // ESP_LOGI(TAG, "Starting initialisation of timer");

    gptimer_config_t timer_config = {.clk_src = GPTIMER_CLK_SRC_DEFAULT,
                                     .direction = GPTIMER_COUNT_UP,
                                     .resolution_hz = OSTICKS_PER_SEC};
    esp_err_t ret = gptimer_new_timer(&timer_config, &gptimer);
    assert(ret == ESP_OK);
    ret = gptimer_enable(gptimer);
    assert(ret == ESP_OK);
    ret = gptimer_start(gptimer);
    assert(ret == ESP_OK);

    // ESP_LOGI(TAG, "Finished initalisation of timer");
}

u4_t hal_ticks() {
    uint64_t val;
    gptimer_get_raw_count(gptimer, &val);
    return (u4_t)val;
}

// Returns the number of ticks until time. Negative values indicate that
// time has already passed.
static s4_t delta_time(u4_t time) { return (s4_t)(time - hal_ticks()); }

void hal_waitUntil(u4_t time) {

    // ESP_LOGI(TAG, "Wait until");
    s4_t delta = delta_time(time);

    while (delta > 2000) {
        vTaskDelay(1 / portTICK_PERIOD_MS);
        delta -= 1000;
    }
    if (delta > 0) {
        vTaskDelay(delta / portTICK_PERIOD_MS);
    }
    // ESP_LOGI(TAG, "Done waiting until");
}

// check and rewind for target time
u1_t hal_checkTimer(u4_t time) { return delta_time(time) <= 0; }

// -----------------------------------------------------------------------------
// IRQ

int x_irq_level = 0;

void hal_disableIRQs() {
    // ESP_LOGD(TAG, "Disabling interrupts");
    if (x_irq_level < 1) {
        // taskDISABLE_INTERRUPTS();
    }
    x_irq_level++;
}

void hal_enableIRQs() {
    // ESP_LOGD(TAG, "Enable interrupts");
    if (--x_irq_level == 0) {
        // taskENABLE_INTERRUPTS();
        if (lmic_pins.dio[0] != LMIC_UNUSED_PIN) {
            hal_io_check();
        } else {
            hal_spi_check_irq();
        }
    }
}

void hal_sleep() {
    // unused
}

// -----------------------------------------------------------------------------

#if defined(LMIC_PRINTF_TO)
static int uart_putchar(char c, FILE *) {
    putc(c);
    //	printf("%c", c);
    return 0;
}

void hal_printf_init() {
    // nop, init done
}
#endif // defined(LMIC_PRINTF_TO)

void lmichal_init() {
    // configure radio I/O and interrupt handler
    hal_io_init();
    // configure radio SPI
    hal_spi_init();
    // configure timer and interrupt handler
    hal_time_init();
}

void hal_failed(const char *file, u2_t line) {
    // HALT...
    ESP_LOGE(TAG, "LMIC HAL failed (%s:%u)", file, line);
    hal_disableIRQs();
    hal_sleep();
    while (1)
        ;
}
/*******************************************************************************
 * Copyright (c) 2015 Matthijs Kooijman
 * Copyright (c) 2019 Tobias Schramm
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * This is the HAL to run LMIC on top of the esp-idf environment
 *******************************************************************************/
#ifndef _hal_hal_h_
#define _hal_hal_h_

#define NUM_DIO 3

struct lmic_pinmap {
    u1_t nss;
    u1_t rxtx;
    u1_t rst;
    u1_t dio[NUM_DIO];
    u1_t spi[3]; // MISO, MOSI, SCK
};

typedef struct lmic_pinmap lmic_pinmap;

// Use this for any unused pins.
#define LMIC_UNUSED_PIN 0xFF

#endif // _hal_hal_h_

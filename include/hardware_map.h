#pragma once

#include <Arduino.h>

// Current active hardware mapping.
// These values must stay aligned with the existing wiring so the firmware
// continues to behave exactly as before.
static constexpr uint8_t TOUCH_IRQ_PIN = 2;
static constexpr uint8_t TOUCH_CS_PIN = 10;

static constexpr uint8_t TFT_DC_PIN = 20;
static constexpr uint8_t TFT_CS_PIN = 21;
static constexpr uint8_t TFT_RST_PIN = 255; // Unused, tied to 3.3V
static constexpr uint8_t TFT_MOSI_PIN = 7;
static constexpr uint8_t TFT_SCLK_PIN = 14;
static constexpr uint8_t TFT_MISO_PIN = 12;

static constexpr uint8_t GATE_OUT1_PIN = 3;
static constexpr uint8_t GATE_OUT2_PIN = 4;
static constexpr uint8_t GATE_OUT3_PIN = 5;

static constexpr uint8_t VALUE_OUT1_PIN = A21;
static constexpr uint8_t VALUE_OUT2_PIN = A22;
static constexpr uint8_t VALUE_OUT3_PWM_PIN = 29;

// Reserved future expansion mapping.
// Declared now so the project has one stable pin plan, but these are not
// enabled by the current firmware yet.
static constexpr uint8_t MCP4822_CS_PIN = 9;
static constexpr uint8_t MCP4822_LDAC_PIN = 8;

static constexpr uint8_t CV_IN_1_PIN = A13;
static constexpr uint8_t CV_IN_2_PIN = A14;
static constexpr uint8_t CV_IN_3_PIN = A15;

static constexpr uint8_t ENC1_CLK_PIN = 15;
static constexpr uint8_t ENC1_DT_PIN = 16;
static constexpr uint8_t ENC1_SW_PIN = 24;

static constexpr uint8_t ENC2_CLK_PIN = 17;
static constexpr uint8_t ENC2_DT_PIN = 18;
static constexpr uint8_t ENC2_SW_PIN = 25;

static constexpr uint8_t ENC3_CLK_PIN = 22;
static constexpr uint8_t ENC3_DT_PIN = 23;
static constexpr uint8_t ENC3_SW_PIN = 26;

static constexpr uint8_t CLOCK_IN_PIN = 6;
// Reuses the current PWM pin once Val3 moves to the external MCP4822.
static constexpr uint8_t RESET_IN_PIN = 29;

#pragma once

#include <Arduino.h>

// Conflict-free target mapping for Teensy 4.1.
// SPI0: Touch + both MCP4822 (MOSI=11, MISO=12, SCK=13).
// SPI1: TFT only (MOSI=26, MISO=1, SCK=27).

static constexpr uint8_t TOUCH_IRQ_PIN = 2;
static constexpr uint8_t TOUCH_CS_PIN = 10;  // SPI0 CS

static constexpr uint8_t TFT_DC_PIN = 20;
static constexpr uint8_t TFT_CS_PIN = 21;    // SPI1 CS (variable)
static constexpr uint8_t TFT_RST_PIN = 255; // Unused, tied to 3.3V
static constexpr uint8_t TFT_MOSI_PIN = 26; // SPI1 MOSI
static constexpr uint8_t TFT_SCLK_PIN = 27; // SPI1 SCK
static constexpr uint8_t TFT_MISO_PIN = 1;  // SPI1 MISO

static constexpr uint8_t GATE_OUT1_PIN = 3;
static constexpr uint8_t GATE_OUT2_PIN = 4;
static constexpr uint8_t GATE_OUT3_PIN = 5;

// MCP4822 DACs (SPI0) – CV outputs via external DAC, keine MCU-PWM-Pins
static constexpr uint8_t MCP4822_CS_VALUE_PIN = 9;   // CS for Value DAC
static constexpr uint8_t MCP4822_CS_PITCH_PIN = 8;   // CS for Pitch DAC

static constexpr uint8_t CV_IN_1_PIN = 38; // A14 on 4.1
static constexpr uint8_t CV_IN_2_PIN = 39; // A15 on 4.1
static constexpr uint8_t CV_IN_3_PIN = 40; // A16 on 4.1

static constexpr uint8_t ENC1_CLK_PIN = 15;
static constexpr uint8_t ENC1_DT_PIN = 16;
static constexpr uint8_t ENC1_SW_PIN = 24;

static constexpr uint8_t ENC2_CLK_PIN = 17;
static constexpr uint8_t ENC2_DT_PIN = 18;
static constexpr uint8_t ENC2_SW_PIN = 25;

static constexpr uint8_t ENC3_CLK_PIN = 22;
static constexpr uint8_t ENC3_DT_PIN = 23;
static constexpr uint8_t ENC3_SW_PIN = 32;

static constexpr uint8_t CLOCK_IN_PIN = 6;
static constexpr uint8_t RESET_IN_PIN = 7;

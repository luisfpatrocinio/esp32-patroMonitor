#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// --- Pin definitions for ESP32 ---
#define TFT_DC 2
#define TFT_CS 5
#define TFT_MOSI 23 // Used by software SPI, but not passed to constructor
#define TFT_CLK 18  // Used by software SPI, but not passed to constructor
#define TFT_RST 4
#define TFT_MISO 19 // Generally not used for display output

// --- Color definitions ---
#define BG_COLOR 0x10A2     // A dark grey color
#define HEADER_COLOR 0x3186 // A darker blue
#define TEXT_COLOR ILI9341_WHITE
#define VALUE_COLOR ILI9341_YELLOW
#define BORDER_COLOR ILI9341_CYAN

// --- Function Prototypes ---
void drawStaticUI();
void updateTextValue(int x, int y, String oldValue, String newValue);

// Instantiate the display driver using software SPI.
// This constructor variant tells the library to handle SPI communication
// by manually toggling the pins, ignoring the hardware SPI peripheral.
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// Variables to store old values and avoid redrawing what hasn't changed
int oldCounter = -1;
uint8_t oldRed = 0;
uint8_t oldGreen = 0;
uint8_t oldBlue = 0;

void setup()
{
    Serial.begin(115200);

    // For software SPI, you might need to initialize the SPI pins manually
    // before tft.begin(). The library often handles this, but it's good practice.
    SPI.begin(TFT_CLK, TFT_MISO, TFT_MOSI, TFT_CS); // Optional, but can help

    tft.begin();
    tft.setRotation(1); // Horizontal rotation (320x240)

    // --- Draw the static UI ONCE ---
    drawStaticUI();
}

void loop()
{
    // --- Update logic ---
    static int counter = 0;
    counter++;

    // Calculate new colors
    uint8_t currentRed = 127 + sin(counter / 20.0) * 128;
    uint8_t currentGreen = 127 + cos(counter / 30.0) * 128;
    uint8_t currentBlue = 127 + sin(counter / 40.0) * 128;
    uint16_t newColor = tft.color565(currentRed, currentGreen, currentBlue);

    // --- Smart screen update ---
    // The magic happens here: we only redraw the values that actually changed.

    // Update the counter
    if (counter != oldCounter)
    {
        updateTextValue(100, 80, String(oldCounter), String(counter));
        oldCounter = counter;
    }

    // Update the Red value
    if (currentRed != oldRed)
    {
        updateTextValue(100, 110, String(oldRed), String(currentRed));
        oldRed = currentRed;
    }

    // Update the Green value
    if (currentGreen != oldGreen)
    {
        updateTextValue(100, 140, String(oldGreen), String(currentGreen));
        oldGreen = currentGreen;
    }

    // Update the Blue value
    if (currentBlue != oldBlue)
    {
        updateTextValue(100, 170, String(oldBlue), String(currentBlue));
        oldBlue = currentBlue;
    }

    // Update the color preview rectangle
    tft.fillRect(200, 80, 100, 120, newColor);
    tft.drawRect(200, 80, 100, 120, BORDER_COLOR); // Draw a border for emphasis

    delay(20); // Keep the animation smooth
}

/**
 * @brief Draws the static elements of the UI.
 * Called only once in setup() to prevent flickering.
 */
void drawStaticUI()
{
    tft.fillScreen(BG_COLOR);

    // Header
    tft.fillRect(0, 0, 320, 40, HEADER_COLOR);
    tft.setCursor(10, 10);
    tft.setTextColor(TEXT_COLOR);
    tft.setTextSize(3);
    tft.print("PatroMonitor");

    // Value labels
    tft.setTextSize(2);
    tft.setTextColor(TEXT_COLOR);

    tft.setCursor(10, 80);
    tft.print("Count:");

    tft.setCursor(10, 110);
    tft.print("Red:");

    tft.setCursor(10, 140);
    tft.print("Green:");

    tft.setCursor(10, 170);
    tft.print("Blue:");

    // Color preview label
    tft.setCursor(200, 60);
    tft.print("Preview");
}

/**
 * @brief Efficiently updates a text value on the screen.
 * First, it "erases" the old value by drawing it with the background color.
 * Then, it draws the new value with the highlight color.
 * @param x X position for the value.
 * @param y Y position for the value.
 * @param oldValue The previous value as a String.
 * @param newValue The new value as a String.
 */
void updateTextValue(int x, int y, String oldValue, String newValue)
{
    // Set text properties for value updates
    tft.setTextSize(2);

    // Erase the old value by printing it in the background color
    tft.setCursor(x, y);
    tft.setTextColor(BG_COLOR);
    tft.print(oldValue);

    // Print the new value in the designated value color
    tft.setCursor(x, y);
    tft.setTextColor(VALUE_COLOR);
    tft.print(newValue);
}

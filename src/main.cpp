#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// --- Wi-fi Setup
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer server(80);
const char *ap_ssid = "PatroMonitor";
const char *ap_password = "";

// Variáveis para armazenar valores antigos e evitar redesenho desnecessário
int oldCounter = -1;
uint8_t oldRed = 0;
uint8_t oldGreen = 0;
uint8_t oldBlue = 0;
int oldClients = -1; // Novo: armazena o número anterior de clientes conectados

// Variáveis globais para RGB controlado pelo usuário
uint8_t currentRed = 0;
uint8_t currentGreen = 0;
uint8_t currentBlue = 0;
bool rgbManualMode = false; // Flag: false = automático, true = manual

// --- Web Page
void handleRoot()
{
    // Gera o HTML com os valores atuais de RGB
    String html = "<!DOCTYPE html>\n";
    html += "<html>\n<head>\n<meta charset=\"UTF-8\">\n<title>PatroMonitor</title>\n";
    html += "<style>body { background: #222; color: #fff; font-family: Arial, sans-serif; text-align: center; margin: 0; padding: 0; }\n";
    html += ".header { background: #3186; padding: 20px 0; font-size: 2em; color: #fff; }\n";
    html += ".content { margin: 40px auto; max-width: 400px; background: #333; border-radius: 10px; padding: 30px; box-shadow: 0 0 10px #0008; }\n";
    html += ".value { color: #FFD600; font-size: 1.5em; margin: 20px 0; }\n";
    html += ".footer { margin-top: 40px; color: #aaa; font-size: 0.9em; }\n";
    html += ".slider { width: 80%; }\nlabel { display: block; margin-top: 20px; font-size: 1.1em; }\n.rgb-value { font-weight: bold; }\n";
    html += "button { margin-top: 30px; padding: 10px 30px; font-size: 1.1em; background: #FFD600; color: #222; border: none; border-radius: 5px; cursor: pointer; }\n";
    html += "button:hover { background: #FFC107; }\n</style>\n";
    html += "<script>function updateValue(id, value) { document.getElementById(id + '-val').innerText = value; }</script>\n";
    html += "</head>\n<body>\n";
    html += "<div class=\"header\">PatroMonitor</div>\n";
    html += "<div class=\"content\">\n<h2>Controle RGB</h2>\n";
    html += "<form action=\"/setrgb\" method=\"get\">\n";
    html += "<label for=\"red\">Red: <span class=\"rgb-value\" id=\"red-val\">" + String(currentRed) + "</span></label>\n";
    html += "<input type=\"range\" min=\"0\" max=\"255\" value=\"" + String(currentRed) + "\" class=\"slider\" id=\"red\" name=\"red\" oninput=\"updateValue('red', this.value)\">\n";
    html += "<label for=\"green\">Green: <span class=\"rgb-value\" id=\"green-val\">" + String(currentGreen) + "</span></label>\n";
    html += "<input type=\"range\" min=\"0\" max=\"255\" value=\"" + String(currentGreen) + "\" class=\"slider\" id=\"green\" name=\"green\" oninput=\"updateValue('green', this.value)\">\n";
    html += "<label for=\"blue\">Blue: <span class=\"rgb-value\" id=\"blue-val\">" + String(currentBlue) + "</span></label>\n";
    html += "<input type=\"range\" min=\"0\" max=\"255\" value=\"" + String(currentBlue) + "\" class=\"slider\" id=\"blue\" name=\"blue\" oninput=\"updateValue('blue', this.value)\">\n";
    html += "<br>\n<button type=\"submit\">Enviar</button>\n</form>\n";
    html += "<div class=\"value\">Status: <b>Online</b></div>\n</div>\n";
    html += "<div class=\"footer\">&copy; 2025 PatroMonitor</div>\n";
    html += "</body>\n</html>\n";
    server.send(200, "text/html", html);
}

// Handler para receber os valores RGB do formulário
void handleSetRGB()
{
    if (server.hasArg("red") && server.hasArg("green") && server.hasArg("blue"))
    {
        uint8_t r = server.arg("red").toInt();
        uint8_t g = server.arg("green").toInt();
        uint8_t b = server.arg("blue").toInt();
        // Atualiza as variáveis globais
        oldRed = 255; // força update
        oldGreen = 255;
        oldBlue = 255;
        currentRed = r;
        currentGreen = g;
        currentBlue = b;
        rgbManualMode = true; // Ativa modo manual
    }
    // Redireciona de volta para a página principal
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

void handleNotFound()
{
    server.sendHeader("Location", "/", true); // Redireciona para "/"
    server.send(302, "text/plain", "");
}

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

void WebServerTask(void *pvParameters)
{
    while (true)
    {
        dnsServer.processNextRequest();
        server.handleClient();
        vTaskDelay(1);
    }
}

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

    // --- Wi-Fi Setup ---
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    Serial.print("[AP] Access Point iniciado. IP: ");
    Serial.println(WiFi.softAPIP());

    // Initialize DNS to redirect all requests to the ESP
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    // Main Page
    server.on("/", handleRoot);
    server.on("/setrgb", handleSetRGB);
    server.onNotFound(handleNotFound);

    // Start Web Server
    server.begin();
    Serial.println("[AP] Servidor web iniciado.");

    // Create task for Web Server at Core 0
    xTaskCreatePinnedToCore(WebServerTask, "WebServerTask", 4096, NULL, 1, NULL, 0);
}

void loop()
{
    static int counter = 0;
    counter++;
    // Se não estiver no modo manual, atualiza RGB automaticamente
    if (!rgbManualMode)
    {
        currentRed = 127 + sin(counter / 20.0) * 128;
        currentGreen = 127 + cos(counter / 30.0) * 128;
        currentBlue = 127 + sin(counter / 40.0) * 128;
    }
    // Atualiza o display apenas se os valores mudarem
    if (counter != oldCounter)
    {
        updateTextValue(100, 80, String(oldCounter), String(counter));
        oldCounter = counter;
    }
    if (currentRed != oldRed)
    {
        updateTextValue(100, 110, String(oldRed), String(currentRed));
        oldRed = currentRed;
    }
    if (currentGreen != oldGreen)
    {
        updateTextValue(100, 140, String(oldGreen), String(currentGreen));
        oldGreen = currentGreen;
    }
    if (currentBlue != oldBlue)
    {
        updateTextValue(100, 170, String(oldBlue), String(currentBlue));
        oldBlue = currentBlue;
    }
    // Atualiza o preview da cor
    uint16_t newColor = tft.color565(currentRed, currentGreen, currentBlue);
    tft.fillRect(200, 80, 100, 120, newColor);
    tft.drawRect(200, 80, 100, 120, BORDER_COLOR);
    // Atualiza o número de clientes conectados
    int clients = WiFi.softAPgetStationNum();
    if (clients != oldClients)
    {
        updateTextValue(100, 200, String(oldClients), String(clients));
        oldClients = clients;
    }
    delay(50);
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

    // Novo: Label para número de clientes conectados
    tft.setCursor(10, 200);
    tft.print("Clients:");

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

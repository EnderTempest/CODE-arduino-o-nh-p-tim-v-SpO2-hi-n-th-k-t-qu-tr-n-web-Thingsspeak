#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <MAX30105.h>
#include <spo2_algorithm.h>
#include <ESP8266WiFi.h>
#include <ThingSpeak.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// Khai báo các đối tượng toàn cục
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MAX30105 particleSensor;

const char* ssid = "MinhThang";
const char* password = "Wifi0fr@@_190";
unsigned long myChannelNumber = 2716311;
const char* myWriteAPIKey = "FPJOR32CQYKT5MYF";

WiFiClient client;
uint32_t tsLastReport = 0;
#define REPORTING_PERIOD_MS 1000

uint32_t irBuffer[100]; // Dữ liệu LED hồng ngoại
uint32_t redBuffer[100]; // Dữ liệu LED đỏ
int32_t bufferLength = 100; // Độ dài dữ liệu
int32_t spo2; // Giá trị SpO₂
int8_t validSPO2; // Trạng thái hợp lệ của SpO₂
int32_t heartRate; // Giá trị nhịp tim
int8_t validHeartRate; // Trạng thái hợp lệ của nhịp tim

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);

    // Kết nối WiFi
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Connected to WiFi");

    // Kết nối ThingSpeak
    ThingSpeak.begin(client);

    // Khởi động màn hình OLED
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }

    // Hiển thị "Pulse Oximeter" khi khởi động
    display.clearDisplay();
    display.setFont(&FreeSerif9pt7b);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 20);
    display.println("Pulse Oximeter");
    display.display();
    delay(2000); // Hiển thị trong 2 giây

    // Khởi động cảm biến MAX30102 với độ nhạy cao
    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("MAX30102 was not found. Please check wiring/power.");
        for(;;);
    }

    // Thiết lập cảm biến với cấu hình mặc định
    particleSensor.setup();
    particleSensor.setPulseAmplitudeRed(0x2F); // Đèn LED đỏ
    particleSensor.setPulseAmplitudeIR(0x2F);  // Đèn LED hồng ngoại
}

void loop() {
    long irValue = particleSensor.getIR();
    Serial.print("IR Value: ");
    Serial.println(irValue);

    if (irValue < 5000) { 
        display.clearDisplay();
        display.setFont(&FreeSerif9pt7b);
        display.setTextSize(1);

        // Hiển thị thông báo "Please place your finger"
        int16_t x, y;
        uint16_t width, height;
        display.getTextBounds("Please place your finger", 0, 0, &x, &y, &width, &height);
        int cursorX = (SCREEN_WIDTH - width) / 2;
        int cursorY = (SCREEN_HEIGHT - height) / 2;

        display.setCursor(cursorX, cursorY);
        display.println("Please place your finger");
        display.display();
        delay(500);
        return;
    }

    // Đọc dữ liệu từ cảm biến vào buffer
    for (byte i = 0; i < bufferLength; i++) {
        while (particleSensor.available() == false)
            particleSensor.check();

        redBuffer[i] = particleSensor.getRed();
        irBuffer[i] = particleSensor.getIR();
        particleSensor.nextSample();
    }

    // Tính toán nhịp tim và SpO₂
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

    // Thay thế giá trị -999 bằng 0
    if (heartRate == -999) {
        heartRate = 0;
    }
    if (spo2 == -999) {
        spo2 = 0;
    }

    if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
        Serial.print("Heart rate: ");
        Serial.print(heartRate);
        Serial.print(" bpm / SpO2: ");
        Serial.print(spo2);
        Serial.println(" %");

        // Hiển thị nhịp tim và SpO₂ trên màn hình OLED
        display.clearDisplay();
        display.setCursor(10, 12);
        display.print("Pulse Oximeter");

        display.setCursor(0, 35);
        display.print("HeartR:");
        display.setCursor(80, 35);
        display.print(heartRate);
        display.println(" b"); // Chỉ hiển thị chữ "b" thay vì "bpm"

        display.setCursor(0, 59);
        display.print("SpO2:");
        display.setCursor(80, 59);
        display.print(spo2);
        display.println(" %");
        display.display();

        tsLastReport = millis();

        // Đăng tải dữ liệu lên ThingSpeak
        if (WiFi.status() == WL_CONNECTED) {
            ThingSpeak.setField(1, heartRate);
            ThingSpeak.setField(2, spo2);

            int responseCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
            if (responseCode == 200) {
                Serial.println("Data successfully uploaded to ThingSpeak");
            } else {
                Serial.println("Failed to upload data to ThingSpeak. Error code: " + String(responseCode));
            }
        }
    }
    delay(500);
}

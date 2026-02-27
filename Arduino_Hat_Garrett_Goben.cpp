#include <Wire.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===== PIN DEFINITIONS =====
#define DHTPIN         8
#define DHTTYPE        DHT11
#define MQ135PIN       A0

// ===== OLED SETTINGS =====
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDRESS   0x3C

// ===== SYSTEM SETTINGS =====
#define READING_INTERVAL  2000        // ms between readings
#define MQ135_SAMPLES     10          // for averaging
#define CALIBRATION_SAMPLES 20        // samples for baseline calibration
#define ERROR_RETRY_COUNT 3            // DHT read retries

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(DHTPIN, DHTTYPE);

// Global variables for non‑blocking timing
unsigned long lastReadingTime = 0;
float temperature = 0.0, humidity = 0.0;
int airQuality = 0;
bool displayEnabled = false;

// MQ135 baseline (calibrated at startup)
int mq135Baseline = 0;

// Simple moving average filter for MQ135
const int numReadings = 10;
int readings[numReadings];
int readIndex = 0;
int total = 0;
int average = 0;

// ===== FUNCTION PROTOTYPES =====
bool initOLED();
void calibrateMQ135();
int readMQ135Filtered();
bool readDHTWithRetry(float &temp, float &humi);
void updateDisplay();

// ===== SETUP =====
void setup() {
  Serial.begin(9600);
  dht.begin();

  // Initialise I2C (pins are default for Uno: A4(SDA), A5(SCL))
  Wire.begin();

  displayEnabled = initOLED();

  // Initialise MQ135 filter array
  for (int i = 0; i < numReadings; i++) {
    readings[i] = 0;
  }

  // Warm‑up message
  if (displayEnabled) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Air Monitor");
    display.println("Calibrating...");
    display.display();
  }
  Serial.println("Calibrating MQ135, keep sensor in clean air...");

  calibrateMQ135();

  if (displayEnabled) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Ready!");
    display.display();
  }
  delay(1000);  // small pause before starting (acceptable once)
}

// ===== MAIN LOOP =====
void loop() {
  // Non‑blocking timing
  if (millis() - lastReadingTime >= READING_INTERVAL) {
    lastReadingTime = millis();

    // Read sensors
    bool dhtOK = readDHTWithRetry(temperature, humidity);
    airQuality = readMQ135Filtered();

    // Print to Serial
    if (dhtOK) {
      Serial.print("Temp: ");
      Serial.print(temperature, 1);
      Serial.print(" C | Humidity: ");
      Serial.print(humidity, 1);
      Serial.print(" % | Air: ");
      Serial.print(airQuality);
      Serial.print(" (baseline: ");
      Serial.print(mq135Baseline);
      Serial.println(")");
    } else {
      Serial.println("DHT sensor error");
    }

    // Update OLED if available
    if (displayEnabled) {
      updateDisplay(dhtOK);
    }
  }
}

// ===== OLED INITIALISATION =====
bool initOLED() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED not found – continuing without display");
    return false;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  return true;
}

// ===== MQ135 CALIBRATION =====
void calibrateMQ135() {
  long sum = 0;
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    sum += analogRead(MQ135PIN);
    delay(50);  // short delay between samples (acceptable during calibration)
  }
  mq135Baseline = sum / CALIBRATION_SAMPLES;
  Serial.print("MQ135 baseline set to: ");
  Serial.println(mq135Baseline);
}

// ===== FILTERED MQ135 READING =====
int readMQ135Filtered() {
  // Subtract the last reading
  total = total - readings[readIndex];
  // Read new value
  readings[readIndex] = analogRead(MQ135PIN);
  // Add to total
  total = total + readings[readIndex];
  // Advance index
  readIndex = (readIndex + 1) % numReadings;
  // Calculate average
  average = total / numReadings;
  return average;
}

// ===== DHT READ WITH RETRIES =====
bool readDHTWithRetry(float &temp, float &humi) {
  for (int i = 0; i < ERROR_RETRY_COUNT; i++) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      temp = t;
      humi = h;
      return true;
    }
    delay(100);  // short delay before retry
  }
  return false;  // all retries failed
}

// ===== UPDATE OLED DISPLAY =====
void updateDisplay(bool dhtOK) {
  display.clearDisplay();
  display.setCursor(0, 0);

  if (!dhtOK) {
    display.println("DHT Error!");
  } else {
    display.print("Temp: ");
    display.print(temperature, 1);
    display.println(" C");

    display.print("Humidity: ");
    display.print(humidity, 1);
    display.println(" %");

    display.print("Air: ");
    display.print(airQuality);

    // Show deviation from baseline
    int diff = airQuality - mq135Baseline;
    display.print(" (");
    display.print(diff > 0 ? "+" : "");
    display.print(diff);
    display.println(")");

    // Air quality status with symbol
    display.setCursor(0, 48);
    if (diff > 100) {   // arbitrary threshold – adjust as needed
      display.print(" STATUS: BAD AIR ");
      display.print(char(0x15));  // ✗ symbol if your font supports it
    } else {
      display.print(" Status: OK ");
      display.print(char(0x13));  // ✓ symbol
    }
  }

  display.display();
}
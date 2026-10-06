#include "arduino_secrets.h"
#include "thingProperties.h"
#include <WiFi.h>
#include <esp_now.h>
#include "esp_adc_cal.h"
#include "driver/adc.h"

// Pin Definiti  ons (ESP32 GPIO pins)
#define TURBIDITY_PIN 34      // ADC1_CHANNEL_6
#define TDS_PIN 35            // ADC1_CHANNEL_7
#define PH_PIN 32             // ADC1_CHANNEL_4

// General ADC Configuratio
#define ADC_MAX_VALUE 4095.0  // ESP32 12-bit ADC
#define ADC_VOLTAGE_REF 3.3   // ADC reference voltage (ensure this matches your ESP32's VDD)

// pH Sensor: ADC Calibration & Specifics
static esp_adc_cal_characteristics_t *adc_chars_ph;
static const adc1_channel_t ph_adc_channel = ADC1_CHANNEL_4;
static const adc_atten_t ph_attenuation = ADC_ATTEN_DB_11;
static const adc_unit_t ph_adc_unit_id = ADC_UNIT_1;
static const adc_bits_width_t ph_adc_bit_width = ADC_WIDTH_BIT_12;
const float VOLTAGE_AT_PH7_ADJUSTED = 2.44f; // Calibrate this
const float SLOPE_V_PER_PH = 0.1841f;        // Calibrate this

// --- Turbidity Sensor Configuration (from Snippet A) ---
const int TURBIDITY_SAMPLES = 10; // Number of samples to average for noise reduction
float currentTurbidityVoltage = 0.0;
static const adc1_channel_t turbidity_adc_channel = ADC1_CHANNEL_6;
static const adc_atten_t turbidity_attenuation = ADC_ATTEN_DB_11; // For full 0-3.3V range
static const adc_bits_width_t turbidity_adc_bit_width = ADC_WIDTH_BIT_12; // ESP32 12-bit ADC

// Turbidity Configuration: adjust based on sensor behavior
// Set to true if higher ADC values mean higher turbidity (murky water)
// Set to false if higher ADC values mean lower turbidity (clear water)
bool adcIncreasesWithTurbidity = false;

// ADC range: adjust based on observed values
int adcMin = 0;    // Minimum ADC value (e.g., clear water)
int adcMax = 1000; // Temporary max ADC (adjust after testing in muddy water)

// TDS Sensor: Simplified Class
class GravityTDS {
private:
  int pin;
  float aref = ADC_VOLTAGE_REF;
  float adcRange = ADC_MAX_VALUE;
public:
  void setPin(int _pin) { pin = _pin; }
  void setAref(float _aref) { aref = _aref; }
  void setAdcRange(float _range) { adcRange = _range; }
  void begin() { pinMode(pin, INPUT); }
  void update() {}
  float getTdsValue() {
    int rawAdc = analogRead(pin);
    float voltage = (float)rawAdc / adcRange * aref;
    // The factor 750.0f is a simplification.
    // Real TDS conversion is: voltage -> EC -> TDS
    // EC = k * voltage (k depends on cell constant and solution)
    // TDS = EC * factor (factor usually 0.5 to 0.7 for ppm)
    // This 750.0f needs calibration with TDS standard solutions.
    float tds_value = voltage * 750.0f; // Placeholder: Calibrate this factor
    return tds_value;
  }
};
GravityTDS gravityTds;

// ESP-NOW Configuration
uint8_t broadcastAddress[] = {0x34, 0x5F, 0x45, 0xA8, 0x77, 0xF0}; // Replace with your Receiver's MAC Address
typedef struct struct_data {
  float s1_ntu;
  float s2_tds;
  float s3_ph;
} struct_data;
struct_data fromSensors;
esp_now_peer_info_t peerInfo;

// Timing
unsigned long lastSensorReadTime = 0;
const unsigned long sensorReadInterval = 5000; // 5 seconds

// Global Variables for sensor values (as used by Arduino IoT Cloud)
float phCalibratedVoltage = 0.0; // For debugging pH voltage
float avgRawAdcTurbidity = 0.0; // Global for printing raw ADC

// ESP-NOW Callback
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nESP-NOW Send Status to: ");
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(macStr);
  Serial.print(" - ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// Initialize Functions
void initEspNow() {
  WiFi.mode(WIFI_STA);
  Serial.print("Device MAC Address: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0; // Use channel 0 for simplicity, or match receiver
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add ESP-NOW peer");
    return;
  }
  Serial.println("ESP-NOW Initialized.");
}

void initTurbiditySensor() {
  // Configure ADC1 for the turbidity sensor pin
  adc1_config_width(turbidity_adc_bit_width); // Set ADC resolution (12-bit)
  esp_err_t err = adc1_config_channel_atten(turbidity_adc_channel, turbidity_attenuation); // Set attenuation for 0-3.3V range
  if (err == ESP_OK) {
    Serial.println("Turbidity sensor ADC configured (GPIO34, ADC1_CH6).");
  } else {
    Serial.println("Failed to configure turbidity ADC: " + String(esp_err_to_name(err)));
  }

  // Debug information from Snippet A
  Serial.println("ESP32 Turbidity Sensor Debug");
  Serial.println("Reading from pin 34 (GPIO34)");
  Serial.println("Expected ADC range: 0-4095 (~0-3.3V)");
  Serial.print("Turbidity mapping: ADC ");
  Serial.print(adcMin);
  Serial.print("-");
  Serial.print(adcMax);
  Serial.println(adcIncreasesWithTurbidity ? " -> 1-5 (higher ADC = higher turbidity)" : " -> 5-1 (higher ADC = lower turbidity)");

  Serial.println("--- IMPORTANT ---");
  Serial.println("Turbidity Sensor: Adjust adcMin, adcMax, and adcIncreasesWithTurbidity based on your sensor behavior!");
  Serial.println("Test in clear and muddy water to determine proper calibration values.");
  Serial.println("-----------------");
}

void initPhSensor() {
  adc1_config_width(ph_adc_bit_width);
  adc1_config_channel_atten(ph_adc_channel, ph_attenuation);
  adc_chars_ph = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ph_adc_unit_id, ph_attenuation, ph_adc_bit_width, ESP_ADC_CAL_VAL_DEFAULT_VREF, adc_chars_ph);
  // Check Characterization
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    Serial.println("pH ADC: eFuse Vref for calibration enabled.");
  } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
    Serial.println("pH ADC: Two Point calibration values enabled.");
  } else {
    Serial.println("pH ADC: Default Vref for calibration enabled.");
  }
  Serial.println("pH Sensor initialized. Verify VOLTAGE_AT_PH7_ADJUSTED and SLOPE_V_PER_PH calibration constants!");
}

void initTdsSensor() {
  gravityTds.setPin(TDS_PIN);
  gravityTds.setAref(ADC_VOLTAGE_REF);
  gravityTds.setAdcRange(ADC_MAX_VALUE);
  gravityTds.begin();
  Serial.println("TDS sensor initialized. Calibrate conversion factor (currently simplified)!");
}

void setup() {
  Serial.begin(115200);
  delay(1500); // Wait for serial monitor to connect
  Serial.println("Water Quality Monitoring System Initializing...");

  initProperties(); // For Arduino IoT Cloud variables

  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2); // Or 0 for no cloud debug messages, 1 for essential, 2 for more
  ArduinoCloud.printDebugInfo();

  initEspNow();
  initPhSensor();
  initTurbiditySensor(); // Initialize Turbidity sensor with ADC settings
  initTdsSensor();

  Serial.println("System Setup Complete. Reading sensors...");
  lastSensorReadTime = millis(); // Initialize for first read
}

void loop() {
  ArduinoCloud.update(); // Keep Arduino IoT Cloud connection alive

  if (millis() - lastSensorReadTime >= sensorReadInterval) {
    onNtuChange();    // Read and process Turbidity
    onPhChange();     // Read and process pH
    onTdsChange();    // Read and process TDS

    // Populate struct for ESP-NOW
    fromSensors.s1_ntu = ntu;
    fromSensors.s2_tds = tds;
    fromSensors.s3_ph = ph;

    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&fromSensors, sizeof(fromSensors));

    printDataToSerial();
    lastSensorReadTime = millis();
  }
}

// --- Sensor Reading and Processing Functions ---

void onNtuChange() {
  // Average multiple readings to reduce noise (from Snippet A)
  long adcSum = 0;
  for (int i = 0; i < TURBIDITY_SAMPLES; i++) {
    adcSum += analogRead(TURBIDITY_PIN);
    delay(10); // Small delay between samples
  }
  int adcAverage = adcSum / TURBIDITY_SAMPLES;

  // Store the averaged ADC value for printing
  avgRawAdcTurbidity = (float)adcAverage;

  // Convert to voltage
  currentTurbidityVoltage = (adcAverage / ADC_MAX_VALUE) * ADC_VOLTAGE_REF;
  currentTurbidityVoltage = roundToDP(currentTurbidityVoltage, 3);

  // Map to turbidity (1 = clear, 5 = murky) - from Snippet A approach
  int turbidityMapped;
  if (adcIncreasesWithTurbidity) {
    turbidityMapped = map(adcAverage, adcMin, adcMax, 1, 5);
  } else {
    turbidityMapped = map(adcAverage, adcMin, adcMax, 5, 1);
  }
  // Constrain turbidity to 1-5
  turbidityMapped = constrain(turbidityMapped, 1, 5);

  // Update global ntu variable
  ntu = (float)turbidityMapped;
  ntu = roundToDP(ntu, 2);

  // Diagnostic warnings from Snippet A
  if (adcAverage < 100 && adcAverage > 0) {
    Serial.println("WARNING: Low ADC reading. Possible causes:");
    Serial.println("- Sensor outputting low voltage (check if expected for clear water).");
    Serial.println("- Partial connection or noise in wiring.");
    Serial.println("- Test: Submerge in muddy water to check for higher ADC.");
  } else if (adcAverage == 0) {
    Serial.println("ERROR: ADC reading 0. Possible causes:");
    Serial.println("- Sensor not connected or signal pin disconnected.");
    Serial.println("- Sensor not powered (check 3.3V/5V and GND).");
    Serial.println("- Signal pin shorted to GND.");
    Serial.println("- Faulty sensor or damaged ESP32 pin.");
    Serial.println("- Test: Connect pin 34 to 3.3V to verify ADC.");
  } else if (adcAverage == 4095) {
    Serial.println("WARNING: ADC reading max value. Possible causes:");
    Serial.println("- Signal pin shorted to 3.3V.");
    Serial.println("- Sensor outputting voltage above 3.3V (check datasheet).");
  }
}

void onPhChange() {
  uint32_t adcRawValueSum = 0;
  const int phSamples = 20; // Increased samples for pH stability
  int validSamples = 0;

  for (int i = 0; i < phSamples; i++) {
    int sample = adc1_get_raw(ph_adc_channel);
    if (sample != -1) { // adc1_get_raw returns -1 on error
      adcRawValueSum += sample;
      validSamples++;
    }
    delay(10); // Short delay between samples
  }

  if (validSamples > 0) {
    uint32_t adcRawValueAvg = adcRawValueSum / validSamples;
    // Convert raw ADC to calibrated voltage in mV
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(adcRawValueAvg, adc_chars_ph);
    phCalibratedVoltage = voltage_mv / 1000.0f; // Convert mV to V
    phCalibratedVoltage = roundToDP(phCalibratedVoltage, 3);

    // pH formula: pH = pH_neutral + (Voltage_at_neutral_pH - Measured_Voltage) / Slope_Volts_per_pH_unit
    // Ensure SLOPE_V_PER_PH is not zero to avoid division by zero
    if (SLOPE_V_PER_PH != 0) {
        ph = 7.0f + ((VOLTAGE_AT_PH7_ADJUSTED - phCalibratedVoltage) / SLOPE_V_PER_PH);
    } else {
        Serial.println("pH Error: SLOPE_V_PER_PH is zero!");
        ph = 7.0; // Default to neutral on error
    }
    ph = roundToDP(ph, 2);

    // Clamp pH to a reasonable range (0-14)
    if (ph < 0.0) ph = 0.0;
    if (ph > 14.0) ph = 14.0;
  } else {
    Serial.println("Error reading pH sensor: No valid samples.");
  }
}

void onTdsChange() {
  tds = gravityTds.getTdsValue(); // This function handles its own averaging/sampling if any
  tds = roundToDP(tds, 0); // TDS usually shown as whole number
  if (tds < 0) tds = 0;
}

// Helper Functions
float roundToDP(float value, int decimalPlaces) {
  float multiplier = pow(10.0f, decimalPlaces);
  return round(value * multiplier) / multiplier;
}

void printDataToSerial() {
  Serial.println("--- Sensor Data Log ---");
  Serial.print("Turbidity Raw ADC Avg: "); Serial.print(avgRawAdcTurbidity, 1);
  Serial.print(" | Voltage: "); Serial.print(currentTurbidityVoltage, 3); Serial.print(" V");
  Serial.print(" | Turbidity: "); Serial.print(ntu, 2); Serial.println(" (mapped 1-5)");

  Serial.print("TDS: "); Serial.print(tds, 0); Serial.println(" ppm (approx)");

  Serial.print("pH Voltage: "); Serial.print(phCalibratedVoltage, 3); Serial.print(" V");
  Serial.print(" | pH: "); Serial.println(ph, 2);
  Serial.println("-------------------------");
}

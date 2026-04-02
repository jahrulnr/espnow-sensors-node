#include "battery_manager.h"

#include "app/algorithms/trimmed_mean.h"
#include <app_config.h>
#include <esp_log.h>

namespace {

static constexpr const char* TAG = "battery";
static constexpr int BATTERY_CRITICAL = 10;
static constexpr int BATTERY_LOW = 25;
static constexpr int BATTERY_MEDIUM = 50;
static constexpr int BATTERY_HIGH = 75;
static constexpr size_t BATTERY_ADC_SAMPLE_CAP = 32;

static constexpr bool BATTERY_NOTIFY_CRITICAL = true;
static constexpr bool BATTERY_NOTIFY_LOW = true;

}  // namespace

BatteryManager::BatteryManager() {
    voltageMax = 4.2;
    voltageMin = 3.3;
    voltageDivider = 2;
    adcResolution = 4095.0; // 12-bit ADC
    batteryPin = INPUT_BATTERY_ADC_PIN;
    chargePin = INPUT_BATTERY_CHARGE_PIN;
    updateInterval = 5000; // 5 seconds

    lastUpdate = 0;
    currentVoltage = 0;
    currentLevel = 0;
    currentState = BATTERY_STATE_CRITICAL;
    chargingState = CHARGING_UNKNOWN;
    
    notifyCritical = BATTERY_NOTIFY_CRITICAL;
    notifyLow = BATTERY_NOTIFY_LOW;
    wasLowNotified = false;
    wasCriticalNotified = false;
    voltageEma.reset();
    voltageEma.setAlpha(BATTERY_VOLTAGE_EMA_ALPHA);

    pinMode(batteryPin, INPUT);
}

BatteryManager::~BatteryManager() {
}

void BatteryManager::init(int pin){
    setPin(pin);
    setup();
}

void BatteryManager::setup() {
    // Configure ADC
    analogReadResolution(12); // Set ADC resolution to 12 bits (0-4095)
    adcResolution = 4095.0;   // 12-bit ADC = 2^12 - 1
    
    // Get initial readings
    update();
    printStatus();
}

void BatteryManager::setVoltage(float min, float max, float divider){
    // Update member variables
    setVoltageMin(min);
    setVoltageMax(max);
    setVoltageDivider(divider);
}

void BatteryManager::update() {
    unsigned long currentTime = millis();
    
    // Only update at the specified interval
    if ((currentTime - lastUpdate) >= updateInterval) {
        // Read and calculate battery voltage and level
        currentVoltage = readVoltage();
        currentLevel = calculateLevel(currentVoltage);
        BatteryState newState = determineState(currentLevel);
        
        // Check if state has changed
        if (newState != currentState) {
            currentState = newState;
            
            // Handle notifications for low and critical states
            if (currentState == BATTERY_STATE_CRITICAL && notifyCritical && !wasCriticalNotified) {
                // Critical battery notification
                wasCriticalNotified = true;
            } 
            else if (currentState == BATTERY_STATE_LOW && notifyLow && !wasLowNotified) {
                // Low battery notification
                wasLowNotified = true;
            }
            
            // Reset notification flags if battery level improved
            if (currentState > BATTERY_STATE_LOW) {
                wasLowNotified = false;
            }
            if (currentState > BATTERY_STATE_CRITICAL) {
                wasCriticalNotified = false;
            }
        }
        
        // Update timestamp
        lastUpdate = currentTime;
    }
}

float BatteryManager::readVoltage() {
    size_t sampleCount = BATTERY_ADC_SAMPLES;
    if (sampleCount < 1) {
        sampleCount = 1;
    }
    if (sampleCount > BATTERY_ADC_SAMPLE_CAP) {
        sampleCount = BATTERY_ADC_SAMPLE_CAP;
    }

    uint16_t samples[BATTERY_ADC_SAMPLE_CAP] = {0};
    for (size_t i = 0; i < sampleCount; i++) {
        const int raw = analogRead(batteryPin);
        const uint16_t clipped = raw < 0 ? 0 : (raw > 65535 ? 65535 : static_cast<uint16_t>(raw));
        samples[i] = clipped;
    }

    const float trimmedAdc = app::algorithms::trimmedMeanU16(samples, sampleCount, BATTERY_ADC_TRIM_PERCENT);
    const float adcVoltage = trimmedAdc * (3.3f / adcResolution);
    const float rawVoltage = adcVoltage * voltageDivider;

#if BATTERY_FILTER_ENABLED
    const float filteredVoltage = voltageEma.update(rawVoltage);
    ESP_LOGD(TAG, "battery voltage raw=%.3f filtered=%.3f", rawVoltage, filteredVoltage);
    return filteredVoltage;
#else
    ESP_LOGD(TAG, "battery voltage raw=%.3f", rawVoltage);
    return rawVoltage;
#endif
}

int BatteryManager::calculateLevel(float voltage) {
    // Calculate battery percentage based on voltage
    // Linear mapping from min voltage (0%) to max voltage (100%)
    if (voltage <= voltageMin) return 0;
    if (voltage >= voltageMax) return 100;
    
    // Linear interpolation
    int level = (int)(((voltage - voltageMin) / (voltageMax - voltageMin)) * 100.0);
    return constrain(level, 0, 100); // Ensure level is between 0-100
}

BatteryState BatteryManager::determineState(int level) {
    // Determine battery state based on percentage
    if (level <= BATTERY_CRITICAL) return BATTERY_STATE_CRITICAL;
    if (level <= BATTERY_LOW) return BATTERY_STATE_LOW;
    if (level <= BATTERY_MEDIUM) return BATTERY_STATE_MEDIUM;
    if (level <= BATTERY_HIGH) return BATTERY_STATE_HIGH;
    return BATTERY_STATE_FULL;
}

void BatteryManager::setUpdateInterval(unsigned long interval) {
    updateInterval = interval;
}

void BatteryManager::setChargingState(ChargingState state) {
    chargingState = state;
}

int BatteryManager::getBatteryIconIndex() const {
    // Return icon index based on battery state and charging state
    if (chargingState == CHARGING_IN_PROGRESS) {
        return 5; // Charging icon
    }
    
    // Return icon based on battery level (0-4)
    switch (currentState) {
        case BATTERY_STATE_CRITICAL: return 0;
        case BATTERY_STATE_LOW:      return 1;
        case BATTERY_STATE_MEDIUM:   return 2;
        case BATTERY_STATE_HIGH:     return 3;
        case BATTERY_STATE_FULL:     return 4;
        default:                     return 2; // Medium as default
    }
}

void BatteryManager::printStatus() const {
    
    // Print state
    switch (currentState) {
        case BATTERY_STATE_CRITICAL: break;
        case BATTERY_STATE_LOW:      break;
        case BATTERY_STATE_MEDIUM:   break;
        case BATTERY_STATE_HIGH:     break;
        case BATTERY_STATE_FULL:     break;
        default:                     break;
    }
    
    // Print charging state
    switch (chargingState) {
        case CHARGING_NOT_CONNECTED: break;
        case CHARGING_IN_PROGRESS:   break;
        case CHARGING_COMPLETE:      break;
        default:                     break;
    }
    
    // Print calibration info
}

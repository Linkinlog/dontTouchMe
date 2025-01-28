# Dont Touch Me! - An ESP32-based Sensor Monitoring System
> [!CAUTION]
> This project is under active development.

A robust IoT solution for monitoring sensor data and sending real-time notifications via HTTP/HTTPS. Ideal for ensuring safes stay put, luggage remains undisturbed, and more.

## Features

- **WiFi Connectivity**: Connects to user-configured WiFi networks with retry logic.
- **Sensor Data Handling**: 
  - Reads analog sensor data via ADC with noise filtering (10-sample averaging).
  - Triggers HTTP notifications when sensor values change.
- **Efficient HTTP Communication**:
  - Connection pooling (3 simultaneous connections).
  - Queued requests to prevent data loss.
  - HTTPS support with certificate bundles.
- **Status Indication**: LED blinks during boot and stops when operational.
- **Configuration**: Uses ESP-IDF's `menuconfig` for WiFi and endpoint settings.
- **Modular Design**: Separated modules for GPIO, HTTP, WiFi, and core logic.

## Hardware Requirements

- ESP32 development board
- Analog sensor (e.g., potentiometer, motion sensor) connected to:
  - ADC Channel: `ADC1_CHANNEL_0` (GPIO 36)
- LED (optional, for status): GPIO 25

## Setup

### Prerequisites
- ESP-IDF v5.0+ environment
- `esp-crt-bundle` component (for HTTPS)

### Configuration
1. Run `idf.py menuconfig`:
   - Set WiFi SSID/password under `Example Configuration`.
   - Configure HTTP endpoint/path under `HTTP Configuration`.
2. Connect sensor to ADC1_CHANNEL_0 (GPIO 36 / VP).
3. (Optional) Connect LED to GPIO 25.

### Build & Flash
```bash
idf.py build
idf.py -p PORT flash monitor
```
## Usage
### Initialization
- LED blinks during WiFi/HTTP setup.
- Solid LED indicates operational state.

### Sensor Monitoring
- Device reads sensor every 50ms.
- Sends HTTP POST to configured endpoint when value changes (5-second debounce).
    - Example payload: {"content":"I've been touched!!"}.

### Network Resilience
- Automatically reconnects to WiFi on failure (up to 5 retries).

## TODO
### High Priority
- [ ] Improve WiFi reconnection logic
- [ ] Add web interface for configuration
    - [ ] View sensor data
    - [ ] Change the wifi network
    - [ ] Change the notification settings
    - [ ] Change the push notification service
- [ ] Document hardware wiring diagram
- [ ] Add a license
- [ ] Add a contributing guide
- [ ] Add a code of conduct

### Future Goals
- [ ] REFACTOR again probably
- [ ] GPS/cellular integration for mobile tracking
- [ ] Add a changelog
- [ ] Custom PCB
- [ ] Custom Enclosure
    - [ ] Add a battery
- [ ] OTA firmware updates

## License
To be added (Suggested: MIT License).

## Contributing
Contributions welcome! Please review the (upcoming) CONTRIBUTING.md guidelines.

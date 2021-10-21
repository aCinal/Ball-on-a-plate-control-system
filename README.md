# Ball-on-a-plate control system

Distributed control application for an ESP32-based ball-on-a-plate system

## Usage

1. Clone the `https://github.com/espressif/esp-idf` repository

```bash
git clone https://github.com/espressif/esp-idf
```

2. Set up the ESP-IDF environment as described here: `https://github.com/espressif/esp-idf`

3. Build a selected component, e.g. *plant*

```bash
cd plant
idf.py build
```

4. Flash the board and monitor its startup

```bash
idf.py -p (PORT) flash monitor
```

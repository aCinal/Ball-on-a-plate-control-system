# Ball-on-a-plate control system

Distributed control application for an ESP32-based ball-on-a-plate system

## Usage

1. Clone the `https://github.com/espressif/esp-idf` repository

```bash
git clone https://github.com/espressif/esp-idf
```

2. Set up the ESP-IDF environment as described here: `https://github.com/espressif/esp-idf`

3. Set the MAC addresses of specific nodes in file `env.cmake` (see `env.cmake.example` for reference)

4. Flash the boards and monitor their startup

```bash
cd plant
idf.py build
idf.py -p (PORT) flash monitor
cd ../router
idf.py build
idf.py -p (PORT) flash monitor
cd ../controller
idf.py build
idf.py -p (PORT) flash monitor
cd ..
```

5. Set up a Python virtual environment and install dependencies

```bash
cd python
python -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

6. Start the Python GUI application

```bash
python boap.py -p (ROUTER PORT) -e stdout
```

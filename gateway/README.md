## Gateway

This ESP32-S3 needs a driver for M1 macbooks
- https://www.wch.cn/downloads/CH34XSER_MAC_ZIP.html

Working SIM connection
- https://gist.github.com/leighghunt/016f615ba5af4816482cd7d264b68411


### Boards 

#### Waveshare ESP32-S3-SIM7607G

1. Battery Management 

Unlike some ESP32 dev boards (e.g. Heltec, LilyGO) that expose the battery voltage on an ADC pin through a resistor divider, the Waveshare ESP32-S3-SIM7607G does not connect VBAT to an ESP32 ADC pin. Instead, the board includes a MAX17048 fuel gauge IC, which measures the LiPo battery’s voltage and state-of-charge (SOC) internally. The ESP32 reads these values over I²C, not analogRead().

- Cannot use analogRead() a GPIO for battery level
- Need to talk to the MAX17048 via I²C (using a fuel guage library).

A benefit of this is getting accurate voltage + percentage without needing to calibrate LiPo discharge curves yourself.
# Vindriktning

Code for Laskakit Vindriktning ESP32 board (https://www.laskakit.cz/laskakit-esp-vindriktning-esp-32-i2c/) with CO2 sensor (https://www.laskakit.cz/laskakit-scd41-senzor-co2--teploty-a-vlhkosti-vzduchu/) connected.

Sends temperature, humidity, co2 and pm25 values to MQTT with topi string "vindriktning/0xhhhhhhhhhhhh" where hhhhhhhhhhhh serial number of SCD41 in hex form (lowercase)
Body of MQTT message is in json format: {"temperature": 19.63, "humidity": 50.35, "co2": 1301, "pm25": 11}

Registers "vindriktning/0xhhhhhhhhhhhh/light" on MQTT server. Expects "on" and "off" messages.



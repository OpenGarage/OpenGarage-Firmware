~/esp8266_3.0/tools/esptool/esptool.py -p /dev/ttyUSB? -b 460800 -a soft_reset write_flash 0x00000 og_1.2.3.bin
sleep 3
gtkterm -p /dev/ttyUSB0 -s 115200

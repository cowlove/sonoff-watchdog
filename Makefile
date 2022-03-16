#BOARD=esp32doit-devkit-v1
#BOARD=heltec_wifi_kit_32
#BOARD=nodemcu-32s
#VERBOSE=1

CHIP=esp8266
BOARD=sonoff

OTA_ADDR=192.168.4.111
IGNORE_STATE=1
GIT_VERSION := "$(shell git describe --abbrev=4 --dirty --always --tags)"
BUILD_EXTRA_FLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
EXCLUDE_DIRS=esp32-micro-sdcard

include ${HOME}/Arduino/libraries/makeEspArduino/makeEspArduino.mk

fixtty:
	stty -F ${UPLOAD_PORT} -hupcl -crtscts -echo raw 115200 

cat:	fixtty
	cat ${UPLOAD_PORT}

socat:	
	socat udp-recv:9000 - 
	
backtrace:
	tr ' ' '\n' | /home/jim/.arduino15/packages/esp32/tools/xtensa-esp32-elf-gcc/1.22.0-80-g6c4433a-5.2.0/bin/xtensa-esp32-elf-addr2line -f -i -e $(BUILD_DIR)/$(MAIN_NAME).elf

curl:
	curl -v -F "image=@${BUILD_DIR}/${MAIN_NAME}.bin" ${OTA_ADDR}/update


csim:	ota.ino ESP32sim_ubuntu.h jimlib.h 
	g++  -x c++ -g $< -o $@ -DESP8266 -DUBUNTU -I./ 

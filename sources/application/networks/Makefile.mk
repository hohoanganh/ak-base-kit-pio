ifeq ($(IF_NETWORK_NRF24_OPTION),-DIF_NETWORK_NRF24_EN)
include sources/networks/rf_protocols/Makefile.mk
endif

ifeq ($(IF_LINK_OPTION),-DIF_LINK_UART_EN)
include sources/networks/net/link/Makefile.mk
endif

ifeq ($(TASK_ZIGBEE_OPTION),-DTASK_ZIGBEE_EN)
include sources/networks/ArduinoZigBee/Makefile.mk
endif

# mbmaster-v2.9.6 (thu vien thuong mai) da go, thay bang nanoMODBUS + mb_port
# (build bang PlatformIO, khong con Makefile.mk rieng cho module nay).

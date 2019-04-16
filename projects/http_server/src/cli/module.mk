
LIB_SRC := cli.c
LIB_SRC += cli_cmd.c
LIB_SRC += http_parser.c
LIB_SRC += user_httpserver.c


LIB_ASRC :=
LIBRARY_NAME := cli
LOCAL_CFLAGS :=
LOCAL_AFLAGS :=

LOCAL_INC += -Icomponents/inc
LOCAL_INC += -Icomponents/drv
LOCAL_INC += -Icomponents/sys

ifeq ($(strip $(OTA_EN)), 1)
LOCAL_INC += -Icomponents/tools/ota_api
endif

ifeq ($(strip $(SMARTCONFIG_EN)), 1)
LOCAL_INC += -Icomponents/third_party
endif

ifeq ($(strip $(LWIP_HTTPD_EN)), 1)
#LOCAL_INC += -Icomponents/third_party/lwipserver
endif

LOCAL_INC += -Icomponents/bsp/soc/lowpower
LOCAL_INC += -Icomponents/net/tcpip/lwip-1.4.0/src/include/lwip

$(eval $(call build-lib,$(LIBRARY_NAME),$(LIB_SRC),$(LIB_ASRC),$(LOCAL_CFLAGS),$(LOCAL_INC),$(LOCAL_AFLAGS),$(MYDIR)))

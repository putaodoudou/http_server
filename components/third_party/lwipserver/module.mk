LIB_SRC := fs.c
LIB_SRC += httpd.c
LIB_SRC += httpd_cgi_ssi.c
LIB_SRC += fsdata.c

				   
				   
LIB_ASRC :=
LIBRARY_NAME := lwip_httpd
LOCAL_CFLAGS += -Werror-implicit-function-declaration -Wno-address
LOCAL_AFLAGS += 

LOCAL_INC += -Icomponents/softmac
LOCAL_INC += -Icomponents/iotapi
LOCAL_INC += -Icomponents/third_party/iperf3.0
LOCAL_INC += -Icomponents/netstack_wrapper
LOCAL_INC += -Icomponents/net/tcpip/lwip-1.4.0/src/include
LOCAL_INC += -Icomponents/net/tcpip/lwip-1.4.0/ports/icomm/include
LOCAL_INC += -Icomponents/net/tcpip/lwip-1.4.0/src/include/ipv4

RELEASE_SRC := 2

$(eval $(call build-lib,$(LIBRARY_NAME),$(LIB_SRC),$(LIB_ASRC),$(LOCAL_CFLAGS),$(LOCAL_INC),$(LOCAL_AFLAGS),$(MYDIR)))

#include "lwip/debug.h"
#include "httpd.h"
#include "lwip/tcp.h"
#include <lwip/sockets.h>
#include <lwip/netif.h>
#include <lwip/ip_addr.h>
#include <lwip/dhcp.h>
#include <lwip/sys.h>

#include "fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wificonf.h"

#include <wifi_api.h>
#include "softap_func.h"

#include "porting.h"
#include "ieee80211_mgmt.h"
#if HTTPD_SUPPORT

#define NUM_CONFIG_CGI_URIS	(sizeof(ppcURLs) / sizeof(tCGI))
#define NUM_CONFIG_SSI_TAGS	(sizeof(ppcTAGs) / sizeof(char *))

const char* LOGIN_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
const char* WIFI_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
const char* NETCFG_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
const char* APLIST_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
const char* SCAN_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);

#define APLIST_TAG_NAME     "aplist"
#define APLIST_COUNT_TAG_NAME "aplistcount"
#define APLIST_PAGE_TAG_NAME "aplistpage"

static const char *ppcTAGs[]=
{
	APLIST_TAG_NAME,
	APLIST_COUNT_TAG_NAME,
	APLIST_PAGE_TAG_NAME,
};

static const tCGI ppcURLs[]=
{
    {"/login.cgi",LOGIN_CGI_Handler},
    {"/wifimode.cgi",WIFI_CGI_Handler},
    {"/scan.cgi",SCAN_CGI_Handler},
    {"/aplist.cgi",APLIST_CGI_Handler},
};

static int g_aplist_page = 1;
#define 	AP_PAGE_NUM 20

static int FindCGIParameter(const char *pcToFind,char *pcParam[],int iNumParams)
{
	int iLoop;
	for(iLoop = 0;iLoop < iNumParams;iLoop ++ )
	{
		if(strcmp(pcToFind,pcParam[iLoop]) == 0)
		{
			return (iLoop);
		}
	}
	return (-1);
}





//BSSID#50:7e:5d:45:c1:86;SSID#SBC_RX;proto#3;channel#11;
void web_aplist_get_str(char *wifi_list_str, int page)
{
    u32 i = 0;
    u8  temp[128];
    u32 count = 0;
    u8 page_num = 1;
    u8 ssid_buf[MAX_SSID_LEN+1]={0};

    if (!wifi_list_str)
    {
	    LOG_PRINTF("wifi_list_str null !!\r\n");
        return;
    }
	//OS_EnterCritical();
	//OS_MutexLock(gDeviceInfo->g_dev_info_mutex);
    wifi_list_str[0]=0;

    for (i=0; i<getAvailableIndex(); i++)
    {
        if(ap_list[i].channel!= 0)
        {
            if (page_num == page)
            {
                sprintf((void *)temp, "BSSID#%02x:%02x:%02x:%02x:%02x:%02x;",
                ap_list[i].mac[0],ap_list[i].mac[1],ap_list[i].mac[2],ap_list[i].mac[3],ap_list[i].mac[4],ap_list[i].mac[5]);
				strcat(wifi_list_str, (void *)temp);
                MEMSET((void*)ssid_buf,0,sizeof(ssid_buf));
                MEMCPY((void*)ssid_buf,(void*)ap_list[i].name,ap_list[i].name_len);
                sprintf((void *)temp, "SSID#%s;", ssid_buf);
                strcat(wifi_list_str, (void *)temp);
                sprintf((void *)temp, "proto#%d;", ap_list[i].security_type);
                strcat(wifi_list_str, (void *)temp);
                sprintf((void *)temp, "channel#%d;\r\n",ap_list[i].channel);
                strcat(wifi_list_str, (void *)temp);
            }
            count++;
            if (count >= AP_PAGE_NUM)
            {
                count = 0;
                page_num++;
            }
        }
    }
   //OS_ExitCritical();
}


void Aplist_Handler(char *pcInsert)
{
	web_aplist_get_str(pcInsert, g_aplist_page);
}

void AplistCount_Handler(char *pcInsert)
{
    //sprintf(pcInsert, "%d", (getAvailableIndex()+AP_PAGE_NUM-1)/AP_PAGE_NUM);
	 sprintf(pcInsert, "%d", getAvailableIndex());
}

void AplistPage_Handler(char *pcInsert)
{
    sprintf(pcInsert, "%d", g_aplist_page);
}




const char * SSIGetTagName(int iIndex)
{
    if (iIndex < (int)NUM_CONFIG_SSI_TAGS)
    {
        return ppcTAGs[iIndex];
    }
    else
    {
        return NULL;
    }
}

static u16_t SSIHandler(int iIndex,char *pcInsert,int iInsertLen)
{
    char *TagName = NULL;

    TagName = (char *)SSIGetTagName(iIndex);

    if (TagName == NULL) return 0;
	if (STRCMP(TagName, APLIST_TAG_NAME) == 0)
    {
        Aplist_Handler(pcInsert);
    }
    else if (STRCMP(TagName, APLIST_COUNT_TAG_NAME) == 0)
    {
        AplistCount_Handler(pcInsert);
    }
    else if (STRCMP(TagName, APLIST_PAGE_TAG_NAME) == 0)
    {
        AplistPage_Handler(pcInsert);
    }

	return strlen(pcInsert);
}


const char* LOGIN_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
  LOG_PRINTF("LOGIN_CGI_Handler\r\n");

  iIndex = FindCGIParameter("username",pcParam,iNumParams);
  if (iIndex != -1)
  {
      LOG_PRINTF("username: %s\r\n", pcValue[iIndex]);
  }

  iIndex = FindCGIParameter("password",pcParam,iNumParams);
  if (iIndex != -1)
  {
      LOG_PRINTF("password: %s\r\n", pcValue[iIndex]);
  }

  //return "/run_status.shtml";
  return "/wireless_config.shtml";

}


static OsTimer conn_ap_timer;
static u8 connect_ssid[33];
static u8 connect_pwd[65];

//Connects to an AP
static void atwificbfunc(WIFI_RSP *msg)
{
    uint8_t dhcpen;
    u8 mac[6];
    uip_ipaddr_t ipaddr, submask, gateway, dnsserver;
	OS_TimerDelete(conn_ap_timer);
    if(msg->wifistatus == 1)
    {
        printf("connect OK\n");
		
        if(msg->id == 0)
            get_if_config_2("et0", &dhcpen, &ipaddr, &submask, &gateway, &dnsserver, mac, 6);
        else
            get_if_config_2("et1", &dhcpen, &ipaddr, &submask, &gateway, &dnsserver, mac, 6);
        printf("STA%d:\n", msg->id);
        printf("mac addr        - %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        printf("ip addr         - %d.%d.%d.%d\n", ipaddr.u8[0], ipaddr.u8[1], ipaddr.u8[2], ipaddr.u8[3]);
        printf("netmask         - %d.%d.%d.%d\n", submask.u8[0], submask.u8[1], submask.u8[2], submask.u8[3]);
        printf("default gateway - %d.%d.%d.%d\n", gateway.u8[0], gateway.u8[1], gateway.u8[2], gateway.u8[3]);
        printf("DNS server      - %d.%d.%d.%d\n", dnsserver.u8[0], dnsserver.u8[1], dnsserver.u8[2], dnsserver.u8[3]);
    }
    else
    {
        printf("disconnect OK\n");
    }
}

void conn_ap_handler(void)
{
	softap_exit();
	DUT_wifi_start(DUT_STA);
	wifi_connect_active_3(connect_ssid,STRLEN(connect_ssid),connect_pwd,STRLEN(connect_pwd),\
		NET80211_CRYPT_UNKNOWN,0,NULL,atwificbfunc);
}

int init_conn_ap_timer(void)
{
    printf("%s\n", __func__);
    if( OS_TimerCreate(&conn_ap_timer, 3000, (u8)FALSE, NULL, (OsTimerHandler)conn_ap_handler) == OS_FAILED)
        return -1;
    OS_TimerStart(conn_ap_timer);
    return 0;
}



const char* WIFI_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    WIFI_OPMODE mode;

	//uint8_t connect_bssid[6];

	memset(connect_ssid,0,sizeof(connect_ssid));
	memset(connect_pwd,0,sizeof(connect_pwd));
	//memset(connect_bssid,0,sizeof(connect_bssid))
    LOG_PRINTF("WIFI_CGI_Handler\r\n");
	
    iIndex = FindCGIParameter("ssid",pcParam,iNumParams);
    if (iIndex != -1)
    {
        LOG_PRINTF("ssid:%s\r\n", pcValue[iIndex]);
        if (strlen( pcValue[iIndex]) == 0)
        {
			//return "/run_status.shtml";
			return "/wireless_config.shtml";
        }
		memcpy(connect_ssid,pcValue[iIndex],STRLEN(pcValue[iIndex])>=33?32:STRLEN(pcValue[iIndex]));
    }
    else
    {
		//return "/run_status.shtml";
		return "/wireless_config.shtml";
    }

    iIndex = FindCGIParameter("key",pcParam,iNumParams);
    if (iIndex != -1)
    {
        LOG_PRINTF("key:%s\r\n", pcValue[iIndex]);
        if (strlen( pcValue[iIndex]) == 0)
        {
            //return "/run_status.shtml";
    		return "/wireless_config.shtml";
        }
		memcpy(connect_pwd,pcValue[iIndex],STRLEN(pcValue[iIndex])>=65?64:STRLEN(pcValue[iIndex]));
    }
	
    iIndex = FindCGIParameter("channel",pcParam,iNumParams);
    if (iIndex != -1)
    {
        LOG_PRINTF("channel: %s\r\n", pcValue[iIndex]);
    }
	#if 1
	init_conn_ap_timer();
	#endif
    //return "/run_status.shtml";
    return "/wireless_config.shtml";
}


const char* APLIST_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int totalpage = 0;

    LOG_PRINTF("APLIST_CGI_Handler\r\n");

    iIndex = FindCGIParameter("page",pcParam,iNumParams);
    if (iIndex != -1)
    {
        LOG_PRINTF("page: %s\r\n", pcValue[iIndex]);
        if (atoi(pcValue[iIndex]) > 0)
        {
            g_aplist_page = atoi(pcValue[iIndex]);
            totalpage = getAvailableIndex();
            g_aplist_page = (g_aplist_page > totalpage) ? totalpage : g_aplist_page;
        }
        else
        {
            g_aplist_page = 1;
        }
    }

  return "/wireless_config.shtml";
}

static int scan_flg = 0;
static void scan_cbfunc()
{
    u8 i;

    printf("\nCount:%d\n", getAvailableIndex());/*
    for(i = 0; i < getAvailableIndex(); i++)
    {
        printf("%2d - name:%32s, rssi:-%2d CH:%2d mac:%02x-%02x-%02x-%02x-%02x-%02x\n", i, ap_list[i].name, ap_list[i].rssi, ap_list[i].channel
                , ap_list[i].mac[0], ap_list[i].mac[1], ap_list[i].mac[2], ap_list[i].mac[3], ap_list[i].mac[4], ap_list[i].mac[5]);
    }*/
    printf("end\n");
	scan_flg = 0;
}

const char* SCAN_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
  LOG_PRINTF("SCAN_CGI_Handler\r\n");
  g_aplist_page = 1;
  //scan_flg = 1;
  //scan_AP(scan_cbfunc);
  //while(scan_flg==1){OS_MsDelay(10);}
  return "/wireless_config.shtml";
}


void httpd_ssi_init(void)
{
	http_set_ssi_handler(SSIHandler,ppcTAGs,NUM_CONFIG_SSI_TAGS);
}

void httpd_cgi_init(void)
{
     http_set_cgi_handlers(ppcURLs, NUM_CONFIG_CGI_URIS);
}
#endif /* HTTPD_SUPPORT */


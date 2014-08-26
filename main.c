//*****************************************************************************
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/ 
// 
// 
//  Redistribution and use in source and binary forms, with or without 
//  modification, are permitted provided that the following conditions 
//  are met:
//
//    Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the 
//    documentation and/or other materials provided with the   
//    distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************


//*****************************************************************************
//
// Application Name     -   Provisioning with Smart Config
// Application Overview -   This is a sample application demonstrating how to
//                          connect a  CC3200 device to an AP using Smart Config
//
// Application Details  -
// docs\examples\CC32xx Provisioning_Smart_Config.pdf
// or
// http://processors.wiki.ti.com/index.php/CC32xx_Provisioning_Smart_Config
//
//*****************************************************************************


//****************************************************************************
//
//! \addtogroup provisioning_smart_config
//! @{
//
//****************************************************************************
#include "stdio.h"
#include <stdlib.h>
#include <string.h>

// Simplelink includes
#include "simplelink.h"

//Driverlib includes
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "rom.h"
#include "rom_map.h"
#include "interrupt.h"
#include "prcm.h"
#include "utils.h"

//Common interface includes
#include "pinmux.h"
#include "gpio_if.h"
#include "udma_if.h"
#ifndef NOTERM
#include "uart_if.h"
#endif

//Application includes
#include "CommandTable.h"

#ifdef NOTERM
#define UART_PRINT(x,...)
#else
#define UART_PRINT Report
#endif

#define APPLICATION_NAME        "PROVISIONING_SMART_CONFIG"
#define APPLICATION_VERSION     "1.0.0"
#define SUCCESS                 0


#define WLAN_DEL_ALL_PROFILES   0xFF
#define SSID_LEN_MAX            (32)
#define BSSID_LEN_MAX           (6)
#define BUF_SIZE            	20

#define SL_STOP_TIMEOUT          30
#define UNUSED(x)               x = x
#define CLR_STATUS_BIT_ALL(status_variable)  (status_variable = 0)

// Loop forever, user can change it as per application's requirement
#define LOOP_FOREVER(line_number) \
            {\
                while(1); \
            }

// check the error code and handle it
#define ASSERT_ON_ERROR(line_number, error_code) \
            {\
                if (error_code < 0) return error_code;\
            }

//Status bits - These are used to set/reset the corresponding bits in
// given variable
typedef enum{
    STATUS_BIT_CONNECTION =  0, // If this bit is Set SimpleLink device is
                                // connected to the AP

    STATUS_BIT_IP_AQUIRED,       // If this bit is Set SimpleLink device has
                                 // acquired IP

}e_StatusBits;


// Application specific status/error codes
typedef enum{
    // Choosing -0x7D0 to avoid overlap w/ host-driver's error codes
    LAN_CONNECTION_FAILED = -0x7D0,
    DEVICE_NOT_IN_STATION_MODE = LAN_CONNECTION_FAILED - 1,
    STATUS_CODE_MAX = -0xBB8
}e_AppStatusCodes;

#define SET_STATUS_BIT(status_variable, bit)  status_variable |= (1<<(bit))
#define CLR_STATUS_BIT(status_variable, bit)  status_variable &= ~(1<<(bit))
#define GET_STATUS_BIT(status_variable, bit)  (0 != (status_variable & (1<<(bit))))

#define IS_CONNECTED(status_variable)         GET_STATUS_BIT(status_variable, \
                                                       STATUS_BIT_CONNECTION)
#define IS_IP_ACQUIRED(status_variable)       GET_STATUS_BIT(status_variable, \
                                                       STATUS_BIT_IP_AQUIRED)

//
// GLOBAL VARIABLES -- Start
//
char g_cBsdBuf[BUF_SIZE];
unsigned long  g_ulStatus = 0;//SimpleLink Status
unsigned long  g_ulGatewayIP = 0; //Network Gateway IP address
unsigned char  g_ucConnectionSSID[SSID_LEN_MAX+1]; //Connection SSID
unsigned char  g_ucConnectionBSSID[BSSID_LEN_MAX]; //Connection BSSID
#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif
//
// GLOBAL VARIABLES -- End
//

//****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES
//****************************************************************************
int BsdTcpServer(unsigned short usPort);
static void DisplayBanner();
int SmartConfigConnect();
static void BoardInit(void);
static void InitializeAppVariables();
static void mDNS_Task();

//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- Start
//*****************************************************************************


//*****************************************************************************
//
//! \brief The Function Handles WLAN Events
//!
//! \param[in]  pWlanEvent - Pointer to WLAN Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
    switch(pWlanEvent->Event)
    {
        case SL_WLAN_CONNECT_EVENT:
        {
            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);

            //
            // Information about the connected AP (like name, MAC etc) will be
            // available in 'sl_protocol_wlanConnectAsyncResponse_t'
            // Applications can use it if required
            //
            //  sl_protocol_wlanConnectAsyncResponse_t *pEventData = NULL;
            // pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
            //

            // Copy new connection SSID and BSSID to global parameters
            memcpy(g_ucConnectionSSID,pWlanEvent->EventData.
                   STAandP2PModeWlanConnected.ssid_name,
                   pWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_len);
            memcpy(g_ucConnectionBSSID,
                   pWlanEvent->EventData.STAandP2PModeWlanConnected.bssid,
                   SL_BSSID_LENGTH);

            UART_PRINT("[WLAN EVENT] STA Connected to the AP: %s , "
            		   "BSSID: %x:%x:%x:%x:%x:%x\n\r",
                      g_ucConnectionSSID,g_ucConnectionBSSID[0],
                      g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                      g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                      g_ucConnectionBSSID[5]);
        }
        break;

        case SL_WLAN_DISCONNECT_EVENT:
        {
            sl_protocol_wlanConnectAsyncResponse_t*  pEventData = NULL;

            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

            pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;

            // If the user has initiated 'Disconnect' request,
            //'reason_code' is SL_USER_INITIATED_DISCONNECTION
            if(SL_USER_INITIATED_DISCONNECTION == pEventData->reason_code)
            {
                UART_PRINT("[WLAN EVENT]Device disconnected from the AP: %s, "
                		   "BSSID: %x:%x:%x:%x:%x:%x on application's "
                		   "request \n\r",
                           g_ucConnectionSSID,g_ucConnectionBSSID[0],
                           g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                           g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                           g_ucConnectionBSSID[5]);
            }
            else
            {
                UART_PRINT("[WLAN ERROR]Device disconnected from the AP AP: %s, "
                		   "BSSID: %x:%x:%x:%x:%x:%x on an ERROR..!! \n\r",
                           g_ucConnectionSSID,g_ucConnectionBSSID[0],
                           g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                           g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                           g_ucConnectionBSSID[5]);
            }
            memset(g_ucConnectionSSID,0,sizeof(g_ucConnectionSSID));
            memset(g_ucConnectionBSSID,0,sizeof(g_ucConnectionBSSID));
        }
        break;

        default:
        {
            UART_PRINT("[WLAN EVENT] Unexpected event [0x%x]\n\r",
                       pWlanEvent->Event);
        }
        break;
    }
}

//*****************************************************************************
//
//! \brief This function handles network events such as IP acquisition, IP
//!           leased, IP released etc.
//!
//! \param[in]  pNetAppEvent - Pointer to NetApp Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
    switch(pNetAppEvent->Event)
    {
        case SL_NETAPP_IPV4_ACQUIRED:
        {
            SlIpV4AcquiredAsync_t *pEventData = NULL;

            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

            //Ip Acquired Event Data
            pEventData = &pNetAppEvent->EventData.ipAcquiredV4;

            //Gateway IP address
            g_ulGatewayIP = pEventData->gateway;

            UART_PRINT("[NETAPP EVENT] IP Acquired: IP=%d.%d.%d.%d , "
            		   "Gateway=%d.%d.%d.%d\n\r",
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,3),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,2),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,1),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,0),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,3),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,2),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,1),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,0));
        }
        break;

        default:
        {
            UART_PRINT("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
                       pNetAppEvent->Event);
        }
        break;
    }
}


//*****************************************************************************
//
//! \brief This function handles HTTP server events
//!
//! \param[in]  pServerEvent - Contains the relevant event information
//! \param[in]    pServerResponse - Should be filled by the user with the
//!                                      relevant response information
//!
//! \return None
//!
//****************************************************************************
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent,
                                  SlHttpServerResponse_t *pHttpResponse)
{
    // Unused in this application
}

//*****************************************************************************
//
//! \brief This function handles General Events
//!
//! \param[in]     pDevEvent - Pointer to General Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    //
    // Most of the general errors are not FATAL are are to be handled
    // appropriately by the application
    //
    UART_PRINT("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
               pDevEvent->EventData.deviceEvent.status,
               pDevEvent->EventData.deviceEvent.sender);
}


//*****************************************************************************
//
//! This function handles socket events indication
//!
//! \param[in]      pSock - Pointer to Socket Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    //
    // This application doesn't work w/ socket - Events are not expected
    //
       switch( pSock->Event )
    {
        case SL_NETAPP_SOCKET_TX_FAILED:
            switch( pSock->EventData.status )
            {
                case SL_ECLOSE:
                    UART_PRINT("[SOCK ERROR] - close socket (%d) operation "
                    		   "failed to transmit all queued packets\n\n",
                               pSock->EventData.sd);
                    break;
                default:
                    UART_PRINT("[SOCK ERROR] - TX FAILED  :  socket %d , "
                    		   "reason (%d) \n\n",
                               pSock->EventData.sd, pSock->EventData.status);
            }
            break;

        default:
            UART_PRINT("[SOCK EVENT] - Unexpected Event [%x0x]\n\n", \
            		    pSock->Event);
    }
}

//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- End
//*****************************************************************************


//*****************************************************************************
//
//! \brief This function initializes the application variables
//!
//! \param    None
//!
//! \return None
//!
//*****************************************************************************
static void InitializeAppVariables()
{
    g_ulStatus = 0;
    g_ulGatewayIP = 0;
    memset(g_ucConnectionSSID,0,sizeof(g_ucConnectionSSID));
    memset(g_ucConnectionBSSID,0,sizeof(g_ucConnectionBSSID));
}

//*****************************************************************************
//! \brief This function puts the device in its default state. It:
//!           - Set the mode to STATION
//!           - Configures connection policy to Auto and AutoSmartConfig
//!           - Deletes all the stored profiles
//!           - Enables DHCP
//!           - Disables Scan policy
//!           - Sets Tx power to maximum
//!           - Sets power policy to normal
//!           - TBD - Unregister mDNS services
//!
//! \param   none
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************

static long ConfigureSimpleLinkToDefaultState()
{
    SlVersionFull   ver = {0};

    unsigned char ucVal = 1;
    unsigned char ucConfigOpt = 0;
    unsigned char ucConfigLen = 0;
    unsigned char ucPower = 0;

    long lRetVal = -1;
    long lMode = -1;

    lMode = sl_Start(0, 0, 0);
    ASSERT_ON_ERROR(__LINE__, lMode);

    // Get the device's version-information
    ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
    ucConfigLen = sizeof(ver);
    lRetVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ucConfigOpt, \
    		            &ucConfigLen, (unsigned char *)(&ver));
    ASSERT_ON_ERROR(__LINE__, lRetVal);

    UART_PRINT("Host Driver Version: %s\n\r",SL_DRIVER_VERSION);
    UART_PRINT("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
                        ver.NwpVersion[0],ver.NwpVersion[1],
                        ver.NwpVersion[2],ver.NwpVersion[3],
                        ver.ChipFwAndPhyVersion.FwVersion[0],
                        ver.ChipFwAndPhyVersion.FwVersion[1],
                        ver.ChipFwAndPhyVersion.FwVersion[2],
                        ver.ChipFwAndPhyVersion.FwVersion[3],
                        ver.ChipFwAndPhyVersion.PhyVersion[0],
                        ver.ChipFwAndPhyVersion.PhyVersion[1],
                        ver.ChipFwAndPhyVersion.PhyVersion[2],
                        ver.ChipFwAndPhyVersion.PhyVersion[3]);

    // Set connection policy to Auto + SmartConfig (Device's default connection
    // policy)
    lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION, \
    		                   SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);

    ASSERT_ON_ERROR(__LINE__, lRetVal);

    // Remove all profiles
    lRetVal = sl_WlanProfileDel(0xFF);
    ASSERT_ON_ERROR(__LINE__, lRetVal);

    // If the device is not in station-mode, try putting it in staion-mode
    if (ROLE_STA != lMode)
    {
        if (ROLE_AP == lMode)
        {
            // If the device is in AP mode, we need to wait for this event
        	// before doing anything
            while(!IS_IP_ACQUIRED(g_ulStatus))
            {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask();
#endif
            }
        }

        // Switch to STA role and restart
        lRetVal = sl_WlanSetMode(ROLE_STA);
        ASSERT_ON_ERROR(__LINE__, lRetVal);

        lRetVal = sl_Stop(SL_STOP_TIMEOUT);
        ASSERT_ON_ERROR(__LINE__, lRetVal);

        lRetVal = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(__LINE__, lRetVal);

        // Check if the device is in station again
        if (ROLE_STA != lRetVal)
        {
            // We don't want to proceed if the device is not coming up in
        	// station-mode
            return DEVICE_NOT_IN_STATION_MODE;
        }
    }

    //
    // Device in station-mode. Disconnect previous connection if any
    // return 0 if 'Disconnected done', negative number if already disconnected
    // Wait for 'disconnection' event if 0 is returned, Ignore other
    // return-codes
    //
    lRetVal = sl_WlanDisconnect();
    if(0 == lRetVal)
    {
        // Wait
        while(IS_CONNECTED(g_ulStatus))
        {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask();
#endif
        }
    }

    // Enable DHCP client
    lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&ucVal);
    ASSERT_ON_ERROR(__LINE__, lRetVal);

    // Disable scan
    ucConfigOpt = SL_SCAN_POLICY(0);
    lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN , ucConfigOpt, NULL, 0);
    ASSERT_ON_ERROR(__LINE__, lRetVal);

    // Set Tx power level for station mode
    // Number between 0-15, as dB offset from max power - 0 will set maximum
    // power
    ucPower = 0;
    lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, \
    		             WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, \
    		             (unsigned char *)&ucPower);

    ASSERT_ON_ERROR(__LINE__, lRetVal);

    // Set PM policy to normal
    lRetVal = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(__LINE__, lRetVal);

    // Unregister mDNS services
    lRetVal = sl_NetAppMDNSUnRegisterService(0, 0);
    ASSERT_ON_ERROR(__LINE__, lRetVal);

    lRetVal = sl_Stop(SL_STOP_TIMEOUT);
    ASSERT_ON_ERROR(__LINE__, lRetVal);

    InitializeAppVariables();

    return lRetVal; // Success
}

//****************************************************************************
//
//! \brief Advertising the service via mDNS
//!
//! This function advertises a single service on local domain via
//! the native mDNS functions of the CC3200.  The current service
//! is advertised arbirarily as a _uart service.  Though it is
//! used to communicate with the MCU via a TCP server.
//!
//! \param[in] None.
//!
//! \return    None.
//!
//! \note   This function will return immediately.
//

//****************************************************************************
static void mDNS_Task()
{
    int iretvalmDNS;
    unsigned char CustomService1[38]	= "CC3200._uart._tcp.local";

    //Registering for the mDNS service.
	iretvalmDNS = sl_NetAppMDNSRegisterService(CustomService1, strlen(CustomService1), "nada=95", strlen("nada=95"),1010,120,1);
	Report("Register service  %s \n\rstatus %d\n\n",CustomService1,iretvalmDNS);

	if(iretvalmDNS == 0)
	{
		Report("MDNS Registration successful\n\r");
	}
	else
	{
		Report("MDNS Registered failed: %d\n\r", iretvalmDNS);
	}
}

//****************************************************************************
//
//! \brief Opening a TCP server side socket and receiving data
//!
//! This function opens a TCP socket in Listen mode and waits for an incoming
//!    TCP connection.
//! If a socket connection is established then the function will try to read
//!    1000 TCP packets from the connected client.
//!
//! \param[in] port number on which the server will be listening on
//!
//! \return     0 on success, -1 on error.
//!
//! \note   This function will wait for an incoming connection till
//!                     one is established
//

//****************************************************************************
int BsdTcpServer(unsigned short usPort)
{
  SlSockAddrIn_t  sAddr;
  SlSockAddrIn_t  sLocalAddr;
  int             iCounter;
  int             iAddrSize;
  int             iSockID;
  int             iStatus;
  int             iNewSockID;
  long            lLoopCount = 0;
  long            lNonBlocking = 1;
  int             iTestBufLen;


  unsigned long  g_ulPacketCount = 10;

  int i;
  char answer;

  // filling the buffer
  for (iCounter=0 ; iCounter<BUF_SIZE ; iCounter++)
  {
    g_cBsdBuf[iCounter] = (char)(iCounter % 10);
  }

  iTestBufLen  = BUF_SIZE;

  //filling the TCP server socket address
  sLocalAddr.sin_family = SL_AF_INET;
  sLocalAddr.sin_port = sl_Htons((unsigned short)usPort);
  sLocalAddr.sin_addr.s_addr = 0;


  // creating a TCP socket
  iSockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, 0);
  if( iSockID < 0 )
  {
    // error
    return -1;
  }

  iAddrSize = sizeof(SlSockAddrIn_t);

  // binding the TCP socket to the TCP server address
  iStatus = sl_Bind(iSockID, (SlSockAddr_t *)&sLocalAddr, iAddrSize);
  if( iStatus < 0 )
  {
    // error
    sl_Close(iSockID);
    return -1;
  }

  // putting the socket for listening to the incoming TCP connection
  iStatus = sl_Listen(iSockID, 0);
  if( iStatus < 0 )
  {
    sl_Close(iSockID);
    return -1;
  }

  // setting socket option to make the socket as non blocking
  iStatus = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, SL_SO_NONBLOCKING,
                            &lNonBlocking, sizeof(lNonBlocking));
  if( iStatus < 0 )
  {
    return -1;
  }

  while(1)
   {

	  iNewSockID = SL_EAGAIN;
	  // waiting for an incoming TCP connection
	  while( iNewSockID < 0 )
	  {
		// accepts a connection form a TCP client, if there is any
		// otherwise returns SL_EAGAIN
		iNewSockID = sl_Accept(iSockID, ( struct SlSockAddr_t *)&sAddr,
								(SlSocklen_t*)&iAddrSize);
		if( iNewSockID == SL_EAGAIN )
		{
		   MAP_UtilsDelay(10000);
		}
		else if( iNewSockID < 0 )
		{
		  // error
		  sl_Close(iNewSockID);
		  sl_Close(iSockID);
		  Report("Closed on accept.\n\r");
		  return -1;
		}
	  }

	  iStatus = SL_EAGAIN;
	  // waiting for an incoming TCP connection
	  while( iStatus < 0 )
	  {
		// accepts a connection form a TCP client, if there is any
		// otherwise returns SL_EAGAIN
		iStatus = sl_Recv(iNewSockID, g_cBsdBuf, iTestBufLen, 0);
		if( iStatus == SL_EAGAIN )
		{
		   MAP_UtilsDelay(10000);
		}
		else if( iStatus < 0 )
		{
		  // error
		  sl_Close(iNewSockID);
		  sl_Close(iSockID);
		  Report("Closed on read.\n\r");
		  return -1;
		}
	  }
		answer = Interpreter(g_cBsdBuf);
		for(i = 0; g_cBsdBuf[i] != 0; i++)
		{
			Report("%c", g_cBsdBuf[i]);
		}
		Report("\n\r");
		Report("%d\n\r", answer);

		iStatus = sl_Send(iNewSockID, &answer, sizeof(answer), 0 );
		if( iStatus <= 0 )
		{
			// error
			sl_Close(iNewSockID);
			sl_Close(iSockID);
			Report("Closed on send.\n\r");
			return -1;
		}

		lLoopCount++;
	  //}


	  Report("Recieved %u packets successfully\n\r",lLoopCount);

	  iStatus = SL_EAGAIN;
	  // waiting for an incoming TCP connection
	  while( iStatus < 0 )
	  {
		// accepts a connection form a TCP client, if there is any
		// otherwise returns SL_EAGAIN
		iStatus = sl_Recv(iNewSockID, g_cBsdBuf, iTestBufLen, 0);
		if( iStatus == SL_EAGAIN )
		{
		   continue;
		}
		else if( iStatus == 0 )
		{
			sl_Close(iNewSockID);
			Report("Closed that!\n\r");
		}
		else if( iStatus < 0 )
		{
			//error
			sl_Close(iNewSockID);
			sl_Close(iSockID);
			Report("Closed on error final read.\n\r");
			return -1;
		}
	  }
		/*
	  	  iStatus = sl_Recv(iNewSockID, g_cBsdBuf, iTestBufLen, 0);
		if( iStatus <= 0 )
		{
		  // error
		  sl_Close(iNewSockID);
		  Report("Closed that!\n\r");
		}
		*/
   } //while(1)

  // close the listening socket
  sl_Close(iSockID);

  return 0;
}

//*****************************************************************************
//
//! Application startup display on UART
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
static void
DisplayBanner(char * AppName)
{

    Report("\n\n\n\r");
    Report("\t\t *************************************************\n\r");
    Report("\t\t      CC3200 %s Application       \n\r", AppName);
    Report("\t\t *************************************************\n\r");
    Report("\n\n\n\r");
}

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void
BoardInit(void)
{
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
    //
    // Set vector table base
    //
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}

//*****************************************************************************
//
//! \brief Connecting to a WLAN Accesspoint using SmartConfig provisioning
//!
//! Enables SmartConfig provisioning for adding a new connection profile
//! to CC3200. Since we have set the connection policy to Auto, once
//! SmartConfig is complete, CC3200 will connect automatically to the new
//! connection profile added by smartConfig.
//!
//! \param[in] 			        None
//!
//! \return	                    None
//!
//! \note
//!
//! \warning					If the WLAN connection fails or we don't
//!                             acquire an IP address, We will be stuck in this
//!                             function forever.
//
//*****************************************************************************
int SmartConfigConnect()
{
    unsigned char policyVal;
    long lRetVal = -1;

    /* Clear all profiles */
    /* This is of course not a must, it is used in this example to make sure
     * we will connect to the new profile added by SmartConfig
     */
    lRetVal = sl_WlanProfileDel(WLAN_DEL_ALL_PROFILES);

    //set AUTO policy
    lRetVal = sl_WlanPolicySet(  SL_POLICY_CONNECTION,
                      SL_CONNECTION_POLICY(1,0,0,0,1),
                      &policyVal,
                      1 /*PolicyValLen*/);
    ASSERT_ON_ERROR(__LINE__, lRetVal);

    /* Start SmartConfig
     * This example uses the unsecured SmartConfig method
     */

    lRetVal = sl_WlanSmartConfigStart(0,                            //groupIdBitmask
    								  SMART_CONFIG_CIPHER_NONE,    //cipher
                                      0,                           //publicKeyLen
                                      0,                           //group1KeyLen
                                      0,                           //group2KeyLen
                                      NULL,                          //publicKey
                                      NULL,                          //group1Key
                                      NULL);                         //group2Key
    ASSERT_ON_ERROR(__LINE__, lRetVal);

    // Wait for WLAN Event
	while((!IS_CONNECTED(g_ulStatus)) || (!IS_IP_ACQUIRED(g_ulStatus)))
	{
		_SlNonOsMainLoopTask();
	}
     //
     // Turn ON the RED LED to indicate connection success
     //
     GPIO_IF_LedOn(MCU_RED_LED_GPIO);
     //wait for few moments
     MAP_UtilsDelay(80000000);
     //reset to default AUTO policy
     lRetVal = sl_WlanPolicySet(  SL_POLICY_CONNECTION,
                           SL_CONNECTION_POLICY(1,0,0,0,0),
                           &policyVal,
                           1 /*PolicyValLen*/);
     ASSERT_ON_ERROR(__LINE__, lRetVal);

     return SUCCESS;
}



int main(void)

{
	long lRetVal = -1;
    //
    // Initialize Board configurations
    //
    BoardInit();
    //
    // Initialize the uDMA
    //
    UDMAInit();
    //
    // Configure the pinmux settings for the peripherals exercised
    //
    PinMuxConfig();

    #ifndef NOTERM
    	InitTerm();
	#endif

    // configure RED LED
	GPIO_IF_LedConfigure(LED1|LED2|LED3);

	GPIO_IF_LedOff(MCU_ALL_LED_IND);

	//
	// Display banner
	//
	DisplayBanner(APPLICATION_NAME);

	InitializeAppVariables();

	//
   // Following function configure the device to default state by cleaning
   // the persistent settings stored in NVMEM (viz. connection profiles &
   // policies, power policy etc)
   //
   // Applications may choose to skip this step if the developer is sure
   // that the device is in its default state at start of applicaton
   //
   // Note that all profiles and persistent settings that were done on the
   // device will be lost
   //

   lRetVal = ConfigureSimpleLinkToDefaultState();
   if(lRetVal < 0)
   {
	   if (DEVICE_NOT_IN_STATION_MODE == lRetVal)
		   UART_PRINT("Failed to configure the device in its "
				         "default state \n\r");

	   LOOP_FOREVER(__LINE__);
   }

   UART_PRINT("Device is configured in default state \n\r");


   CLR_STATUS_BIT_ALL(g_ulStatus);

   //Start simplelink
   lRetVal = sl_Start(0,0,0);
   if (lRetVal < 0 || ROLE_STA != lRetVal)
   {
	   UART_PRINT("Failed to start the device \n\r");
	   LOOP_FOREVER(__LINE__);
   }

   UART_PRINT("Device started as STATION \n\r");

   /* Connect to our AP using SmartConfig method */
   SmartConfigConnect();

   mDNS_Task();

   BsdTcpServer(1010);


   LOOP_FOREVER(__LINE__);
}

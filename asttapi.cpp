/*
  Name: asttapi.cpp
  Copyright: Under the GNU General Public License Version 2 or later (the "GPL")
  Author: Nick Knight
          Klaus Darilion
  Description:
*/
/* ***** BEGIN LICENSE BLOCK *****
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Asttapi.
 *
 * The Initial Developer of the Original Code is
 * Nick Knight.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Klaus Darilion (enum.at)
 *
 * ***** END LICENSE BLOCK ***** */

// The debugger can't handle symbols more than 255 characters long.
// STL often creates symbols longer than that.
// When symbols are longer than 255 characters, the warning is issued.
#pragma warning(disable:4786)


#include "dll.h"
#include <windows.h>

#include "tapiastmanager.h"
#include "asttspglue.h"
#include <stdio.h>

#include <wchar.h>

#include <atlconv.h>

//For debug
#include "WaveTsp.h"

//our resources
#include "resource.h"

//misc functions
#include "utilities.h"

#include <map>

// #include <boost/regex.hpp>

////////////////////////////////////////////////////////////////////////////////
//
// Globals
//
// Keep track of all the globals we require
//
////////////////////////////////////////////////////////////////////////////////
//   TODO   Need to make these members of a class so that we can have multiple lines.
//Function call for asyncrounous functions
ASYNC_COMPLETION g_pfnCompletionProc = 0;
//LINEEVENT g_pfnEventProc = 0;
//HTAPILINE g_htLine = 0;
//HTAPICALL g_htCall = 0;


DWORD g_dwPermanentProviderID = 0;
DWORD g_dwLineDeviceIDBase = 0;

HPROVIDER g_hProvider = 0;

//For our windows...
HINSTANCE g_hinst = 0;

//Keep track of our lines which we have opened (aka sockets to the server)
typedef std::map<HDRVLINE, tapiAstManager*> mapLine;
mapLine trackLines;
HDRVLINE lastValue = 0;

Mutex lineMut;



// { dwDialPause, dwDialSpeed, dwDigitDuration, dwWaitForDialtone }
LINEDIALPARAMS      g_dpMin = { 100,  50, 100,  100 };
LINEDIALPARAMS      g_dpDef = { 250,  50, 250,  500 };
LINEDIALPARAMS      g_dpMax = { 1000, 50, 1000, 1000 };


////////////////////////////////////////////////////////////////////////////////
// Function DllMain
//
// Dll entry
//
////////////////////////////////////////////////////////////////////////////////
BOOL WINAPI DllMain(
    HINSTANCE   hinst,
    DWORD       dwReason,
    void*      /*pReserved*/)
{
    if( dwReason == DLL_PROCESS_ATTACH )
    {
        g_hinst = hinst;
    }
    
    return TRUE;
}



LONG TSPIAPI TSPI_providerInit(
    DWORD               dwTSPIVersion,
    DWORD               dwPermanentProviderID,
    DWORD               dwLineDeviceIDBase,
    DWORD               dwPhoneDeviceIDBase,
    DWORD_PTR           dwNumLines,
    DWORD_PTR           dwNumPhones,
    ASYNC_COMPLETION    lpfnCompletionProc,
    LPDWORD             lpdwTSPIOptions                         // TSPI v2.0
    )

{
	BEGIN_PARAM_TABLE("TSPI_providerInit")
		DWORD_IN_ENTRY(dwTSPIVersion)
		DWORD_IN_ENTRY(dwPermanentProviderID)
		DWORD_IN_ENTRY(dwLineDeviceIDBase)
		DWORD_IN_ENTRY(dwPhoneDeviceIDBase)
		DWORD_OUT_ENTRY(dwNumLines)
		DWORD_OUT_ENTRY(dwNumPhones)
	END_PARAM_TABLE()

    WSADATA info;
    
	//TSPTRACE("Setting up sockets");

 //   //First off initialize sockets - which we have to do under Win32
 //   if (WSAStartup(MAKELONG(1, 1), &info) == SOCKET_ERROR) {
 //       TSPTRACE("Failed to setup sockets");
 //       WSACleanup();       
 //       return EPILOG(1);
 //   }
	//TSPTRACE("Sockets setup sucsesfully");
    
    //Record all of the globals we need
    g_pfnCompletionProc = lpfnCompletionProc;
    
    //other params we need to track
    g_dwPermanentProviderID = dwPermanentProviderID;
    g_dwLineDeviceIDBase = dwLineDeviceIDBase;
  
    return (0);
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_providerShutdown
//
// Shutdown and clean up.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_providerShutdown(
    DWORD               dwTSPIVersion,
    DWORD               dwPermanentProviderID                   // TSPI v2.0
    )
{
	BEGIN_PARAM_TABLE("TSPI_providerShutdown")
		DWORD_IN_ENTRY(dwTSPIVersion)
		DWORD_IN_ENTRY(dwPermanentProviderID)
	END_PARAM_TABLE()

    //Clean up our sockets
    WSACleanup();
    
    
    return EPILOG(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Capabilities
//
// TAPI will ask us what our capabilities are
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineNegotiateTSPIVersion
//
// 
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineNegotiateTSPIVersion(
	DWORD dwDeviceID,
	DWORD dwLowVersion,
	DWORD dwHighVersion,
	LPDWORD lpdwTSPIVersion)
{
	BEGIN_PARAM_TABLE("TSPI_lineNegotiateTSPIVersion")
		DWORD_IN_ENTRY(dwDeviceID)
		DWORD_IN_ENTRY(dwLowVersion)
		DWORD_IN_ENTRY(dwHighVersion)
		DWORD_OUT_ENTRY(lpdwTSPIVersion)
	END_PARAM_TABLE()

	LONG tr = 0;

	if ( dwLowVersion <= TAPI_CURRENT_VERSION )
	{
#define MIN(a, b) (a < b ? a : b)
		*lpdwTSPIVersion = MIN(TAPI_CURRENT_VERSION,dwHighVersion);
	}
	else
	{
		tr = LINEERR_INCOMPATIBLEAPIVERSION;
	}

	return EPILOG(tr);

}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_providerEnumDevices
//
// 
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_providerEnumDevices(
		DWORD dwPermanentProviderID,
		LPDWORD lpdwNumLines,
		LPDWORD lpdwNumPhones,
		HPROVIDER hProvider,
		LINEEVENT lpfnLineCreateProc,
		PHONEEVENT lpfnPhoneCreateProc)
{
	BEGIN_PARAM_TABLE("TSPI_providerEnumDevices")
		DWORD_IN_ENTRY(dwPermanentProviderID)
		DWORD_OUT_ENTRY(lpdwNumLines)
		DWORD_OUT_ENTRY(lpdwNumPhones)
		DWORD_IN_ENTRY(hProvider)
		DWORD_IN_ENTRY(lpfnLineCreateProc)
		DWORD_IN_ENTRY(lpfnPhoneCreateProc)
	END_PARAM_TABLE()

	g_hProvider = hProvider;
	*lpdwNumLines = 1;
	*lpdwNumPhones = 1;
	return EPILOG(0);
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetDevCaps
//
// Allows TAPI to check our line capabilities before placing a call
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetDevCaps(
    DWORD           dwDeviceID,
    DWORD           dwTSPIVersion,
    DWORD           dwExtVersion,
    LPLINEDEVCAPS   pldc
    )
{
   BEGIN_PARAM_TABLE("TSPI_lineGetDevCaps")
        DWORD_IN_ENTRY(dwDeviceID)
        DWORD_IN_ENTRY(dwTSPIVersion)
        DWORD_IN_ENTRY(dwExtVersion)
        DWORD_IN_ENTRY(pldc)
    END_PARAM_TABLE()

    LONG            tr = 0;
   const wchar_t   szProviderInfo[] = L"ProviderInfo: SIP TAPI for click2dial";
    const wchar_t   szLineName[] = L"LineName: SIP TAPI for click2dial";

    pldc->dwNeededSize = sizeof(LINEDEVCAPS) +
                         sizeof(szProviderInfo) +
                         sizeof(szLineName);
    
    if( pldc->dwNeededSize <= pldc->dwTotalSize )
    {
        pldc->dwUsedSize = pldc->dwNeededSize;

        pldc->dwProviderInfoSize    = sizeof(szProviderInfo);
        pldc->dwProviderInfoOffset  = sizeof(LINEDEVCAPS) + 0;
        wchar_t* pszProviderInfo = (wchar_t*)((BYTE*)pldc + pldc->dwProviderInfoOffset);
        wcscpy(pszProviderInfo, szProviderInfo);
        
        pldc->dwLineNameSize        = sizeof(szLineName);
        pldc->dwLineNameOffset      = sizeof(LINEDEVCAPS) + sizeof(szProviderInfo);
        wchar_t* pszLineName = (wchar_t*)((BYTE*)pldc + pldc->dwLineNameOffset);
        wcscpy(pszLineName, szLineName);
    }
    else
    {
        pldc->dwUsedSize = sizeof(LINEDEVCAPS);
    }
    
    pldc->dwStringFormat      = STRINGFORMAT_ASCII;

// Microsoft recommended algorithm for
// calculating the permanent line ID
#define MAKEPERMLINEID(dwPermProviderID, dwDeviceID) \
    ((LOWORD(dwPermProviderID) << 16) | dwDeviceID)

    pldc->dwPermanentLineID   = MAKEPERMLINEID(g_dwPermanentProviderID, dwDeviceID - g_dwLineDeviceIDBase);
    pldc->dwAddressModes      = LINEADDRESSMODE_ADDRESSID;
    pldc->dwNumAddresses      = 1;
    pldc->dwBearerModes       = LINEBEARERMODE_VOICE;
    pldc->dwMediaModes        = LINEMEDIAMODE_INTERACTIVEVOICE;
    pldc->dwGenerateDigitModes= LINEDIGITMODE_DTMF;
    pldc->dwDevCapFlags       = LINEDEVCAPFLAGS_CLOSEDROP;
    pldc->dwMaxNumActiveCalls = 1;
    pldc->dwLineFeatures      = LINEFEATURE_MAKECALL;

    // DialParams
    pldc->MinDialParams = g_dpMin;
    pldc->MaxDialParams = g_dpMax;
    pldc->DefaultDialParams = g_dpDef;
    
    return EPILOG(tr);
}


////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetAddressCaps
//
// Allows TAPI to check our line capabilities before placing a call
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetAddressCaps(
    DWORD   dwDeviceID,
    DWORD   dwAddressID,
    DWORD   dwTSPIVersion,
    DWORD   dwExtVersion,
    LPLINEADDRESSCAPS  pac)
{
	BEGIN_PARAM_TABLE("TSPI_lineGetAddressCaps")
		DWORD_IN_ENTRY(dwDeviceID)
		DWORD_IN_ENTRY(dwAddressID)
		DWORD_IN_ENTRY(dwTSPIVersion)
		DWORD_OUT_ENTRY(dwExtVersion)
	END_PARAM_TABLE()

    /* TODO (Nick#1#): Most of this function has been taken from an example
                       and will need to be modified in more detail */

    //pac->dwNeededSize = sizeof(LPLINEADDRESSCAPS);
    //pac->dwUsedSize = sizeof(LPLINEADDRESSCAPS);
	pac->dwNeededSize = sizeof(LINEADDRESSCAPS);
    pac->dwUsedSize = sizeof(LINEADDRESSCAPS);
    
    
    pac->dwLineDeviceID = dwDeviceID;
    pac->dwAddressSharing = LINEADDRESSSHARING_PRIVATE;
    pac->dwCallInfoStates = LINECALLINFOSTATE_MEDIAMODE | LINECALLINFOSTATE_APPSPECIFIC;
    pac->dwCallerIDFlags = LINECALLPARTYID_ADDRESS | LINECALLPARTYID_UNKNOWN;
    pac->dwCalledIDFlags = LINECALLPARTYID_ADDRESS | LINECALLPARTYID_UNKNOWN;
    pac->dwRedirectionIDFlags = LINECALLPARTYID_ADDRESS | LINECALLPARTYID_UNKNOWN ;
    pac->dwCallStates = LINECALLSTATE_IDLE | LINECALLSTATE_OFFERING | LINECALLSTATE_ACCEPTED | LINECALLSTATE_DIALING | LINECALLSTATE_CONNECTED;
    
    pac->dwDialToneModes = LINEDIALTONEMODE_UNAVAIL;
    pac->dwBusyModes = LINEDIALTONEMODE_UNAVAIL;
    pac->dwSpecialInfo = LINESPECIALINFO_UNAVAIL;
    
    pac->dwDisconnectModes = LINEDISCONNECTMODE_UNAVAIL;
    
    /* TODO (Nick#1#): This needs to be taken from the UI */
    pac->dwMaxNumActiveCalls = 1;
    
    pac->dwAddrCapFlags = LINEADDRCAPFLAGS_DIALED;
    
    pac->dwCallFeatures = LINECALLFEATURE_DIAL | LINECALLFEATURE_DROP | LINECALLFEATURE_GENERATEDIGITS;
    pac->dwAddressFeatures = LINEADDRFEATURE_MAKECALL | LINEADDRFEATURE_PICKUP;

    return EPILOG(0);
}


////////////////////////////////////////////////////////////////////////////////
//
// Lines
//
// After a suitable line has been found it will be opened with lineOpen
// which TAPI will forward onto TSPI_lineOpen
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Function ThreadProc
//
// this is the thread for a line which listens for messages and processes them
//
////////////////////////////////////////////////////////////////////////////////
DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
	BEGIN_PARAM_TABLE("ThreadProc")
		DWORD_IN_ENTRY(lpParameter)
	END_PARAM_TABLE()

	((tapiAstManager*)lpParameter)->processMessages();

	return EPILOG(0);
}
////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineOpen
//
// This function is typically called where the software needs to reserve some 
// hardware, you can assign any 32bit value to the *phdLine, and it will be sent
// back to any future calls to functions about that line.
//
// Becuase this is sockets not hardware we can set-up the sockets required in this 
// functions, and perhaps get the thread going to read the output of the manager.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineOpen(
    DWORD               dwDeviceID,
    HTAPILINE           htLine,
    LPHDRVLINE          phdLine,
    DWORD               dwTSPIVersion,
    LINEEVENT           pfnEventProc
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineOpen")
		DWORD_IN_ENTRY(dwDeviceID)
		DWORD_IN_ENTRY(htLine)
		DWORD_IN_ENTRY(phdLine)
		DWORD_IN_ENTRY(dwTSPIVersion)
		DWORD_IN_ENTRY(pfnEventProc)
	END_PARAM_TABLE()

//	g_pfnEventProc = pfnEventProc;

/* TODO (Nick#1#): the syncrounous call back needs to be managed 
                   somehow. For the time being we don't use them but in 
                   the future perhaps have a child class to handle this? */

    tapiAstManager *ourConnection;
    ourConnection = new tapiAstManager;
  
    if ( ourConnection )
    {
		std::string strData,strPass,strExten;
		DWORD intData;

		//get our config parameters
		// SIP Proxy
		if ( false == readConfigString("host", strData) ) {
			TspTrace("Error: no SIP proxy configured");
			return EPILOG(LINEERR_CALLUNAVAIL);
		}
        ourConnection->setHost(strData);
		
		// port not used for SIP
		//if ( false == readConfigInt("port", intData) )
		//	return EPILOG(LINEERR_CALLUNAVAIL);
		//ourConnection->setPort(intData);

		TspTrace("Host is '%s'",strData.c_str());
		
/*		if ( false == readConfigString("ochan", strData) )
			return EPILOG(LINEERR_CALLUNAVAIL);
		ourConnection->setOutgoingChannel(strData);
*/

		// SIP user = auth user
		if ( false == readConfigString("user", strData) ) {
			TspTrace("Error: no SIP user configured");
			return EPILOG(LINEERR_CALLUNAVAIL);
		}
		// SIP user's extension (for asterisk dummies)
		if ( false == readConfigString("userexten", strExten) ) {
			TspTrace("Info: no SIP user extension configured, using username...");
			strExten = strData;
		}
		// SIP password
		if ( false == readConfigString("pass", strPass) ) {
			TspTrace("Error: no SIP password");
			return EPILOG(LINEERR_CALLUNAVAIL);
		}
        ourConnection->setUsernamePassword(strData,strExten,strPass);

		// SIP Domain
		if ( false == readConfigString("uchan", strData) ) {
			TspTrace("Error: no SIP user configured");
			return EPILOG(LINEERR_CALLUNAVAIL);
		}
        ourConnection->setOriginator(strData);
		
		// initialize SIP stack
		if ( ourConnection->astConnect() == 0)
		{
			TspTrace("Couldn't initialize SIP stack");
			return EPILOG(LINEERR_CALLUNAVAIL);
		}

/*		if ( false == readConfigString("contextorchan", strData) )
			return EPILOG(LINEERR_CALLUNAVAIL);

		if ( strData == "context" )
		{
			if ( false == readConfigString("context", strData) )
				return EPILOG(LINEERR_CALLUNAVAIL);
			ourConnection->setContext(strData);
		}

		if ( false == readConfigString("setcallerid", strData) )
			return EPILOG(LINEERR_CALLUNAVAIL);

		if ( strData == "true" )
		{
			if ( false == readConfigString("callerid", strData) )
				return EPILOG(LINEERR_CALLUNAVAIL);

			ourConnection->setCallerID(strData);
		}

		if ( false == readConfigString("ichan", strData) )
			return EPILOG(LINEERR_CALLUNAVAIL);

		ourConnection->setInBoundChannel(strData);

		if ( false == readConfigString("ichanregex", strData) )
			return EPILOG(LINEERR_CALLUNAVAIL);

		if ( strData == "true" )
		{
			ourConnection->useInBoundRegex(true);
		}
		else
		{
			ourConnection->useInBoundRegex(false);
		}

*/
		//log the connection in
		ourConnection->login();

		ourConnection->setTapiLine(htLine);
		ourConnection->setLineEvent(pfnEventProc);

		//create the background thread we use to monitor monitor the asterisk manager
		//for events
		HANDLE thrHandle = CreateThread(NULL,0,ThreadProc,ourConnection,0,NULL);

		//keep track of it for our other functions
		lineMut.Lock();
		lastValue++;
		trackLines[lastValue] = ourConnection;
		*phdLine = lastValue;
		lineMut.Unlock();

        //return 0 - signal success
        return EPILOG(0);
    }

    return EPILOG(LINEERR_NOMEM);
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineClose
//
// Called by TAPI when a line is no longer required.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineClose(HDRVLINE hdLine)
{
	BEGIN_PARAM_TABLE("TSPI_lineClose")
		DWORD_IN_ENTRY(hdLine)
	END_PARAM_TABLE()
	//HDRVLINE is a pointer to our asterisk manager object.
	//this should free any resources, such as the socket, 
	

	lineMut.Lock();
	//no find it and remove it from the list
	mapLine::iterator it = trackLines.find(hdLine);
	if ( it != trackLines.end() )
	{
		//clean up

		//which in turn our thread should exit nicley.
		delete ((astManager *)(*it).second);
		trackLines.erase(it);
	}
	lineMut.Unlock();

	

	return EPILOG(0);
}


////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineMakeCall
//
// This function is called by TAPI to initialize a new outgoing call. This will
// initiate the call and return, when the call is made (but not necasarily connected)
// we should then signal TAPI via the asyncrounous completion function.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineMakeCall(
    DRV_REQUESTID       dwRequestID,
    HDRVLINE            hdLine,
    HTAPICALL           htCall,
    LPHDRVCALL          phdCall,
    LPCWSTR             pszDestAddress,
    DWORD               dwCountryCode,
    LPLINECALLPARAMS    const pCallParams
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineMakeCall")
		DWORD_IN_ENTRY(hdLine)
		DWORD_IN_ENTRY(htCall)
		DWORD_IN_ENTRY(phdCall)
		DWORD_IN_ENTRY(dwCountryCode)
//		DWORD_IN_ENTRY(g_pfnEventProc)
//		DWORD_IN_ENTRY(g_pfnCompletionProc)
	END_PARAM_TABLE()

    mbstate_t mbs;
	LONG tr = 0;
    
	lineMut.Lock();
	mapLine::iterator it;
	it = trackLines.find(hdLine);

	if ( it == trackLines.end() )
	{
		lineMut.Unlock();		
		//TODO - more error reporting
		return EPILOG(LINEERR_INVALLINEHANDLE);
	}
    tapiAstManager *ourConnection = (tapiAstManager*) (*it).second;

	// check if call is busy
	if (ourConnection->ongoingcall != 0) {
		// line is busy, return proper error value
		lineMut.Unlock();		
		TspTrace("Line busy ... stop.");
		return EPILOG(LINEERR_INUSE);
	}
	ourConnection->ongoingcall = 4;
	lineMut.Unlock();

	astTspGlue call;
    char charString[100];
    
    //if( mbrlen(pszDestAddress,100) <= 0 ) {
        //what type of length address is that!
    //    return EPILOG(0);
    //}
	if( *pszDestAddress == L'T' || *pszDestAddress == L'P' )
    {
        pszDestAddress++;
    }
    
    mbsinit(&mbs);
    wcsrtombs(&charString[0], &pszDestAddress,100, &mbs);

	if (ourConnection->originate(&charString[0]) == LINEERR_CALLUNAVAIL) {
		TspTrace("Call failed, some SIP problems...? stop.");
		return EPILOG(LINEERR_CALLUNAVAIL);
	}

    
	if ( g_pfnCompletionProc != 0 )
	{
		g_pfnCompletionProc(dwRequestID,0);
		tr = dwRequestID;
	}

	//todo - this needs to be made more interactive - i.e. wait for events from Asterisk?
	//if ( g_pfnEventProc != 0 )
	//{
	//	g_pfnEventProc(g_htLine,htCall,LINE_CALLSTATE,LINECALLSTATE_DIALING,0,0);
	//}
	//g_htCall = htCall;

	call.setTapiCall(htCall);
	call.setDest(&charString[0]);
	ourConnection->addCall(call);
	

	//klaus
	ourConnection->setTapiCall(htCall);
    return EPILOG(tr);
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineDrop
//
// This function is called by TAPI to signal the end of a call. The status 
// information for the call should be retained until the function TSPI_lineCloseCall
// is called.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineDrop(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall,
    LPCSTR              lpsUserInfo,
    DWORD               dwSize
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineDrop")
		DWORD_IN_ENTRY(dwRequestID)
		DWORD_IN_ENTRY(hdCall)
		DWORD_OUT_ENTRY(lpsUserInfo)
		DWORD_IN_ENTRY(dwSize)
	END_PARAM_TABLE()

	lineMut.Lock();

	mapLine::iterator it;

	for ( it = trackLines.begin() ; it != trackLines.end() ; it++ )
	{
		TspTrace("Dropping Call...");
		(*it).second->dropCall(hdCall);
	}

	lineMut.Unlock();


	//if ( g_pfnEventProc != 0 )
	//{
	//	g_pfnEventProc(g_htLine,g_htCall,LINE_CALLSTATE,LINECALLSTATE_IDLE,0,0);
	//}
    
    //Lets pretend to be asyncrounous for the time being!
    //in the furture we should make this so that it waits
    //for asterisk to send its results back to us.
    /* TODO (Nick#1#): Make this part a true asyncronous call */
	if ( g_pfnCompletionProc )
	{
        g_pfnCompletionProc(dwRequestID, 0);
	}
    
    return EPILOG(dwRequestID);
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineCloseCall
//
// This function should deallocate all of the calls resources, TSPI_lineDrop 
// may not be called before this one - so we also have to check the call 
// is dropped as well.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineCloseCall(
    HDRVCALL            hdCall
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineCloseCall")
		DWORD_IN_ENTRY(hdCall)
	END_PARAM_TABLE()

    lineMut.Lock();

	mapLine::iterator it;

	for ( it = trackLines.begin() ; it != trackLines.end() ; it++ )
	{
		(*it).second->dropCall(hdCall);
	}

	lineMut.Unlock();

    return EPILOG(0);
}



////////////////////////////////////////////////////////////////////////////////
//
// Status
//
// if TAPI requires to find out the status of our lines then it can call
// the following functions.
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetLineDevStatus
//
// As the function says.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetLineDevStatus(
    HDRVLINE            hdLine,
    LPLINEDEVSTATUS     plds
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetLineDevStatus")
		DWORD_IN_ENTRY(hdLine)
	END_PARAM_TABLE()

    return EPILOG(0);
}


////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetAdressStatus
//
// As the function says.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPI_lineGetAdressStatus(
    HDRVLINE hdLine,
    DWORD dwAddressID,
    LPLINEADDRESSSTATUS pas)
{
	BEGIN_PARAM_TABLE("TSPI_lineGetAdressStatus")
		DWORD_IN_ENTRY(dwAddressID)
	END_PARAM_TABLE()

    return EPILOG(0);
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetCallStatus
//
// As the function says.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetCallStatus(
    HDRVCALL            hdCall,
    LPLINECALLSTATUS    pls
    )
{
	//TODO finish this offf....
	BEGIN_PARAM_TABLE("TSPI_lineGetCallStatus")
		DWORD_IN_ENTRY(hdCall)
	END_PARAM_TABLE()

	TSPTRACE("TSPI_lineGetCallStatus: ERROR: pls->dwTotalSize      = %d\r\n",pls->dwTotalSize);
	TSPTRACE("TSPI_lineGetCallStatus: ERROR: sizeof(LINECALLSTATUS)= %d\r\n",sizeof(LINECALLSTATUS));

	if (sizeof(LINECALLSTATUS) > pls->dwTotalSize) {
		TSPTRACE("TSPI_lineGetCallStatus: ERROR: sizeof(LINECALLSTATUS) > dwTotalSize\r\n");
		return EPILOG(LINEERR_NOMEM);
	}

	// TAPI Service Provider MUST NOT write this member!
	// http://msdn2.microsoft.com/en-us/library/ms725567.aspx
	//	pls->dwTotalSize = sizeof(LINECALLSTATUS);
	// we use all the fixed size members, thus we need at least the size of the fixed size members
	pls->dwNeededSize = sizeof(LINECALLSTATUS);
	pls->dwUsedSize = sizeof(LINECALLSTATUS);
	pls->dwCallStateMode = 0;

	TSPTRACE("TSPI_lineGetCallStatus: error: pls->dwTotalSize  = %d\r\n",pls->dwTotalSize);
	TSPTRACE("TSPI_lineGetCallStatus: error: pls->dwNeededSize = %d\r\n",pls->dwNeededSize);


	// TAPI Service PRovider MUST NOT write this member!
	// http://msdn2.microsoft.com/en-us/library/ms725567.aspx
	// pls->dwCallPrivilege = LINECALLPRIVILEGE_MONITOR | LINECALLFEATURE_ACCEPT | LINECALLFEATURE_ANSWER | LINECALLFEATURE_COMPLETECALL | LINECALLFEATURE_DIAL | LINECALLFEATURE_DROP;
	//LINECALLPRIVILEGE_NONE 
	//LINECALLPRIVILEGE_OWNER 

	pls->dwCallFeatures = LINECALLFEATURE_ACCEPT;
	//and more...

	lineMut.Lock();
	mapLine::iterator it;
	int found=0;
	for ( it = trackLines.begin() ; it != trackLines.end(); it++ )
	{
		astTspGlue *ourCall;
		if ( ( ourCall = (*it).second->findCall(hdCall)) != NULL )
		{
			TSPTRACE("Found call - getting state\r\n");
			found = 1;
			pls->dwCallState = ourCall->getState();
				//LINECALLSTATE_OFFERING;
				//LINECALLSTATE_CONNECTED;
				//LINECALLSTATE_OFFERING
				//LINECALLSTATE_DISCONNECTED
		}
	}
	lineMut.Unlock();

	if (found) {
		return EPILOG(0);
	} else {
		return EPILOG(LINEERR_INVALCALLHANDLE);
	}
}

//Required (maybe) by lineGetCallInfo
//Thanks to the poster!
//http://groups.google.com/groups?hl=en&lr=&ie=UTF-8&oe=UTF-8&threadm=114501c32a84%24a337f930%24a101280a%40phx.gbl&rnum=3&prev=/groups%3Fq%3DTSPI_lineGetCallInfo%26ie%3DUTF-8%26oe%3DUTF-8%26hl%3Den%26btnG%3DGoogle%2BSearch
void TackOnData(void* pData, const char* pStr, DWORD* pSize)
{
   USES_CONVERSION;

   // Convert the string to Unicode
   LPCWSTR pWStr = A2CW(pStr);

   size_t cbStr = (strlen(pStr) + 1) * 2;
   LPLINECALLINFO pDH = (LPLINECALLINFO)pData;

   // If this isn't an empty string then tack it on
   if (cbStr > 2)
   {
      // Increase the needed size to reflect this string whether we are
      // successful or not.
      pDH->dwNeededSize += cbStr;

      // Do we have space to tack on the string?
      if (pDH->dwTotalSize >= pDH->dwUsedSize + cbStr)
      {
         // YES, tack it on
         memcpy((char *)pDH + pDH->dwUsedSize, pWStr, cbStr);

         // Now adjust size and offset in message and used 
		 // size in the header
         DWORD* pOffset = pSize + 1;

         *pSize   = cbStr;
         *pOffset = pDH->dwUsedSize;
         pDH->dwUsedSize += cbStr;
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetCallInfo
//
// As the function says.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetCallInfo(
    HDRVCALL            hdCall,
    LPLINECALLINFO      lpCallInfo
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetCallInfo")
		DWORD_IN_ENTRY(hdCall)
	END_PARAM_TABLE()

	//Some useful information taken from a newsgroup cutting I found - it's ok for
	//the SP to fill in as much info as it can, set dwNeededSize apprpriately, &
	//return success.  (The app might not care about the var-length fields;
	//otherwise it'll see dwNeededSized, realloc a bigger buf, and retry)

	//These are the items that a TSP is required to fill out - other items we must preserve
	// - that is they are used by TAPI.

	lpCallInfo->dwNeededSize = sizeof (LINECALLINFO);
	lpCallInfo->dwUsedSize = sizeof (LINECALLINFO);
	lpCallInfo->dwLineDeviceID = 0;  //at the mo we don't have any concept of more than one line - perhaps should to though!
	lpCallInfo->dwAddressID = 0;
	lpCallInfo->dwBearerMode = LINEBEARERMODE_SPEECH;
	//lpCallInfo->dwRate;
	//lpCallInfo->dwMediaMode;
	//lpCallInfo->dwAppSpecific;
	//lpCallInfo->dwCallID;
	//lpCallInfo->dwRelatedCallID;
	//lpCallInfo->dwCallParamFlags;
	//lpCallInfo->DialParams;
	lpCallInfo->dwOrigin = LINECALLORIGIN_INBOUND;
	lpCallInfo->dwReason = LINECALLREASON_DIRECT;  //one of LINECALLORIGIN_ Constants
	lpCallInfo->dwCompletionID = 0;
	lpCallInfo->dwCountryCode = 0;   // 0 = unknown
	lpCallInfo->dwTrunk = 0xFFFFFFFF; // 0xFFFFFFFF = unknown
	lpCallInfo->dwCallerIDFlags = 0;   //or LINECALLPARTYID_NAME | LINECALLPARTYID_ADDRESS; once we know we have the caller ID
	
	lpCallInfo->dwConnectedIDFlags = 0;
	lpCallInfo->dwConnectedIDSize = 0;
	lpCallInfo->dwConnectedIDOffset = 0;
	lpCallInfo->dwConnectedIDNameSize = 0;
	lpCallInfo->dwConnectedIDNameOffset = 0;


	// For the next section we need to find our call relating to this.
	lpCallInfo->dwCallerIDOffset = 0;
	lpCallInfo->dwCallerIDSize = 0;

	mapLine::iterator it;
	bool foundCall = false;
	lineMut.Lock();
	for ( it = trackLines.begin() ; it != trackLines.end(); it++ )
	{
		astTspGlue *ourCall;
		if ( ( ourCall = (*it).second->findCall(hdCall)) != NULL )
		{
			lpCallInfo->dwCallerIDFlags = 0;
			std::string callerID = ourCall->getCallerID();
			if ( callerID != "" )
			{
				TSPTRACE("inserting callerID");
				TackOnData(lpCallInfo, callerID.c_str() ,&lpCallInfo->dwCallerIDSize);

				//This needs be calculated carefully! - the main reason for this driver!
				//lpCallInfo->dwCallerIDSize = 6;
				lpCallInfo->dwCallerIDOffset = lpCallInfo->dwUsedSize; //This is where we place the caller ID info
				//lpCallInfo->dwCallerIDNameSize;
				//lpCallInfo->dwCallerIDNameOffset;
				//we can also call the following function on caller ID name etc
				TackOnData(lpCallInfo, ourCall->getCallerID().c_str() ,&lpCallInfo->dwCallerIDSize);
				lpCallInfo->dwCallerIDFlags |= LINECALLPARTYID_ADDRESS;

			}

			std::string callerName(ourCall->getCallerName());
			if ( callerName != "" )
			{
				TSPTRACE("inserting callerName");
				TackOnData(lpCallInfo, callerName.c_str() ,&lpCallInfo->dwCallerIDNameSize);

				lpCallInfo->dwCallerIDNameOffset = lpCallInfo->dwUsedSize;

				TackOnData(lpCallInfo, callerName.c_str() ,&lpCallInfo->dwCallerIDNameSize);
				lpCallInfo->dwCallerIDFlags |= LINECALLPARTYID_NAME;
			}
			//insert the called party (if outgoing?)
			//CalledID - do we even need to bother?
			std::string peerAddress;
			if ( ( peerAddress = ourCall->getPeerAddress() ) != "" )
			{
				TSPTRACE("Inserting peeraddress: %s ", peerAddress.c_str());
				lpCallInfo->dwOrigin = LINECALLORIGIN_OUTBOUND;
				lpCallInfo->dwReason = LINECALLREASON_DIRECT;
				lpCallInfo->dwCalledIDFlags = LINECALLPARTYID_ADDRESS;
				lpCallInfo->dwCalledIDOffset = lpCallInfo->dwUsedSize;

				TackOnData(lpCallInfo, peerAddress.c_str() ,&lpCallInfo->dwCalledIDSize);

				//lpCallInfo->dwCalledIDOffset;
				//lpCallInfo->dwCalledIDNameSize;
				//lpCallInfo->dwCalledIDNameOffset;
			}

			if ( ourCall->getState() == callStIncomingConnected )
			{
				TSPTRACE("inserting callerID on connected");
				std::string callerID = ourCall->getCallerID();
				TackOnData(lpCallInfo, callerID.c_str() ,&lpCallInfo->dwConnectedIDSize);

				//This needs be calculated carefully! - the main reason for this driver!
				lpCallInfo->dwConnectedIDOffset = lpCallInfo->dwUsedSize; //This is where we place the caller ID info
				TackOnData(lpCallInfo, ourCall->getCallerID().c_str() ,&lpCallInfo->dwCallerIDSize);
				lpCallInfo->dwConnectedIDFlags |= LINECALLPARTYID_ADDRESS;

				std::string peerAddress;
				if ( ( peerAddress = ourCall->getPeerAddress() ) != "" )
				{
					TSPTRACE("Inserting peeraddress: %s ", peerAddress.c_str());
					lpCallInfo->dwConnectedIDFlags = LINECALLPARTYID_ADDRESS;
					lpCallInfo->dwConnectedIDNameOffset = lpCallInfo->dwUsedSize;
					TackOnData(lpCallInfo, peerAddress.c_str() ,&lpCallInfo->dwConnectedIDNameSize);
				}
			}

			break;
		}
	}
	lineMut.Unlock();

	

	//lpCallInfo->dwRedirectionIDFlags;
	//lpCallInfo->dwRedirectionIDSize;
	//lpCallInfo->dwRedirectionIDOffset;
	//lpCallInfo->dwRedirectionIDNameSize;
	//lpCallInfo->dwRedirectionIDNameOffset;
	//lpCallInfo->dwRedirectingIDFlags;
	//lpCallInfo->dwRedirectingIDSize;
	//lpCallInfo->dwRedirectingIDOffset;
	//lpCallInfo->dwRedirectingIDNameSize;
	//lpCallInfo->dwRedirectingIDNameOffset;
	//lpCallInfo->dwDisplaySize;
	//lpCallInfo->dwDisplayOffset;
	//lpCallInfo->dwUserUserInfoSize;
	//lpCallInfo->dwUserUserInfoOffset;
	//lpCallInfo->dwHighLevelCompSize;
	//lpCallInfo->dwHighLevelCompOffset;
	//lpCallInfo->dwLowLevelCompSize;
	//lpCallInfo->dwLowLevelCompOffset;
	//lpCallInfo->dwChargingInfoSize;
	//lpCallInfo->dwChargingInfoOffset;
	//lpCallInfo->dwTerminalModesSize;
	//lpCallInfo->dwTerminalModesOffset;
	//lpCallInfo->dwDevSpecificSize;
	//lpCallInfo->dwDevSpecificOffset;

	return EPILOG(0);
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_lineGetCallAddressID
//
// As the function says.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_lineGetCallAddressID(
    HDRVCALL            hdCall,
    LPDWORD             pdwAddressID
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetCallAddressID")
		DWORD_IN_ENTRY(hdCall)
	END_PARAM_TABLE()

    return EPILOG(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Installation, removal and configuration
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Function TUISPI_providerInstall
//
// Called by TAPI on installation, ideal oppertunity to gather information
// via. some form of user interface.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TUISPI_providerInstall(
    TUISPIDLLCALLBACK pfnUIDLLCallback,
    HWND hwndOwner,
    DWORD dwPermanentProviderID)
{
	BEGIN_PARAM_TABLE("TUISPI_providerInstall")
		DWORD_IN_ENTRY(dwPermanentProviderID)
	END_PARAM_TABLE()

	initConfigStore();

	std::string strData;
	if( false == readConfigString("host", strData) )
	{
		storeConfigString("host", "");
		storeConfigInt("port", 5038);
		storeConfigString("user", "");
		storeConfigString("pass", "");
		storeConfigString("ochan", "");
		storeConfigString("uchan", "");
		storeConfigString("ichan", "");
	}

	MessageBox(hwndOwner, "SIP TAPI Service Provider installed",
        "TAPI Installation", MB_OK);

    return EPILOG(0);
}

LONG
TSPIAPI
TSPI_providerInstall(
    HWND                hwndOwner,
    DWORD               dwPermanentProviderID
    )
{
	// Although this func is never called by TAPI v2.0, we export
    // it so that the Telephony Control Panel Applet knows that it
    // can add this provider via lineAddProvider(), otherwise
    // Telephon.cpl will not consider it installable

    BEGIN_PARAM_TABLE("TSPI_providerInstall")
        DWORD_IN_ENTRY(hwndOwner)
        DWORD_IN_ENTRY(dwPermanentProviderID)
    END_PARAM_TABLE()

	return EPILOG(0);
}


////////////////////////////////////////////////////////////////////////////////
// Function TUISPI_providerRemove
//
// When TAPI is requested to remove the TSP via the call lineRemoveProvider.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TUISPI_providerRemove(
    TUISPIDLLCALLBACK pfnUIDLLCallback,
    HWND hwndOwner,
    DWORD dwPermanentProviderID)
{
	BEGIN_PARAM_TABLE("TUISPI_providerRemove")
		DWORD_IN_ENTRY(dwPermanentProviderID)
	END_PARAM_TABLE()

    return EPILOG(0);
}
LONG TSPIAPI TSPI_providerRemove(
    HWND    hwndOwner,
    DWORD   dwPermanentProviderID)
{
    // Although this func is never called by TAPI v2.0, we export
    // it so that the Telephony Control Panel Applet knows that it
    // can remove this provider via lineRemoveProvider(), otherwise
    // Telephon.cpl will not consider it removable
    BEGIN_PARAM_TABLE("TSPI_providerRemove")
        DWORD_IN_ENTRY(hwndOwner)
        DWORD_IN_ENTRY(dwPermanentProviderID)
    END_PARAM_TABLE()

    return EPILOG(0);
}
////////////////////////////////////////////////////////////////////////////////
// Function TUISPI_providerRemove
//
// When TAPI is requested to remove the TSP via the call lineRemoveProvider.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TUISPI_providerUIIdentify(
    TUISPIDLLCALLBACK pfnUIDLLCallback,
    LPWSTR pszUIDLLName)
{
	BEGIN_PARAM_TABLE("TUISPI_providerUIIdentify")
		STRING_IN_ENTRY(pszUIDLLName)
	END_PARAM_TABLE()

	return EPILOG(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Dialog config area
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Function ConfigDlgProc
//
// The callback function for our dialog box.
//
////////////////////////////////////////////////////////////////////////////////
BOOL CALLBACK ConfigDlgProc(
    HWND    hwnd,
    UINT    nMsg,
    WPARAM  wparam,
    LPARAM  lparam)
{
    BOOL    b = FALSE;
	char szTemp[256];
	std::string temp;
	DWORD tempInt;

    switch( nMsg )
    {
    case WM_INITDIALOG:
        //CenterWindow(hwnd);
		readConfigString("host", temp);
        SetDlgItemText(hwnd, IDC_HOST, temp.c_str());
		readConfigInt("port", tempInt);
		SetDlgItemInt(hwnd, IDC_PORT, tempInt,FALSE);
		readConfigString("user", temp);
		SetDlgItemText(hwnd, IDC_USER, temp.c_str());
		readConfigString("userexten", temp);
		SetDlgItemText(hwnd, IDC_USER_EXTEN, temp.c_str());
		readConfigString("pass", temp);
		SetDlgItemText(hwnd, IDC_PASS, temp.c_str());

		readConfigString("ochan", temp);
		SetDlgItemText(hwnd, IDC_OCHAN, temp.c_str());
		//User channel
		readConfigString("ichan", temp);
		SetDlgItemText(hwnd, IDC_ICHAN, temp.c_str());
		//inbound channel
		readConfigString("ichanregex", temp);
		if ( temp == "true" )
		{
			CheckDlgButton(hwnd, IDC_ICHREGEX,BST_CHECKED);
		}

		readConfigString("uchan", temp);
		SetDlgItemText(hwnd, IDC_UCHAN, temp.c_str());

		//CallerID
		readConfigString("callerid", temp);
		SetDlgItemText(hwnd, IDC_CALLERID, temp.c_str() );

		readConfigString("setcallerid", temp);
		if ( temp == "true" )
		{
			CheckDlgButton(hwnd, IDC_CALLERIDEN,BST_CHECKED);
		}

		readConfigString("context", temp);
		SetDlgItemText(hwnd, IDC_CONTEXT, temp.c_str() );

		readConfigString("contextorchan", temp);

		if ( temp == "context" )
		{
			CheckRadioButton(hwnd,IDC_RADIO1, IDC_RADIO2, IDC_RADIO2);
		}
		else
		{
			CheckRadioButton(hwnd,IDC_RADIO1, IDC_RADIO2, IDC_RADIO1);
		}

        b = TRUE;
    break;

    case WM_COMMAND:
        switch( wparam )
        {
			        
		case IDOK:
			if ( TRUE == IsDlgButtonChecked(hwnd, IDC_ICHREGEX) )
			{
				try
				{
					GetDlgItemText(hwnd, IDC_ICHAN, szTemp, sizeof(szTemp));
//					boost::regex e(szTemp);
				}
				catch(std::exception e)
				{
					//if we get here the regex is invalid
					MessageBox(hwnd, "Invalid Regular Exression", "Problem", 0);
					return b;
				}
			}

            EndDialog(hwnd, IDOK);
			//plus it will now go onto apply...
		
		case IDC_APPLY:

			if ( TRUE == IsDlgButtonChecked(hwnd, IDC_ICHREGEX) )
			{
				try
				{
					GetDlgItemText(hwnd, IDC_ICHAN, szTemp, sizeof(szTemp));
//					boost::regex e(szTemp);
				}
				catch(std::exception e)
				{
					//if we get here the regex is invalid
					MessageBox(hwnd, "Invalid Regular Exression", "Problem", 0);
					return b;
				}
			}
			
			GetDlgItemText(hwnd, IDC_HOST, szTemp, sizeof(szTemp));
			storeConfigString("host", szTemp);

			tempInt = GetDlgItemInt(hwnd, IDC_PORT, NULL, FALSE);
			storeConfigInt("port", tempInt);

			GetDlgItemText(hwnd, IDC_USER, szTemp, sizeof(szTemp));
			storeConfigString("user", szTemp);
			
			GetDlgItemText(hwnd, IDC_USER_EXTEN, szTemp, sizeof(szTemp));
			storeConfigString("userexten", szTemp);
			
			GetDlgItemText(hwnd, IDC_PASS, szTemp, sizeof(szTemp));
			storeConfigString("pass", szTemp);

			GetDlgItemText(hwnd, IDC_OCHAN, szTemp, sizeof(szTemp));
			storeConfigString("ochan", szTemp);

			GetDlgItemText(hwnd, IDC_ICHAN, szTemp, sizeof(szTemp));
			storeConfigString("ichan", szTemp);

			//store the state of wether we use regular expresion matching
			//for the inbound channel.
			if ( TRUE == IsDlgButtonChecked(hwnd, IDC_ICHREGEX) )
			{
				storeConfigString("ichanregex", "true");
			}
			else
			{
				storeConfigString("ichanregex", "false");
			}


			GetDlgItemText(hwnd, IDC_UCHAN, szTemp, sizeof(szTemp));
			storeConfigString("uchan", szTemp);

			//wether we origintae by call the dial application or 
			//by dropping a user into a context
			if ( TRUE == IsDlgButtonChecked(hwnd, IDC_RADIO1) )
			{
				storeConfigString("contextorchan", "chan");
			}
			else
			{
				storeConfigString("contextorchan", "context");
			}

			//what context?
			GetDlgItemText(hwnd, IDC_CONTEXT, szTemp, sizeof(szTemp));
			storeConfigString("context", szTemp);

			//Do we attempt to set the caller ID when we place a call?
			if ( TRUE == IsDlgButtonChecked(hwnd, IDC_CALLERIDEN) )
			{
				storeConfigString("setcallerid", "true");
			}
			else
			{
				storeConfigString("setcallerid", "false");
			}

			//Caller ID string
			GetDlgItemText(hwnd, IDC_CALLERID, szTemp, sizeof(szTemp));
			storeConfigString("callerid", szTemp);

			GetDlgItemText(hwnd, IDC_CONTEXT, szTemp, sizeof(szTemp));
			storeConfigString("context", szTemp);

			

			b= TRUE;
			break;

        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
        break;

//        case IDC_TEST:
//        {
 //           char    szDtmf[256];
            //GetDlgItemText(hwnd, IDC_DTMF, szDtmf, DIM(szDtmf));
            //if( *szDtmf )
            //{
                //CtDialString    ds(0, szDtmf);
                //ds.Dial(WAVE_MAPPER, 250, 0);
//            MessageBox(0, "Playing test digits...", "WaveTSP", MB_SETFOREGROUND);
            //}
//        }
        break;
        }
    break;
    }

    return b;
}

////////////////////////////////////////////////////////////////////////////////
// Function TUISPI_lineConfigDialog
//
// Called when a request for config is made upon us.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TUISPI_lineConfigDialog(                                        // TSPI v2.0
    TUISPIDLLCALLBACK   lpfnUIDLLCallback,
    DWORD               dwDeviceID,
    HWND                hwndOwner,
    LPCWSTR             lpszDeviceClass
    )
{
	BEGIN_PARAM_TABLE("TUISPI_lineConfigDialog")
		DWORD_IN_ENTRY(lpfnUIDLLCallback)
		DWORD_IN_ENTRY(dwDeviceID)
		DWORD_IN_ENTRY(hwndOwner)
		STRING_IN_ENTRY(lpszDeviceClass)
	END_PARAM_TABLE()

	DialogBox(g_hinst,
        MAKEINTRESOURCE(IDD_DIALOG1),
        hwndOwner,
        ConfigDlgProc);

	return EPILOG(0);
}

////////////////////////////////////////////////////////////////////////////////
// Function TSPI_providerUIIdentify
//
// Becuase the UI part of a TSP can exsist in another DLL we need to tell TAPI
// that this is the DLL which provides the UI functionality as well.
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TSPI_providerUIIdentify(                                        // TSPI v2.0
        LPWSTR              lpszUIDLLName
    )
{
	BEGIN_PARAM_TABLE("TSPI_providerUIIdentify")
		STRING_IN_ENTRY(lpszUIDLLName)
	END_PARAM_TABLE()

	char szPath[MAX_PATH+1];
	GetModuleFileNameA(g_hinst,szPath,MAX_PATH);
	mbstowcs(lpszUIDLLName,szPath,strlen(szPath)+1);

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_providerGenericDialogData(                                 // TSPI v2.0
    DWORD_PTR           dwObjectID,
    DWORD               dwObjectType,
    LPVOID              lpParams,
    DWORD               dwSize
    )
{
	BEGIN_PARAM_TABLE("TSPI_providerGenericDialogData")
		DWORD_IN_ENTRY(dwObjectID)
		DWORD_IN_ENTRY(dwObjectType)
		DWORD_IN_ENTRY(lpParams)
		DWORD_IN_ENTRY(dwSize)
	END_PARAM_TABLE()

	return EPILOG(0);
}
////////////////////////////////////////////////////////////////////////////////
// Function TUISPI_providerConfig
//
// Obsolete - only in TAPI version 1.4 and below
//
////////////////////////////////////////////////////////////////////////////////
LONG TSPIAPI TUISPI_providerConfig(
    TUISPIDLLCALLBACK pfnUIDLLCallback,
    HWND hwndOwner,
    DWORD dwPermanentProviderID)
{
	BEGIN_PARAM_TABLE("TUISPI_providerConfig")
		DWORD_IN_ENTRY(dwPermanentProviderID)
	END_PARAM_TABLE()

	DialogBox(g_hinst,
        MAKEINTRESOURCE(IDD_DIALOG1),
        hwndOwner,
        ConfigDlgProc);

    return EPILOG(0);
}

///////////////////Start////////////////////
LONG
TSPIAPI
TSPI_lineAccept(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall,
    LPCSTR              lpsUserUserInfo,
    DWORD               dwSize
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineAccept")
		DWORD_IN_ENTRY(dwRequestID)
	END_PARAM_TABLE()
	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineAddToConference(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdConfCall,
    HDRVCALL            hdConsultCall
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineAddToConference")
		DWORD_IN_ENTRY(dwRequestID)
	END_PARAM_TABLE()
	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineAnswer(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall,
    LPCSTR              lpsUserUserInfo,
    DWORD               dwSize
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineAnswer")
		DWORD_IN_ENTRY(dwSize)
	END_PARAM_TABLE()
	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineBlindTransfer(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall,
    LPCWSTR             lpszDestAddress,
    DWORD               dwCountryCode)
{
	BEGIN_PARAM_TABLE("TSPI_lineBlindTransfer")
		DWORD_IN_ENTRY(dwRequestID)
	END_PARAM_TABLE()
	return EPILOG(0);
}





LONG
TSPIAPI
TSPI_lineCompleteCall(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall,
    LPDWORD             lpdwCompletionID,
    DWORD               dwCompletionMode,
    DWORD               dwMessageID
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineCompleteCall")
		DWORD_IN_ENTRY(dwCompletionMode)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineCompleteTransfer(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall,
    HDRVCALL            hdConsultCall,
    HTAPICALL           htConfCall,
    LPHDRVCALL          lphdConfCall,
    DWORD               dwTransferMode
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineCompleteTransfer")
		DWORD_IN_ENTRY(dwRequestID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineConditionalMediaDetection(
    HDRVLINE            hdLine,
    DWORD               dwMediaModes,
    LPLINECALLPARAMS    const lpCallParams
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineConditionalMediaDetection")
		DWORD_IN_ENTRY(dwMediaModes)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineDevSpecific(
    DRV_REQUESTID       dwRequestID,
    HDRVLINE            hdLine,
    DWORD               dwAddressID,
    HDRVCALL            hdCall,
    LPVOID              lpParams,
    DWORD               dwSize
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineDevSpecific")
		DWORD_IN_ENTRY(dwAddressID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineDevSpecificFeature(
    DRV_REQUESTID       dwRequestID,
    HDRVLINE            hdLine,
    DWORD               dwFeature,
    LPVOID              lpParams,
    DWORD               dwSize
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineDevSpecificFeature")
		DWORD_IN_ENTRY(dwRequestID)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineDial(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall,
    LPCWSTR             lpszDestAddress,
    DWORD               dwCountryCode
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineDial")
		DWORD_IN_ENTRY(dwRequestID)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineDropOnClose(                                           // TSPI v1.4
    HDRVCALL            hdCall
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineDropOnClose")
		DWORD_IN_ENTRY(hdCall)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineDropNoOwner(                                           // TSPI v1.4
    HDRVCALL            hdCall
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineDropNoOwner")
		DWORD_IN_ENTRY(hdCall)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineForward(
    DRV_REQUESTID       dwRequestID,
    HDRVLINE            hdLine,
    DWORD               bAllAddresses,
    DWORD               dwAddressID,
    LPLINEFORWARDLIST   const lpForwardList,
    DWORD               dwNumRingsNoAnswer,
    HTAPICALL           htConsultCall,
    LPHDRVCALL          lphdConsultCall,
    LPLINECALLPARAMS    const lpCallParams
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineForward")
		DWORD_IN_ENTRY(bAllAddresses)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineGatherDigits(
    HDRVCALL            hdCall,
    DWORD               dwEndToEndID,
    DWORD               dwDigitModes,
    LPWSTR              lpsDigits,
    DWORD               dwNumDigits,
    LPCWSTR             lpszTerminationDigits,
    DWORD               dwFirstDigitTimeout,
    DWORD               dwInterDigitTimeout
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineGatherDigits")
		DWORD_IN_ENTRY(dwEndToEndID)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineGenerateDigits(
    HDRVCALL            hdCall,
    DWORD               dwEndToEndID,
    DWORD               dwDigitMode,
    LPCWSTR             lpszDigits,
    DWORD               dwDuration
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineGenerateDigits")
		DWORD_IN_ENTRY(dwEndToEndID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineGenerateTone(
    HDRVCALL            hdCall,
    DWORD               dwEndToEndID,
    DWORD               dwToneMode,
    DWORD               dwDuration,
    DWORD               dwNumTones,
    LPLINEGENERATETONE  const lpTones
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineGenerateTone")
		DWORD_IN_ENTRY(dwEndToEndID)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineGetAddressID(
    HDRVLINE            hdLine,
    LPDWORD             lpdwAddressID,
    DWORD               dwAddressMode,
    LPCWSTR             lpsAddress,
    DWORD               dwSize
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetAddressID")
		DWORD_IN_ENTRY(dwAddressMode)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineGetAddressStatus(
    HDRVLINE            hdLine,
    DWORD               dwAddressID,
    LPLINEADDRESSSTATUS lpAddressStatus
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetAddressStatus")
		DWORD_IN_ENTRY(dwAddressID)
	END_PARAM_TABLE()
	//This function can be expanded when we impliment
	//parking, transfer etc

	lpAddressStatus->dwTotalSize = sizeof(LINEADDRESSSTATUS);  
	lpAddressStatus->dwNeededSize = sizeof(LINEADDRESSSTATUS); 


	lineMut.Lock();
	mapLine::iterator it = trackLines.find(hdLine);
	if ( it != trackLines.end() )
	{
		//DWORD dwUsedSize;  
		lpAddressStatus->dwNumInUse = 1;  
		lpAddressStatus->dwNumActiveCalls = (*it).second->getNumCalls();  
		lpAddressStatus->dwNumOnHoldCalls = 0;  
		lpAddressStatus->dwNumOnHoldPendCalls = 0;  
		//DWORD dwAddressFeatures;  
		//DWORD dwNumRingsNoAnswer;  
		//DWORD dwForwardNumEntries;  
		//DWORD dwForwardSize; 
		//DWORD dwForwardOffset;  
		//DWORD dwTerminalModesSize;  
		//DWORD dwTerminalModesOffset;  
		//DWORD dwDevSpecificSize;  
		//DWORD dwDevSpecificOffset;
	}
	lineMut.Unlock();

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineGetDevConfig(
    DWORD               dwDeviceID,
    LPVARSTRING         lpDeviceConfig,
    LPCWSTR             lpszDeviceClass
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetDevConfig")
		DWORD_IN_ENTRY(dwDeviceID)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineGetExtensionID(
    DWORD               dwDeviceID,
    DWORD               dwTSPIVersion,
    LPLINEEXTENSIONID   lpExtensionID
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetExtensionID")
		DWORD_IN_ENTRY(dwDeviceID)
	END_PARAM_TABLE()

	lpExtensionID->dwExtensionID0 = 0;
	lpExtensionID->dwExtensionID1 = 0;
	lpExtensionID->dwExtensionID2 = 0;
	lpExtensionID->dwExtensionID3 = 0;

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineGetIcon(
    DWORD               dwDeviceID,
    LPCWSTR             lpszDeviceClass,
    LPHICON             lphIcon
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetIcon")
		DWORD_IN_ENTRY(dwDeviceID)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineGetID(
    HDRVLINE            hdLine,
    DWORD               dwAddressID,
    HDRVCALL            hdCall,
    DWORD               dwSelect,
    LPVARSTRING         lpDeviceID,
    LPCWSTR             lpszDeviceClass,
    HANDLE              hTargetProcess                          // TSPI v2.0
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetID")
		DWORD_IN_ENTRY(dwSelect)
	END_PARAM_TABLE()

	return EPILOG(0);
}



LONG
TSPIAPI
TSPI_lineGetNumAddressIDs(
    HDRVLINE            hdLine,
    LPDWORD             lpdwNumAddressIDs
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineGetNumAddressIDs")
		DWORD_IN_ENTRY(hdLine)
	END_PARAM_TABLE()

	*lpdwNumAddressIDs = 1;

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineHold(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineHold")
		DWORD_IN_ENTRY(dwRequestID)
	END_PARAM_TABLE()

	return EPILOG(0);
}



LONG
TSPIAPI
TSPI_lineMonitorDigits(
    HDRVCALL            hdCall,
    DWORD               dwDigitModes
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineMonitorDigits")
		DWORD_IN_ENTRY(dwDigitModes)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineMonitorMedia(
    HDRVCALL            hdCall,
    DWORD               dwMediaModes
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineMonitorMedia")
		DWORD_IN_ENTRY(dwMediaModes)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineMonitorTones(
    HDRVCALL            hdCall,
    DWORD               dwToneListID,
    LPLINEMONITORTONE   const lpToneList,
    DWORD               dwNumEntries
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineMonitorTones")
		DWORD_IN_ENTRY(dwToneListID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineNegotiateExtVersion(
    DWORD               dwDeviceID,
    DWORD               dwTSPIVersion,
    DWORD               dwLowVersion,
    DWORD               dwHighVersion,
    LPDWORD             lpdwExtVersion
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineNegotiateExtVersion")
		DWORD_IN_ENTRY(dwDeviceID)
	END_PARAM_TABLE()

	return EPILOG(0);
}



LONG
TSPIAPI
TSPI_linePark(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall,
    DWORD               dwParkMode,
    LPCWSTR             lpszDirAddress,
    LPVARSTRING         lpNonDirAddress
    )
{
	BEGIN_PARAM_TABLE("TSPI_linePark")
		DWORD_IN_ENTRY(dwParkMode)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_linePickup(
    DRV_REQUESTID       dwRequestID,
    HDRVLINE            hdLine,
    DWORD               dwAddressID,
    HTAPICALL           htCall,
    LPHDRVCALL          lphdCall,
    LPCWSTR             lpszDestAddress,
    LPCWSTR             lpszGroupID
    )
{
	BEGIN_PARAM_TABLE("TSPI_linePickup")
		DWORD_IN_ENTRY(dwAddressID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_linePrepareAddToConference(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdConfCall,
    HTAPICALL           htConsultCall,
    LPHDRVCALL          lphdConsultCall,
    LPLINECALLPARAMS    const lpCallParams
    )
{
	BEGIN_PARAM_TABLE("TSPI_linePrepareAddToConference")
		DWORD_IN_ENTRY(dwRequestID)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineRedirect(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall,
    LPCWSTR             lpszDestAddress,
    DWORD               dwCountryCode
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineRedirect")
		DWORD_IN_ENTRY(dwCountryCode)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineReleaseUserUserInfo(                                   // TSPI v1.4
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineReleaseUserUserInfo")
		DWORD_IN_ENTRY(dwRequestID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineRemoveFromConference(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineRemoveFromConference")
		DWORD_IN_ENTRY(dwRequestID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSecureCall(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSecureCall")
		DWORD_IN_ENTRY(hdCall)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSelectExtVersion(
    HDRVLINE            hdLine,
    DWORD               dwExtVersion
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSelectExtVersion")
		DWORD_IN_ENTRY(dwExtVersion)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSendUserUserInfo(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall,
    LPCSTR              lpsUserUserInfo,
    DWORD               dwSize
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSendUserUserInfo")
		DWORD_IN_ENTRY(dwSize)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSetAppSpecific(
    HDRVCALL            hdCall,
    DWORD               dwAppSpecific
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetAppSpecific")
		DWORD_IN_ENTRY(dwAppSpecific)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineSetCallData(                                           // TSPI v2.0
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall,
    LPVOID              lpCallData,
    DWORD               dwSize
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetCallData")
		DWORD_IN_ENTRY(dwSize)
	END_PARAM_TABLE()
	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineSetCallParams(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall,
    DWORD               dwBearerMode,
    DWORD               dwMinRate,
    DWORD               dwMaxRate,
    LPLINEDIALPARAMS    const lpDialParams
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetCallParams")
		DWORD_IN_ENTRY(dwBearerMode)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineSetCallQualityOfService(                               // TSPI v2.0
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall,
    LPVOID              lpSendingFlowspec,
    DWORD               dwSendingFlowspecSize,
    LPVOID              lpReceivingFlowspec,
    DWORD               dwReceivingFlowspecSize
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetCallQualityOfService")
		DWORD_IN_ENTRY(dwSendingFlowspecSize)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSetCallTreatment(                                      // TSPI v2.0
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall,
    DWORD               dwTreatment
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetCallTreatment")
		DWORD_IN_ENTRY(dwTreatment)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineSetCurrentLocation(                                    // TSPI v1.4
    DWORD               dwLocation
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetCurrentLocation")
		DWORD_IN_ENTRY(dwLocation)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSetDefaultMediaDetection(
    HDRVLINE            hdLine,
    DWORD               dwMediaModes
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetDefaultMediaDetection")
		DWORD_IN_ENTRY(dwMediaModes)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineSetDevConfig(
    DWORD               dwDeviceID,
    LPVOID              const lpDeviceConfig,
    DWORD               dwSize,
    LPCWSTR             lpszDeviceClass
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetDevConfig")
		DWORD_IN_ENTRY(dwSize)
	END_PARAM_TABLE()

	return EPILOG(0);
}



LONG
TSPIAPI
TSPI_lineSetLineDevStatus(                                      // TSPI v2.0
    DRV_REQUESTID       dwRequestID,
    HDRVLINE            hdLine,
    DWORD               dwStatusToChange,
    DWORD               fStatus
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetLineDevStatus")
		DWORD_IN_ENTRY(dwStatusToChange)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineSetMediaControl(
    HDRVLINE                    hdLine,
    DWORD                       dwAddressID,
    HDRVCALL                    hdCall,
    DWORD                       dwSelect,
    LPLINEMEDIACONTROLDIGIT     const lpDigitList,
    DWORD                       dwDigitNumEntries,
    LPLINEMEDIACONTROLMEDIA     const lpMediaList,
    DWORD                       dwMediaNumEntries,
    LPLINEMEDIACONTROLTONE      const lpToneList,
    DWORD                       dwToneNumEntries,
    LPLINEMEDIACONTROLCALLSTATE const lpCallStateList,
    DWORD                       dwCallStateNumEntries
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetMediaControl")
		DWORD_IN_ENTRY(dwSelect)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSetMediaMode(
    HDRVCALL            hdCall,
    DWORD               dwMediaMode
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetMediaMode")
		DWORD_IN_ENTRY(dwMediaMode)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSetStatusMessages(
    HDRVLINE            hdLine,
    DWORD               dwLineStates,
    DWORD               dwAddressStates
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetStatusMessages")
		DWORD_IN_ENTRY(dwLineStates)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSetTerminal(
    DRV_REQUESTID       dwRequestID,
    HDRVLINE            hdLine,
    DWORD               dwAddressID,
    HDRVCALL            hdCall,
    DWORD               dwSelect,
    DWORD               dwTerminalModes,
    DWORD               dwTerminalID,
    DWORD               bEnable
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetTerminal")
		DWORD_IN_ENTRY(dwAddressID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSetupConference(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall,
    HDRVLINE            hdLine,
    HTAPICALL           htConfCall,
    LPHDRVCALL          lphdConfCall,
    HTAPICALL           htConsultCall,
    LPHDRVCALL          lphdConsultCall,
    DWORD               dwNumParties,
    LPLINECALLPARAMS    const lpCallParams
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetupConference")
		DWORD_IN_ENTRY(dwNumParties)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSetupTransfer(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall,
    HTAPICALL           htConsultCall,
    LPHDRVCALL          lphdConsultCall,
    LPLINECALLPARAMS    const lpCallParams
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSetupTransfer")
		DWORD_IN_ENTRY(hdCall)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineSwapHold(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdActiveCall,
    HDRVCALL            hdHeldCall
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineSwapHold")
		DWORD_IN_ENTRY(dwRequestID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineUncompleteCall(
    DRV_REQUESTID       dwRequestID,
    HDRVLINE            hdLine,
    DWORD               dwCompletionID
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineUncompleteCall")
		DWORD_IN_ENTRY(dwCompletionID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_lineUnhold(
    DRV_REQUESTID       dwRequestID,
    HDRVCALL            hdCall
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineUnhold")
		DWORD_IN_ENTRY(hdCall)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_lineUnpark(
    DRV_REQUESTID       dwRequestID,
    HDRVLINE            hdLine,
    DWORD               dwAddressID,
    HTAPICALL           htCall,
    LPHDRVCALL          lphdCall,
    LPCWSTR             lpszDestAddress
    )
{
	BEGIN_PARAM_TABLE("TSPI_lineUnpark")
		DWORD_IN_ENTRY(dwAddressID)
	END_PARAM_TABLE()

	return EPILOG(0);
}



LONG
TSPIAPI
TSPI_phoneClose(
    HDRVPHONE           hdPhone
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneClose")
		DWORD_IN_ENTRY(hdPhone)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneDevSpecific(
    DRV_REQUESTID       dwRequestID,
    HDRVPHONE           hdPhone,
    LPVOID              lpParams,
    DWORD               dwSize
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneDevSpecific")
		DWORD_IN_ENTRY(dwSize)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetButtonInfo(
    HDRVPHONE           hdPhone,
    DWORD               dwButtonLampID,
    LPPHONEBUTTONINFO   lpButtonInfo
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetButtonInfo")
		DWORD_IN_ENTRY(dwButtonLampID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetData(
    HDRVPHONE           hdPhone,
    DWORD               dwDataID,
    LPVOID              lpData,
    DWORD               dwSize
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetData")
		DWORD_IN_ENTRY(dwDataID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetDevCaps(
    DWORD               dwDeviceID,
    DWORD               dwTSPIVersion,
    DWORD               dwExtVersion,
    LPPHONECAPS         lpPhoneCaps
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetDevCaps")
		DWORD_IN_ENTRY(dwDeviceID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetDisplay(
    HDRVPHONE           hdPhone,
    LPVARSTRING         lpDisplay
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetDisplay")
		DWORD_IN_ENTRY(hdPhone)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetExtensionID(
    DWORD               dwDeviceID,
    DWORD               dwTSPIVersion,
    LPPHONEEXTENSIONID  lpExtensionID
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetExtensionID")
		DWORD_IN_ENTRY(dwDeviceID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetGain(
    HDRVPHONE           hdPhone,
    DWORD               dwHookSwitchDev,
    LPDWORD             lpdwGain
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetGain")
		DWORD_IN_ENTRY(hdPhone)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetHookSwitch(
    HDRVPHONE           hdPhone,
    LPDWORD             lpdwHookSwitchDevs
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetHookSwitch")
		DWORD_IN_ENTRY(lpdwHookSwitchDevs)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_phoneGetIcon(
    DWORD               dwDeviceID,
    LPCWSTR             lpszDeviceClass,
    LPHICON             lphIcon
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetIcon")
		DWORD_IN_ENTRY(dwDeviceID)
	END_PARAM_TABLE()

	return EPILOG(0);
}



LONG
TSPIAPI
TSPI_phoneGetID(
    HDRVPHONE           hdPhone,
    LPVARSTRING         lpDeviceID,
    LPCWSTR             lpszDeviceClass,
    HANDLE              hTargetProcess                          // TSPI v2.0
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetID")
		DWORD_IN_ENTRY(hdPhone)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetLamp(
    HDRVPHONE           hdPhone,
    DWORD               dwButtonLampID,
    LPDWORD             lpdwLampMode
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetLamp")
		DWORD_IN_ENTRY(dwButtonLampID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetRing(
    HDRVPHONE           hdPhone,
    LPDWORD             lpdwRingMode,
    LPDWORD             lpdwVolume
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetRing")
		DWORD_IN_ENTRY(lpdwRingMode)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetStatus(
    HDRVPHONE           hdPhone,
    LPPHONESTATUS       lpPhoneStatus
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetStatus")
		DWORD_IN_ENTRY(hdPhone)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneGetVolume(
    HDRVPHONE           hdPhone,
    DWORD               dwHookSwitchDev,
    LPDWORD             lpdwVolume
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneGetVolume")
		DWORD_IN_ENTRY(dwHookSwitchDev)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneNegotiateExtVersion(
    DWORD               dwDeviceID,
    DWORD               dwTSPIVersion,
    DWORD               dwLowVersion,
    DWORD               dwHighVersion,
    LPDWORD             lpdwExtVersion
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneNegotiateExtVersion")
		DWORD_IN_ENTRY(dwDeviceID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneNegotiateTSPIVersion(
    DWORD               dwDeviceID,
    DWORD               dwLowVersion,
    DWORD               dwHighVersion,
    LPDWORD             lpdwTSPIVersion
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneNegotiateTSPIVersion")
		DWORD_IN_ENTRY(dwDeviceID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneOpen(
    DWORD               dwDeviceID,
    HTAPIPHONE          htPhone,
    LPHDRVPHONE         lphdPhone,
    DWORD               dwTSPIVersion,
    PHONEEVENT          lpfnEventProc
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneOpen")
		DWORD_IN_ENTRY(dwTSPIVersion)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSelectExtVersion(
    HDRVPHONE           hdPhone,
    DWORD               dwExtVersion
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneSelectExtVersion")
		DWORD_IN_ENTRY(dwExtVersion)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSetButtonInfo(
    DRV_REQUESTID       dwRequestID,
    HDRVPHONE           hdPhone,
    DWORD               dwButtonLampID,
    LPPHONEBUTTONINFO   const lpButtonInfo
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetButtonInfo")
		DWORD_IN_ENTRY(dwButtonLampID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSetData(
    DRV_REQUESTID       dwRequestID,
    HDRVPHONE           hdPhone,
    DWORD               dwDataID,
    LPVOID              const lpData,
    DWORD               dwSize
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetData")
		DWORD_IN_ENTRY(dwDataID)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TSPI_phoneSetDisplay(
    DRV_REQUESTID       dwRequestID,
    HDRVPHONE           hdPhone,
    DWORD               dwRow,
    DWORD               dwColumn,
    LPCWSTR             lpsDisplay,
    DWORD               dwSize
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetDisplay")
		DWORD_IN_ENTRY(dwRow)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSetGain(
    DRV_REQUESTID       dwRequestID,
    HDRVPHONE           hdPhone,
    DWORD               dwHookSwitchDev,
    DWORD               dwGain
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetGain")
		DWORD_IN_ENTRY(dwHookSwitchDev)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSetHookSwitch(
    DRV_REQUESTID       dwRequestID,
    HDRVPHONE           hdPhone,
    DWORD               dwHookSwitchDevs,
    DWORD               dwHookSwitchMode
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetHookSwitch")
		DWORD_IN_ENTRY(dwHookSwitchDevs)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSetLamp(
    DRV_REQUESTID       dwRequestID,
    HDRVPHONE           hdPhone,
    DWORD               dwButtonLampID,
    DWORD               dwLampMode
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetLamp")
		DWORD_IN_ENTRY(dwButtonLampID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSetRing(
    DRV_REQUESTID       dwRequestID,
    HDRVPHONE           hdPhone,
    DWORD               dwRingMode,
    DWORD               dwVolume
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetRing")
		DWORD_IN_ENTRY(dwRingMode)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSetStatusMessages(
    HDRVPHONE           hdPhone,
    DWORD               dwPhoneStates,
    DWORD               dwButtonModes,
    DWORD               dwButtonStates
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetStatusMessages")
		DWORD_IN_ENTRY(dwPhoneStates)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_phoneSetVolume(
    DRV_REQUESTID       dwRequestID,
    HDRVPHONE           hdPhone,
    DWORD               dwHookSwitchDev,
    DWORD               dwVolume
    )
{
	BEGIN_PARAM_TABLE("TSPI_phoneSetVolume")
		DWORD_IN_ENTRY(dwHookSwitchDev)
	END_PARAM_TABLE()

	return EPILOG(0);
}



LONG
TSPIAPI
TSPI_providerConfig(
    HWND                hwndOwner,
    DWORD               dwPermanentProviderID
    )
{   //Tapi version 1.4 and earlier - now can be ignored.
	BEGIN_PARAM_TABLE("TSPI_providerConfig")
		DWORD_IN_ENTRY(dwPermanentProviderID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_providerCreateLineDevice(                                  // TSPI v1.4
    DWORD_PTR           dwTempID,
    DWORD               dwDeviceID
    )
{
	BEGIN_PARAM_TABLE("TSPI_providerCreateLineDevice")
		DWORD_IN_ENTRY(dwDeviceID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TSPI_providerCreatePhoneDevice(                                 // TSPI v1.4
    DWORD_PTR           dwTempID,
    DWORD               dwDeviceID
    )
{
	BEGIN_PARAM_TABLE("TSPI_providerCreatePhoneDevice")
		DWORD_IN_ENTRY(dwDeviceID)
	END_PARAM_TABLE()

	return EPILOG(0);
}



LONG
TSPIAPI
TSPI_providerFreeDialogInstance(                                // TSPI v2.0
    HDRVDIALOGINSTANCE  hdDlgInst
    )
{
	BEGIN_PARAM_TABLE("TSPI_providerFreeDialogInstance")
		DWORD_IN_ENTRY(hdDlgInst)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TUISPI_lineConfigDialogEdit(                                    // TSPI v2.0
    TUISPIDLLCALLBACK   lpfnUIDLLCallback,
    DWORD               dwDeviceID,
    HWND                hwndOwner,
    LPCWSTR             lpszDeviceClass,
    LPVOID              const lpDeviceConfigIn,
    DWORD               dwSize,
    LPVARSTRING         lpDeviceConfigOut
    )
{
	BEGIN_PARAM_TABLE("TUISPI_lineConfigDialogEdit")
		DWORD_IN_ENTRY(dwDeviceID)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TUISPI_phoneConfigDialog(                                       // TSPI v2.0
    TUISPIDLLCALLBACK   lpfnUIDLLCallback,
    DWORD               dwDeviceID,
    HWND                hwndOwner,
    LPCWSTR             lpszDeviceClass
    )
{
	BEGIN_PARAM_TABLE("TUISPI_phoneConfigDialog")
		DWORD_IN_ENTRY(dwDeviceID)
	END_PARAM_TABLE()

	return EPILOG(0);
}


LONG
TSPIAPI
TUISPI_providerGenericDialog(                                   // TSPI v2.0
    TUISPIDLLCALLBACK   lpfnUIDLLCallback,
    HTAPIDIALOGINSTANCE htDlgInst,
    LPVOID              lpParams,
    DWORD               dwSize,
    HANDLE              hEvent
    )
{
	BEGIN_PARAM_TABLE("TUISPI_providerGenericDialog")
		DWORD_IN_ENTRY(dwSize)
	END_PARAM_TABLE()

	return EPILOG(0);
}

LONG
TSPIAPI
TUISPI_providerGenericDialogData(                               // TSPI v2.0
    HTAPIDIALOGINSTANCE htDlgInst,
    LPVOID              lpParams,
    DWORD               dwSize
    )
{
	BEGIN_PARAM_TABLE("TUISPI_providerGenericDialogData")
		DWORD_IN_ENTRY(lpParams)
	END_PARAM_TABLE()

	return EPILOG(0);
}





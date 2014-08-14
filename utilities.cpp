/*
  Name: Utilities.cpp
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


#define STRICT

#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <tapi.h>
#include <tspi.h>
#include "wavetsp.h"
#include <string>

#include "utilities.h"

//Utilities for the windows registry, so we can save out and read in the settings we require
const char RegKey[] = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Telephony\\SipTapiSF";
const HKEY WHICHKEY = HKEY_LOCAL_MACHINE;

//Called once to make sure everything is in place.
bool initConfigStore(void)
{
	HKEY ourKey;
	DWORD result;

	TspTrace("initConfigStore: creating Registry Path HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Telephony\\SipTapiSF  ...");

	if ( ERROR_SUCCESS == RegOpenKeyEx(WHICHKEY, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Telephony", 
										0,KEY_ALL_ACCESS , &ourKey) )
	{
		HKEY newKey;
		if ( ERROR_SUCCESS != RegCreateKeyEx(
			ourKey,
			"SipTapiSF",
			0,
			NULL,
			REG_OPTION_NON_VOLATILE,
			KEY_ALL_ACCESS,
			NULL,
			&newKey,
			&result
			)) {
			TspTrace("initConfigStore: Failed to create key HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Telephony\\SipTapiSF");
		}

		RegCloseKey(newKey);
		RegCloseKey(ourKey);

		return true;
	} else {
		TspTrace("initConfigStore: Failed to open key HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Telephony");
	}
	return false;
}

bool storeConfigString(std::string item, std::string str)
{
	HKEY ourKey;

	if ( ERROR_SUCCESS == RegOpenKeyEx(WHICHKEY, RegKey, 0,KEY_ALL_ACCESS /* for just reading we would use KEY_READ*/, &ourKey) )
	{
		//at this point we should have our key opened.
		if ( ERROR_SUCCESS != RegSetValueEx(ourKey,item.c_str(), 0, REG_SZ,(const BYTE *)str.c_str(),str.size()) )
		{
			//MessageBox(0, "Failed set value", "Windows Registry error", MB_SETFOREGROUND);
			TspTrace("storeConfigString: Failed to set registry value %s", item.c_str());
			return false;
		}
		RegCloseKey(ourKey);
	}
	else
	{
		std::string msg = "storeConfigString: Failed to open key: ";
		msg += RegKey;
		//MessageBox(0, msg.c_str() , "Windows Registry error", MB_SETFOREGROUND);
		TspTrace(msg.c_str());
		return false;
	}
	return true;
}

bool readConfigString(std::string item, std::string &str)
{
	HKEY ourKey;
	char tempStr[100];
	DWORD type;
	DWORD length = sizeof(tempStr);

	str = "";

	if ( ERROR_SUCCESS == RegOpenKeyEx(WHICHKEY, RegKey, 0,KEY_READ, &ourKey) )
	{
		//at this point we should have our key opened.
		
		if ( ERROR_SUCCESS != RegQueryValueEx(ourKey,item.c_str(), 0, &type,(BYTE*)&tempStr[0],&length ) )
		{
			//MessageBox(0, "Failed to read value", "Windows Registry error", MB_SETFOREGROUND);
			TspTrace("readConfigString: Failed to read value %s", item.c_str());
			return false;
		}
		else if ( REG_SZ != type )
		{
			//MessageBox(0, "unexpected data type", "Windows Registry error", MB_SETFOREGROUND);
			TspTrace("readConfigString: Unexpected data type");
			return false;
		}
		RegCloseKey(ourKey);
	}
	else
	{
		std::string msg = "readConfigString: Failed to open key: ";
		msg += RegKey;
		//MessageBox(0, msg.c_str() , "Windows Registry error", MB_SETFOREGROUND);
		TspTrace(msg.c_str());
		return false;
	}
	//ensure it is terminated
	tempStr[length] = 0;
	str = tempStr;
	return true;
}


bool storeConfigInt(std::string item, DWORD value)
{
	HKEY ourKey;

	if ( ERROR_SUCCESS == RegOpenKeyEx(WHICHKEY, RegKey, 0,KEY_ALL_ACCESS /* for just reading we would use KEY_READ*/, &ourKey) )
	{
		//at this point we should have our key opened.
		if ( ERROR_SUCCESS != RegSetValueEx(ourKey,item.c_str(), 0, REG_DWORD,(const BYTE *)&value,sizeof(DWORD)) )
		{
			//MessageBox(0, "Failed set value", "Windows Registry error", MB_SETFOREGROUND);
			TspTrace("storeConfigInt: Failed to set value %s", item.c_str());
			return false;
		}
		RegCloseKey(ourKey);
	}
	else
	{
		std::string msg = "storeConfigInt: Failed to open key: ";
		msg += RegKey;
		//MessageBox(0, msg.c_str() , "Windows Registry error", MB_SETFOREGROUND);
		TspTrace("storeConfigInt: Failed to open key");
		return false;
	}
	return true;
}

bool readConfigInt(std::string item, DWORD &value)
{
	HKEY ourKey;
	char tempStr[100];
	DWORD type;
	DWORD length = sizeof(tempStr);

	value = 0;

	if ( ERROR_SUCCESS == RegOpenKeyEx(WHICHKEY, RegKey, 0,KEY_READ, &ourKey) )
	{
		//at this point we should have our key opened.
		
		if ( ERROR_SUCCESS != RegQueryValueEx(ourKey,item.c_str(), 0, &type,(BYTE*)&value,&length ) )
		{
			//MessageBox(0, "Failed to read value", "Windows Registry error", MB_SETFOREGROUND);
			TspTrace("readConfigInt: Failed to read value %s", item.c_str());
			return false;
		}
		else if ( REG_DWORD != type )
		{
			MessageBox(0, "readConfigInt: Unexpected data type", "Windows Registry error", MB_SETFOREGROUND);
			TspTrace("readConfigInt: Unexpected data type for %s", item.c_str());
			return false;
		}
		RegCloseKey(ourKey);
	}
	else
	{
		std::string msg = "readConfigInt: Failed to open key: ";
		msg += RegKey;
		//MessageBox(0, msg.c_str() , "Windows Registry error", MB_SETFOREGROUND);
		TspTrace(msg.c_str());
		return false;
	}
	return true;

}

#if defined(_DEBUG) && defined(DEBUGTSP)

static
void DumpParams(FUNC_INFO* pInfo, ParamDirection dir)
{
    TSPTRACE("  %s parameters:", (dir == dirIn ? "in" : "out"));

    FUNC_PARAM* begin = &pInfo->rgParams[0];
    FUNC_PARAM* end = &pInfo->rgParams[pInfo->dwNumParams];

    for( FUNC_PARAM* pParam = begin; pParam != end; ++pParam )
    {
        if( pParam->dir == dir )
        {
            switch( pParam->pt )
            {
            case ptString:
            {
                char    sz[MAX_PATH+1] = "<NULL>";
                if( pParam->dwVal )
                {
                    wcstombs(sz, (const wchar_t*)pParam->dwVal, MAX_PATH);
                    sz[MAX_PATH] = 0;
                }

                TSPTRACE("    %s= 0x%lx '%s'", pParam->pszVal, pParam->dwVal, sz);
            }
            break;

            case ptDword:
            default:
                if( dir == dirIn )
                {
                    TSPTRACE("    %s= 0x%lx", pParam->pszVal, pParam->dwVal);
                }
                else
                {
                    TSPTRACE("    %s= 0x%lx '0x%lx'", pParam->pszVal, pParam->dwVal, (pParam->dwVal ? *(DWORD*)pParam->dwVal : 0));
                }
            break;
            }
        }
    }
}

void Prolog(FUNC_INFO* pInfo)
{
    char    sz[MAX_PATH+1];
    GetModuleFileName(0, sz, MAX_PATH);

    TSPTRACE("%s() from %s", pInfo->pszFuncName, sz);
    DumpParams(pInfo, dirIn);
}

LONG Epilog(FUNC_INFO* pInfo, LONG tr)
{

    DumpParams(pInfo, dirOut);
    TSPTRACE("%s() returning 0x%x\r\n", pInfo->pszFuncName, tr);
    return tr;
}


void TspTrace(LPCSTR pszFormat, ...)
{
    char    buf[512];
    va_list ap;
    

    va_start(ap, pszFormat);
    wvsprintf(buf, pszFormat, ap);
    OutputDebugString(buf);

#define _DEBUGTOFILE
#ifdef _DEBUGTOFILE
    HANDLE  hFile = CreateFile("c:\\siptapi_0.2.log", GENERIC_WRITE,
                               0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    SetFilePointer(hFile, 0, 0, FILE_END);
    //DWORD   err = GetLastError();
    DWORD   nBytes;
    WriteFile(hFile, buf, strlen(buf), &nBytes, 0);
    WriteFile(hFile, "\r\n", 2, &nBytes, 0);
    CloseHandle(hFile);

#endif

    va_end(ap);
}

#endif  // _DEBUG

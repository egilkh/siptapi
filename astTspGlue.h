/*
  Name: astTspGlue.h
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

#pragma once

#include <tspi.h>
#include <tapi.h>
#include <string>


//This class maps out a call in Asterisk to a call in TAPI
class astTspGlue
{
public:
	astTspGlue(void);
	~astTspGlue(void);

	DWORD signalTapiRinging(void);
	DWORD signalTapiNewCall(void);
	DWORD setTapiLine(HTAPILINE htLine);
	DWORD setLineEvent(LINEEVENT callBack);
	DWORD getState(void);

private:
	std::string peerAddress;
	std::string callerID;
	std::string callerName;
	std::string astChannelID;
	//are we terminating or originating?
#define TERMINATE 0
#define ORIGINATE 1
	BOOL termOrOrig;

	HTAPILINE ourLine;
	//need some help on the following two!!
	HTAPICALL tapiCall;
	HDRVCALL tspiCall;

	LINEEVENT lineEvent;

#define callStIdle LINECALLSTATE_IDLE
#define callStOutgoing LINECALLSTATE_DIALING
#define callStIncomingNew (LINECALLSTATE_IDLE | LINECALLSTATE_OFFERING)
#define callStOffered LINECALLSTATE_OFFERING
#define callStIncomingRinging (LINECALLSTATE_OFFERING | LINECALLSTATE_ACCEPTED)
#define callStIncomingConnected LINECALLSTATE_CONNECTED

	DWORD state;
	DWORD ringCount;
	
public:
	DWORD signalTapiIdle(void);
	DWORD setTapiCall(HTAPICALL hCall);
	DWORD signalTapiConnected(void);
	DWORD setCaller(std::string caller);
	DWORD setDest(std::string dest);
	DWORD signalTapiCallerID(std::string info);
	DWORD signalTapiCallOffering(void);
	HDRVCALL getTspiCallID(void);
	std::string getCallerID(void);
	DWORD signalTapiDialing(std::string extension);
	DWORD signalTapiOutgoing(void);
	std::string getPeerAddress(void);
	DWORD setAstChannelID(std::string channel);
	std::string getAstChannelID(void);
	std::string getCallerName(void);
};

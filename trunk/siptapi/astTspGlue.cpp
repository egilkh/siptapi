/*
  Name: astTspGlue.cpp
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

#include "asttspglue.h"
#include "utilities.h"
#include ".\asttspglue.h"
#include "wavetsp.h"

HDRVCALL g_hdCall = 0;
Mutex hdCallMut;

astTspGlue::astTspGlue(void)
{
	this->lineEvent = 0;
	this->state = callStIdle;
	this->ringCount = 0;
	//we assume incoming - monitor the line - otherwise we signal outgoing
	this->termOrOrig = TERMINATE;

	//
	this->tspiCall = 0;
	this->tapiCall = 0;
}

astTspGlue::~astTspGlue(void)
{
}

////////////////////////////////////////////////////////////////////////////////
// Function signalTapiNewCall
//
// Indicates to TAPI that this call is ringing, this shouldn't be called unless
// we have a valid HtspiCall. This indicates ringing on a terminted call (incoming)
//
////////////////////////////////////////////////////////////////////////////////
DWORD astTspGlue::signalTapiRinging(void)
{
	//we can get here becuase we offer a terminated and originated call
	//so we need to check this...
	if ( this->termOrOrig == ORIGINATE )
	{
		TSPTRACE("Ringing but originated");
		return 0;
	}

	if ( this->state == callStOffered || this->state == callStIncomingRinging )
	{
		if ( this->lineEvent != 0 )
		{
			TSPTRACE("LINEDEVSTATE_RINGING");
			//If the line is ringing then it isn't a call the line is ringing
			//not the call
			this->lineEvent(this->ourLine,
				NULL,
				LINE_LINEDEVSTATE,
				LINEDEVSTATE_RINGING,
				0,
				this->ringCount);
		}
	}

	this->state = callStIncomingRinging;
	this->ringCount++;

	return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Function signalTapiNewCall
//
// This function signals TAPI that a new call has come in, our object needs
// to be set-up correctly first - mainly the call back function and ourLine
//
////////////////////////////////////////////////////////////////////////////////
DWORD astTspGlue::signalTapiNewCall(void)
{
	if ( this->state == callStIdle )
	{
		if ( this->lineEvent != 0 )
		{
			hdCallMut.Lock();
			g_hdCall++;
			this->tspiCall = g_hdCall;
			hdCallMut.Unlock();

			TSPTRACE("LINE_NEWCALL");
			this->lineEvent( this->ourLine,
				0,
				LINE_NEWCALL,
				(DWORD)this->tspiCall,
				(DWORD)&this->tapiCall,
				0);
			//When this function returns tapiCall will be filled with the correct value

			//now send a LINECALLSTATE_OFFERING?
		}
	}

	this->state = callStIncomingNew;

	return 0;
}

DWORD astTspGlue::signalTapiCallOffering(void)
{
	if ( this->state == callStIncomingNew )
	{
		if ( this->lineEvent != 0 )
		{
			TSPTRACE("LINECALLSTATE_OFFERING");
			this->lineEvent( this->ourLine,
				this->tapiCall,
				LINE_CALLSTATE,
				LINECALLSTATE_OFFERING,
				0,
				0);
			
			//this->lineEvent( this->ourLine,
			//	this->tapiCall,
			//	LINE_CALLSTATE,
			//	LINECALLSTATE_ACCEPTED,
			//	0,
			//	0);
		}
	}

	this->state = callStOffered;

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function signalTapiIdle
//
// Sends signal to Tapi that a call has been dropped.
//
////////////////////////////////////////////////////////////////////////////////
DWORD astTspGlue::signalTapiIdle(void)
{
	this->state = callStIdle;

	if ( this->lineEvent != 0 )
	{
		TSPTRACE("LINECALLSTATE_IDLE");
		this->lineEvent(this->ourLine,
			            this->tapiCall,
						LINE_CALLSTATE,
						LINECALLSTATE_IDLE,
						0,
						0);
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function signalTapiConnected
//
// Sends signal to Tapi that a call has been connected.
//
////////////////////////////////////////////////////////////////////////////////
DWORD astTspGlue::signalTapiConnected(void)
{
	BEGIN_PARAM_TABLE("astTspGlue::signalTapiConnected")
		DWORD_IN_ENTRY(this->ourLine)
		DWORD_IN_ENTRY(this->tapiCall)
		DWORD_IN_ENTRY(this->lineEvent)
	END_PARAM_TABLE()

	if ( this->state != callStIncomingConnected )
	{
		if ( this->lineEvent != 0 )
		{
			TSPTRACE("LINECALLSTATE_CONNECTED");
			this->lineEvent(this->ourLine,
				this->tapiCall,
				LINE_CALLSTATE,
				LINECALLSTATE_CONNECTED,
				0,
				0);
		}
	}

	this->state = callStIncomingConnected;

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function signaltspiCallerID
//
// call this to set the member variable...
// From Asterisk the number will be presented as follows
// "Nick" <328476284623324>
//
////////////////////////////////////////////////////////////////////////////////
DWORD astTspGlue::signalTapiCallerID(std::string info)
{
	std::string number, name;
	size_t num_start, num_end;

	if ( -1 != (num_start = info.find('<') ) )
	{
		if ( -1 != (num_end = info.find('>') ) )
		{
			this->callerID = info.substr(num_start+1,num_end-num_start-1);
		}
		else
		{
			//not properly formatted so lets give it something
			this->callerID = info;
		}
	}
	else
	{
		this->callerID = info;
	}

	if ( -1 != (num_start = info.find('"') ) )
	{
		if ( -1 != (num_end = info.find('"',num_start+1) ) )
		{
			this->callerName = info.substr(num_start+1,num_end-num_start-1);
		}
	}

	TspTrace("Caller ID is after stripping is %s",this->callerID.c_str());
	
	if ( this->lineEvent != 0 )
	{
		TSPTRACE("LINECALLSTATE_CALLERID");
		//TO test - I am not sure how permanent the info for this
		//needs to be made - and whether it is in the correct place.
		this->lineEvent(this->ourLine,
			            this->tapiCall,
						LINE_CALLINFO,
						LINECALLINFOSTATE_CALLERID,
						0, //(DWORD)this->callerID.c_str(),
						0);
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function setTapiLine
//
// call this to set the member variable...
//
////////////////////////////////////////////////////////////////////////////////
DWORD astTspGlue::setTapiLine(HTAPILINE htLine)
{
	this->ourLine = htLine;
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function setLineEvent
//
// call this to set the member variable...
//
////////////////////////////////////////////////////////////////////////////////
DWORD astTspGlue::setLineEvent(LINEEVENT callBack)
{
	this->lineEvent = callBack;
	return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Function settspiCall
//
// call this to set the member variable...
//
////////////////////////////////////////////////////////////////////////////////
DWORD astTspGlue::setTapiCall(HTAPICALL hCall)
{
	this->tapiCall = hCall;
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function setCaller
//
// call this to set the member variable... (if we wish to terminate a call)
//
////////////////////////////////////////////////////////////////////////////////
DWORD astTspGlue::setCaller(std::string CallerID)
{
	this->peerAddress = CallerID;
	this->termOrOrig = TERMINATE;
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function setDest
//
// call this to set the member variable...
//
////////////////////////////////////////////////////////////////////////////////
DWORD astTspGlue::setDest(std::string Dest)
{
	this->peerAddress = Dest;
	this->termOrOrig = ORIGINATE;

	return 0;
}

HDRVCALL astTspGlue::getTspiCallID(void)
{
	return this->tspiCall;
}
DWORD astTspGlue::getState(void)
{
	return this->state;
}

std::string astTspGlue::getCallerID(void)
{
	return this->callerID;
}

DWORD astTspGlue::signalTapiDialing(std::string extension)
{
	this->peerAddress = extension;

	if ( this->lineEvent != 0 )
	{
		TSPTRACE("LINECALLSTATE_DIALING: %s ",extension.c_str());
		this->lineEvent(this->ourLine,
			this->tapiCall,
			LINE_CALLSTATE,
			LINECALLSTATE_DIALING,
			0,
			0);
	}

	return 0;
}

DWORD astTspGlue::signalTapiOutgoing(void)
{
	//this maybe needed to grab the caller ID of the outgoing call
	//but we know this anyway!
	this->termOrOrig = ORIGINATE;
	return 0;
}

std::string astTspGlue::getPeerAddress(void)
{
	return this->peerAddress;
}

DWORD astTspGlue::setAstChannelID(std::string channel)
{
	this->astChannelID = channel;
	return 0;
}

std::string astTspGlue::getAstChannelID(void)
{
	return this->astChannelID;
}

std::string astTspGlue::getCallerName(void)
{
	return this->callerName;
}

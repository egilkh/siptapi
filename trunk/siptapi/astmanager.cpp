/*
  Name: astmanager.cpp
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


#include "astmanager.h" // class's header file
#include "wavetsp.h"
#include ".\astmanager.h"

#include <eXosip2/eXosip.h>

//extern "C" int eXosip_init();

////////////////////////////////////////////////////////////////////////////////
// Function astManager::astManager()
//
// Class constructor.
//
////////////////////////////////////////////////////////////////////////////////
astManager::astManager(void)
{	
	this->LOGGEDIN = FALSE;
	//to indicate that this is not connected.
	this->s = 1;

	this->cid = 0;
	this->did = 0;

	this->reverseMode=0;
	this->dontSendBye=0;
	this->immediateSendBye=0;
	this->dwCallState=LINECALLSTATE_UNKNOWN;
	this->dwCallStateMode=0;
}

////////////////////////////////////////////////////////////////////////////////
// Function astManager::~astManager()
//
// Class destructor.
//
////////////////////////////////////////////////////////////////////////////////
astManager::~astManager()
{
	this->dropConnection();
}

DWORD astManager::setLineEvent(LINEEVENT callBack)
{
	this->lineEvent = callBack;
	return 0;
}

DWORD astManager::setTapiLine(HTAPILINE line)
{
	this->htLine = line;
	return 0;
}

HTAPILINE astManager::getTapiLine(void)
{
	return this->htLine;
}


DWORD astManager::setTapiCall(HTAPICALL line)
{
	this->htCall = line;
	return 0;
}

HTAPICALL astManager::getTapiCall(void)
{
	return this->htCall;
}


////////////////////////////////////////////////////////////////////////////////
// Function astManager::setHost(std::string astHost)
//
// Sets the hostname of the asterisk server we are connecting to.
//
////////////////////////////////////////////////////////////////////////////////
void astManager::setHost(std::string astHost)
{
    this->host = astHost;
}

////////////////////////////////////////////////////////////////////////////////
// Function astManager::setPort(Long astPort)
//
// Sets the port number for the server we are connecting to.
//
////////////////////////////////////////////////////////////////////////////////
void astManager::setPort(DWORD astPort)
{
    this->port = astPort;
}

////////////////////////////////////////////////////////////////////////////////
// Function astManager::usernamePassword(std::string username, std::string password)
//
// Sets the hostname of the asterisk server we are connecting to.
//
////////////////////////////////////////////////////////////////////////////////
void astManager::setUsernamePassword(std::string username, std::string usernameexten,
									 std::string password, std::string authusername)
{
    this->user = username;
    this->authuser = authusername;
    this->userexten = usernameexten;
    this->pass = password;
}

////////////////////////////////////////////////////////////////////////////////
// Function astManager::setOriginator(std::string channel)
//
// Sets call originator i.e. "Sip/nick"
//
////////////////////////////////////////////////////////////////////////////////
void astManager::setOriginator(std::string channel)
{
    this->originator = channel;
}

////////////////////////////////////////////////////////////////////////////////
// Function astManager::setOutgoingChannel(std::string channel)
//
// Sets call outgoing channel i.e. "CAPI:<number>:"
//
////////////////////////////////////////////////////////////////////////////////
void astManager::setOutgoingChannel(std::string channel)
{
    this->outGoingChannel = channel;
	this->context = "";
}

////////////////////////////////////////////////////////////////////////////////
// Function astManager::setInBoundChannel(std::string channel)
//
// Sets call inbound channel channel i.e. "Sip/nick", this can 
// also be a regex if set in the function below
//
////////////////////////////////////////////////////////////////////////////////
void astManager::setInBoundChannel(std::string channel)
{
	this->incomingChannel = channel;
}

////////////////////////////////////////////////////////////////////////////////
// Function astManager::setOutgoingChannel(std::string channel)
//
// Sets call outgoing channel i.e. "CAPI:<number>:"
//
////////////////////////////////////////////////////////////////////////////////
void astManager::useInBoundRegex(bool YesNo)
{
	this->useInComingRegex = YesNo;
}

////////////////////////////////////////////////////////////////////////////////
// Function astManager::setOutgoingChannel(std::string channel)
//
// Sets context i.e. "internal"
//
////////////////////////////////////////////////////////////////////////////////
void astManager::setContext(std::string cont)
{
    this->context = cont;
	this->outGoingChannel = "";
}

////////////////////////////////////////////////////////////////////////////////
// Function astManager::setOutgoingChannel(std::string callerid)
//
// Sets callerid i.e. "Nick" <328476284623324>
// This is for the outgoing caller ID (call origination)
//
////////////////////////////////////////////////////////////////////////////////
void astManager::setCallerID(std::string callerid)
{
    this->callerid = callerid;
}

////////////////////////////////////////////////////////////////////////////////
//
// Actions
//
// Now we have set the object up - what can we do?
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Function astManager::connect(void)
//
// Connects to the asterisk server if succsessful sets this->s to the socket
// and returns the socket else returns the error code.
//
// socket  changed to int: s=0: not initialized, s=1: initialized
////////////////////////////////////////////////////////////////////////////////
int astManager::astConnect(void)
{
	TSPTRACE("astManager::astConnect...");
	TSPTRACE("ipcom.at SIPTAPI " SIPTAPI_VERSION " / eXosip %s: Opening Line ...",eXosip_get_version());
	TSPTRACE("initializing eXosip2...");

	int i;
	char version_string[100];

	TRACE_INITIALIZE (6, stdout);

	i=eXosip_init();
	if (i!=0)
	{
		TSPTRACE("eXosip_init failed");
        this->s = 0;
        return 0;
	}
	TSPTRACE("eXosip_init succeeded");

	this->ongoingcall = 0;

	int sipport = 5066;
	int counter = 0;
	while (1) {
		// old exosip2 version
		// i = eXosip_listen_to(IPPROTO_UDP, INADDR_ANY, sipport);
		// new eXosip requires
		i = eXosip_listen_addr(IPPROTO_UDP, INADDR_ANY, sipport, AF_INET, 0);
		if (i!=0) {
			TspTrace("could not initialize transport layer at port %i",sipport);
			sipport += 2;
			counter++;
			if (counter < 10) {
                continue;
			} else {
				TspTrace("failed to bind a SIP port after %i tries, stopping...",counter);
				return 0;
			}
		}
		TspTrace("SIP stack uses port %i for SIP signaling",sipport);
		break;
	}

	snprintf(version_string, 100, "ipcom.at SIPTAPI " SIPTAPI_VERSION " / eXosip %s", eXosip_get_version());
	eXosip_set_user_agent(version_string);

	this->s = 1;
	return 1;

}

////////////////////////////////////////////////////////////////////////////////
// Function sendCommand(std::string command)
//
// Private function
//
/* TODO (#1#): the send command function needs to wait for a 
               responce to check it has been accepted by the 
               server */
// Sends the formsatted command to the asterisk server
//
////////////////////////////////////////////////////////////////////////////////
DWORD astManager::sendCommand(std::string command)
{
	if (send(this->s, command.c_str(), (int)command.length() , 0) != SOCKET_ERROR )
	{
		TSPTRACE("Sending to asterisk successful");
	} else {
		TSPTRACE("Sending to asterisk failed");
	}
	TSPTRACE("%s",command.c_str());
	return true;
}


////////////////////////////////////////////////////////////////////////////////
// Function astManager::login(std::string user, std::string password)
//
// Private function
//
// Opens the connection to the server and logs in. Returns 1 on sucsess
// if unsucsessful will close the socket down otherwise it will open a socket
// and leave it open.
//
////////////////////////////////////////////////////////////////////////////////
DWORD astManager::login(void)
{
/*    std::string command;

	if ( this->LOGGEDIN == TRUE )
	{
		return 1;
	}
    
    command = "Action: Login\r\n";
    //send the command
    this->sendCommand(command);
      
    command = "UserName: ";
    command += this->user;
    command += "\r\n";
    //send the command
    this->sendCommand(command);
      
    command = "Secret: ";
    command += this->pass;
    command += "\r\n\r\n";
    //send the command    
    this->sendCommand(command);
  */  
    return 1;
}

////////////////////////////////////////////////////////////////////////////////
// Function astManager::originate(std::string destAddress)
//
// Originates a call, when this is called, asterisk will call the originator
// i.e. this->originator, and then call this->outGoingChannel:destAddress
// if that makes sence! Returns 1 on succsess otherwise returns 0.
//
////////////////////////////////////////////////////////////////////////////////
DWORD astManager::originate(std::string destAddress)
{
	int i;

	if (this->authuser == "") {
		this->authuser = this->user;
		TspTrace("authusername = %s",this->authuser.data());
	}

	i = eXosip_clear_authentication_info();
	i = eXosip_add_authentication_info(this->user.data(), 
		this->authuser.data(), this->pass.data(), "", "");
	
	osip_message_t *invite;
	std::string route;

	TspTrace("removing spaces and special characters from phone number...");
	TspTrace("original number: %s",destAddress.data());
	std::string::size_type pos;
	while (	(pos = destAddress.find_first_not_of("*#0123456789")) != std::string::npos) {
		destAddress.erase(pos,1);
		TspTrace("bad digit found, new number is: %s",destAddress.data());
	}
	TspTrace("trimmed number: %s",destAddress.data());

	// from:  wir
	// to:    das final target
	// exten: das lokale SIP Telefon
	this->to   = ("sip:") + destAddress + ("@") + this->originator;
	this->from = ("sip:") + this->user  + ("@") + this->originator;
    if (this->userexten == "") {
        // use SIP AoR username as destination
        this->exten = ("sip:") + this->user + ("@") + this->originator;
    } else {
        if (this->userexten.find("@") == std::string::npos) {
            this->exten = ("sip:") + this->userexten + ("@") + this->originator;
        } else {
            this->exten = ("sip:") + this->userexten;
        }
    }
	if (this->host == "") {
		route = ("");
	} else {
		if (this->host.find(";lr") == std::string::npos) {
			// not found
			route = ("<sip:") + this->host + (";lr>");
		} else {
			// found
			route = ("<sip:") + this->host + (">");
		}
	}

	if (this->reverseMode) {
		// reuse destAddress to change refertarget and calltarget
		destAddress = to;
		to = exten;
		exten = destAddress;
	}

	TspTrace("From:     this->from.data()  ='%s'",this->from.data());
	TspTrace("RURI/To:  this->exten.data() ='%s'",this->exten.data());
	TspTrace("Refer-To: this->to.data()    ='%s'",this->to.data());
	TspTrace("Route:    route.data()       ='%s'",route.data());

	i = eXosip_call_build_initial_invite(&invite,
		this->exten.data(),		// send INVITE to users's extension (Asterisk workaround)
		this->from.data(),		// from
		route.data(),			// route
		"click2dial call");		//subject
	if (i!=0)
	{
		TspTrace("eXosip_call_build_initial_invite failed: (bad arguments?)");
		this->ongoingcall = 0;
		return LINEERR_CALLUNAVAIL;
	}
	TspTrace("eXosip_call_build_initial_invite succeeded");

	// Set supported methods
	i = osip_message_set_header(invite, "Allow", "INVITE, ACK, CANCEL, BYE, NOTIFY");
	if (i != 0) {
		TspTrace("osip_message_set_header Allow failed ...");
	}

	char tmp[4096];
	char localip[128];
	eXosip_guess_localip(AF_INET, localip, 128);
	TspTrace("localip = '%s'", localip);
	_snprintf(tmp, 4096,
		"v=0\r\n"
		"o=click2dial 0 0 IN IP4 %s\r\n"
		"s=click2dial call\r\n"
		"c=IN IP4 %s\r\n"
		"t=0 0\r\n"
		"m=audio %s RTP/AVP 0 8 18 3 4 9 15 97 98\r\n"
		"a=rtpmap:0 PCMU/8000\r\n"
		"a=rtpmap:18 G729/8000\r\n"
		"a=rtpmap:97 ilbc/8000\r\n"
		"a=rtpmap:98 speex/8000\r\n",
		localip, localip, "8000");
	TspTrace("SDP = '%s'", tmp);
	osip_message_set_body(invite, tmp, strlen(tmp));
	osip_message_set_content_type(invite, "application/sdp");

	if (this->autoAnswer) {
		TspTrace("auto-answer is activated, adding headers ...");
		// add header to indicate to the SIP phone that it should
		// auto-answer the phone call
		// SNOM: http://asterisk.snom.com/index.php/Asterisk_1.4/Intercom
		i = osip_message_set_header(invite, "Call-Info", "<sip:127.0.0.1>;answer-after=0");
		if (i != 0) {
			TspTrace("osip_message_set_header SNOM failed ...");
		}
		// Polycom: http://www.voip-info.org/wiki/view/Polycom+auto-answer+config
		i = osip_message_set_header(invite, "Alert-Info", "Ring Answer");
		if (i != 0) {
			TspTrace("osip_message_set_header POLYCOM failed ...");
		}
	}
	if (this->autoAnswer2) {
		TspTrace("auto-answer2 is activated, adding headers ...");
		// Siemens: http://wiki.siemens-enterprise.com/wiki/Asterisk_Feature_Example_Configuration#Auto_Answer
		i = osip_message_set_header(invite, "Alert-Info", "<http://example.com>;info=alert-autoanswer");
		if (i != 0) {
			TspTrace("osip_message_set_header SIEMENS failed ...");
		}
		// Aastra: http://www.voip-info.org/wiki/view/Sayson+IP+Phone+Auto+Answer
		//i = osip_message_set_header(invite, "Alert-Info", "info=alert-autoanswer");
		//if (i != 0) {
		//	TspTrace("osip_message_set_header Aastra failed ...");
		//}
	}

	eXosip_lock();
	i = eXosip_call_send_initial_invite(invite);
	if (i == -1) {
		TspTrace("eXosip_call_send_initial_invite failed: (bad arguments?)");
		this->ongoingcall = 0;
		return LINEERR_CALLUNAVAIL;
	}
	TspTrace("eXosip_call_send_initial_invite returned: %i",i);
	this->cid = i;
	this->ongoingcall = 1;
	eXosip_unlock();

	TspTrace("eXosip_unlock...originating done...further processing must be done in the thread");

		
	//What communication do we need with Asterisk to
    //complete this?
    //
    // 1. Login
    // 2. Originate - we use the application Dial for this
    // 
    // When the object is created, we then login and set up
    // all of the parameters so in this function all we need
    // to do is perform the origination.
/*    if ( this->s == INVALID_SOCKET ) 
    {
        if ( INVALID_SOCKET == this->astConnect() )
        {
                return 0;
        }
    }

	//if the socket is invalid then we need to login as well.
	if ( 0 == this->login() ) 
	{
		return 0;
	}

	std::string command;

	command = "Action: Originate\r\n";
	//send the command
	this->sendCommand(command);

	command = "Channel: ";
	command += this->originator;
	command += "\r\n";
	//send the command
	this->sendCommand(command);

	if ( this->outGoingChannel != "" )
	{
		command = "Application: Dial\r\n";
		//send the command
		this->sendCommand(command);

		//Ensure that we are sending nothing but numbers to Asterisk
		std::string::iterator it;
		std::string tempStr("");

		for ( it = destAddress.begin(); it != destAddress.end(); it++ )
		{
			if ( *it >= '0' && *it <= '9' )
			{
				tempStr += *it;
			}
		}

		command = "Data: ";
		command += this->outGoingChannel;
		command += tempStr;
		command += "\r\n\r\n";
		//send the command
		this->sendCommand(command);
	}
	else
	{
		//call out by using a context
		//Exten: 1234 
		//Context: default 
		//Callerid: "User" <714-555-1212>

		//Ensure that we are sending nothing but numbers to Asterisk
		std::string::iterator it;
		std::string tempStr("");

		for ( it = destAddress.begin(); it != destAddress.end(); it++ )
		{
			if ( *it >= '0' && *it <= '9' )
			{
				tempStr += *it;
			}
		}
		command = "Exten: ";
		command += tempStr;
		command += "\r\n";
		//send the command
		this->sendCommand(command);

		command = "Priority: 1";
		command += "\r\n";
		this->sendCommand(command);

		//if we set the caller ID?
		if ( this->callerid != "" )
		{
			command = "Callerid: ";
			command += this->callerid;
			command += "\r\n";
			//send the command
			this->sendCommand(command);
		}

		command = "Context: ";
		command += this->context;
		command += "\r\n\r\n";
		//send the command
		this->sendCommand(command);


	}
*/    
    return 0;
}



////////////////////////////////////////////////////////////////////////////////
// Function astManager::originate(std::string destAddress)
//
// This function will wait for a time for the a messgage from Asterisk
// te end of a message is marked by a \r\n\r\n
////////////////////////////////////////////////////////////////////////////////
astEvent* astManager::waitForMessage(void)
{
	BEGIN_PARAM_TABLE("astManager::waitForMessage")
		DWORD_IN_ENTRY(0)
	END_PARAM_TABLE()

	astEvent *ev = new astEvent;

	DWORD numBytes;
	/* TODO (#1#): need to have a timeout strategy in case of error and better error checking */
	char recvBuf[2];
	std::string rawMessage;
	
	while ( rawMessage.find("\r\n\r\n") == -1 )
	{
		numBytes = recv ( this->s, recvBuf, 1, 0);

		if ( numBytes == 0 || numBytes == SOCKET_ERROR )
		{
			//if we get here the chances are that the socket has
			//been closed down.
			return NULL;
		}

		//An asterisk message is very similar to other protocols using
		//\r\n at the end of an item, and \r\n\r\n at the end of a message
		recvBuf[1] = 0;	//ensure NULL termination
		rawMessage += recvBuf;
		//TSPTRACE("%s",rawMessage.c_str());

	}
	TSPTRACE("Found terminator");

	//When we get here we have a complete message which we need to split up 
	//into a usable format (I suppose we could also keep as is - but I prefer it this way!)
	size_t carPos;
	while ( ( carPos = rawMessage.find("\r\n") ) != -1)
	{
		std::string line = rawMessage.substr(0,carPos);
		rawMessage = rawMessage.substr(carPos+2, rawMessage.length());
		//When we are down to the line level it is in the format
		//header: data
		size_t dataPos;
		if ( ( dataPos = line.find(":")) != -1 )
		{
			std::string header = line.substr(0,dataPos);
			std::string data = line.substr(dataPos+2, line.length());
			
			// a little logging info
			TSPTRACE("header: %s, data: %s",header.c_str(), data.c_str());

			if ( header == "Event" )
			{
				if ( data == "Shutdown" )
				{
					ev->event = evShutdown;
				}
				else if ( data == "Hangup" )
				{
					ev->event = evHangup;
				}
				else if ( data == "Rename " )
				{
					ev->event = evRename;
				}
				else if ( data == "Hangup" )
				{
					ev->event = evHangup;
				}
				else if ( data == "Newcallerid" )
				{
					ev->event = evNewcallerid;
				}
				else if ( data == "Newchannel" )
				{
					ev->event = evNewchannel;
				}
				else if ( data == "Link" )
				{
					ev->event = evLink;
				}
				else if ( data == "Unlink" )
				{
					ev->event = evUnlink;
				}
				else if ( data == "ExtensionStatus" )
				{
					ev->event = evExtensionStatus;
				}
				else if ( data == "Reload" )
				{
					ev->event = evReload;
				}
				else if ( data == "Newexten" )
				{
					ev->event = evNewexten;
				}
				else if ( data == "Newstate" )
				{
					ev->event = evNewstate;
				}
			}
			else if ( header == "Uniqueid" )
			{
				ev->uniqueid = data;
			}
			else if ( header == "Uniqueid1" )
			{
				ev->uniqueid1 = data;
			}
			else if ( header == "Uniqueid2" )
			{
				ev->uniqueid2 = data;
			}
			else if ( header == "Callerid" )
			{
				ev->callerId = data;
			}
			else if ( header == "Extension" )
			{
				ev->extension = data;
			}
			else if ( header == "Context" )
			{
				ev->context = data;
			}
			else if ( header == "State" )
			{
				if ( data == "Ringing" )
				{
					ev->state = stRinging;
				}
				else if ( data == "Ring" )
				{
					ev->state = stRing;
				}
				else if ( data == "Dialing" )
				{
					ev->state = stDial;
				}
				else if ( data == "Down" )
				{
					ev->state = stDown;
				}
				else if (data == "Up" )
				{
					ev->state = stUp;
				}
			}
			else if ( header == "Channel" )
			{
				ev->channel = data;
			}
		}

	}

	return ev;
}

////////////////////////////////////////////////////////////////////////////////
// Function astManager::originate(std::string destAddress)
//
// Call this function to drop whatever call has been originated by this class. 
// This is still to be completed.
void astManager::dropChannel()
{
	std::string command;

	int i;
	TspTrace("astManager::dropChannel...calling  eXosip_call_terminate...");
	if (this->ongoingcall > 0) {
		i = eXosip_call_terminate(this->cid, this->did);
		if (i!=0)
		{
			TspTrace("dropChannel: eXosip_call_terminate failed ... probably line was closed already");
		} else {
			TspTrace("dropChannel: eXosip_call_terminate succeeded ...");
		}
	} else {
		TspTrace("dropChannel: no ongoing call, just say OK...");
	}
	if ( this->lineEvent != 0 ) {
		TspTrace("dropChannel: sending DICONNECTED and IDLE ...");
		TSPTRACE("sending LINECALLSTATE_DISCONNECTED ...");
		this->lineEvent( this->htLine, this->htCall,
			LINE_CALLSTATE, LINECALLSTATE_DISCONNECTED,
			0, 0 /*or should iI use LINEMEDIAMODE_UNKNOWN ?*/);
		TSPTRACE("sending LINECALLSTATE_IDLE ...");
		this->lineEvent( this->htLine, this->htCall,
			LINE_CALLSTATE, LINECALLSTATE_IDLE,
			0, 0 );
	}
	this->ongoingcall = 0;
}





DWORD astManager::dropConnection(void)
{
	/*if (this->s != INVALID_SOCKET)
	{
		closesocket(this->s);
	}

	this->s = INVALID_SOCKET;
*/

	TSPTRACE("...terminating eXosip2...");
	TSPTRACE("...shutting down thread, set Call State to -1 ...");
	this->ongoingcall = -1;
	TSPTRACE("...shutting down thread, waiting 2 seconds for thread to exit ...");
	Sleep(2000);
	TSPTRACE("...calling eXosip_quit...");
	eXosip_quit();
	TSPTRACE("...eXosip2 terminated!");
	return 0;
}

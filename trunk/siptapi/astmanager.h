/*
  Name: astmanager.h
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

#ifndef ASTMANAGER_H
#define ASTMANAGER_H

#include <string>
// winsock2 will be included in utillities.cpp, must be included before windows.h
//#include <winsock.h>

#include "dll.h"

#include <map>


class astChannel
{
	//Encapsulate information regarding an Asterisk channel
	std::string state;
	std::string channel;
	std::string callerId;
};

class astEvent
{
public:
	astEvent() { event = 0;state = 0; }
	DWORD event;
	DWORD state;
	std::string channel;
	std::string context;
	std::string extension;
	std::string priority;
	std::string uniqueid;
	std::string callerId;
	std::string uniqueid1;
	std::string uniqueid2;
};

//at the moment I am not parsing for all of these types - but 
//for completness will list them here (grep from asterisk source!)
#define evShutdown 1
#define evHangup 2
#define evRename 3
#define evNewstate 4
#define evNewcallerid 5
#define evNewchannel 6
#define evLink 7
#define evUnlink 8
#define evExtensionStatus 9
#define evReload 10
#define evNewexten 11

//incoming ringing
#define stRinging 1
//the next one is a state just before dialing
#define stRing 2
#define stDial 3
#define stDown 4
#define stUp 5

// connection to our asterisk manager
class astManager
{
	public:
		// class constructor
		astManager();
		// class destructor
		~astManager();

		int reverseMode;
		int dontSendBye;
		int immediateSendBye;
		int autoAnswer;
		int autoAnswer2;
		DWORD dwCallState;
		DWORD dwCallStateMode;

		DWORD setLineEvent(LINEEVENT callBack);
		DWORD setTapiLine(HTAPILINE line);
		HTAPILINE getTapiLine(void);
		DWORD setTapiCall(HTAPICALL line);
		HTAPICALL getTapiCall(void);

		//setting up paramaters
		void setHost(std::string astHost);
		void setPort(DWORD astPort);
		
		//this next function sets the originators caller string
		//for example this would be
		//      "Sip/nick"
        void setOriginator(std::string channel);
        
        //outgoing channel would be as an example
        //       "CAPI:<number>:"
        //which we would then appent a number to.
		void setOutgoingChannel(std::string channel);
		//This sets the string we use to search the incoming
		//channel against i.e. Sip/nick of regex Sip/[nick|bob]
		void setInBoundChannel(std::string channel);
		void useInBoundRegex(bool YesNo);
		void setContext(std::string context);
		void setCallerID(std::string callerid);
		void setUsernamePassword(std::string username, std::string usernameexten, 
			std::string password, std::string authusername);
		//things we can do
		int astManager::astConnect(void);
		DWORD originate(std::string destAddress);
		// Call this function to drop whatever call has been originated by this class. 
		// This is still to be completed.
		void dropChannel();
		// This function will wait for a time for the a messgage from Asterisk
		// te end of a message is marked by a \r\n\r\n
		astEvent* waitForMessage(void);
		DWORD login(void);

protected:
		HTAPILINE htLine;
		HTAPICALL htCall;
		LINEEVENT lineEvent;

		//asterisk host
		std::string host;
		DWORD port;

		//asterisk username and password
        std::string user;
        std::string authuser;
        std::string userexten;
        std::string pass;
        
        //error string we maintain
        std::string error;
        
        //channels
        std::string originator;
        std::string outGoingChannel;
		std::string incomingChannel;
		bool useInComingRegex;

		//if we are dialing out by a context
		std::string context;

		//if we are setting a caller ID
		std::string callerid;
        
        //Our connection to the Asterisk server
        int s;

		//are we or aren't we?
		BOOL LOGGEDIN;
        
        //Private functions
        DWORD sendCommand(std::string command);


		// klaus
		std::string to, from, exten;
		int cid, did;

public:
		// outoing call active
		int ongoingcall; // 0=idle, 1=INVITE sent, 2= ACK sent, 3= REFER sent, 0=Done, -1=exit
						 // 4 = line reserved for outgoing INVITE
		
	DWORD dropConnection(void);

};

#endif // ASTMANAGER_H


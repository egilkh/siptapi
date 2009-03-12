/*
  Name: tapiastmanager.cpp
  Copyright: Under the GNU General Public License Version 2 or later (the "GPL")
  Author: Nick Knight
          Klaus Darilion
  Description: ties the astmanager connection to the extra information required for TAPI
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

//we have to make sure we are thread safe - so need a mutex
#include "utilities.h"
#include ".\tapiastmanager.h"
#include "wavetsp.h"
#include <algorithm>
#include <cctype>
#include <CString>

#include <eXosip2/eXosip.h>

// #include <boost/regex.hpp>

Mutex tspMut;

tapiAstManager::tapiAstManager(void)
{
	this->lineEvent = 0;
	this->htLine = 0;
	this->htCall = 0;
}

tapiAstManager::~tapiAstManager(void)
{
	this->dropConnection();
}

DWORD tapiAstManager::processMessages(void)
{
	TSPTRACE("tapiAstManager::processMessages");

	int counter =0;
	char buf[100];
	buf[0]= '\0';
	int idlewait = 500;
	int busywait = 50;
	int wait = 50;
	int i;

	wait = idlewait;
	eXosip_event_t *je;
	for (;;)
	{
		if (this->ongoingcall) {
			wait = busywait;
		} else {
			wait = idlewait;
		}
		eXosip_automatic_action();
		je = eXosip_event_wait(0,wait);
        //TspTrace("eXosip_event_wait ended",je->textinfo);

		if (je != NULL) {
            TspTrace("Event '%s' received...",je->textinfo);
			if (je->response != NULL) {
				TspTrace("response = '%i'",je->response->status_code);
			}
		}

//		TspTrace("this->ongoingcall before switch() = '%i'",this->ongoingcall);
		switch(this->ongoingcall) {
			case 0:
				TspTrace("No ongoing call, doing nothing...\n");
				break;
			case 1:
				TspTrace("Ongoing call = 1");
				if (je != NULL) {
					if (je->type == EXOSIP_CALL_TIMEOUT) {
						TspTrace("EXOSIP_CALL_TIMEOUT received: (cid=%i did=%i) '%s'",je->cid,je->did,je->textinfo);
						//this->ongoingcall = 0;
						break;
					}
					if (je->type == EXOSIP_CALL_RELEASED) {
						TspTrace("EXOSIP_CALL_RELEASED received: (cid=%i did=%i) '%s'",je->cid,je->did,je->textinfo);
						this->ongoingcall = 0;
						if ( this->lineEvent != 0 ) {
							TSPTRACE("sending LINECALLSTATE_IDLE ...");
							this->lineEvent( this->htLine, this->htCall,
								LINE_CALLSTATE, LINECALLSTATE_IDLE,
								0, 0 /*or should iI use LINEMEDIAMODE_UNKNOWN ?*/);
						}
						break;
					}
					if (je->type == EXOSIP_CALL_REQUESTFAILURE) {
						TspTrace("EXOSIP_CALL_REQUESTFAILURE received: (cid=%i did=%i) '%s'",je->cid,je->did,je->textinfo);
						if (je->response != NULL) {
							if (je->response->status_code == 487) {
								TspTrace("Call successful canceled ...");
								this->ongoingcall = 0;
								if ( this->lineEvent != 0 ) {
									TSPTRACE("sending LINECALLSTATE_IDLE ...");
									this->lineEvent( this->htLine, this->htCall,
										LINE_CALLSTATE, LINECALLSTATE_IDLE,
										0, 0 /*or should iI use LINEMEDIAMODE_UNKNOWN ?*/);
								}
								break;
							}
							if ((je->response->status_code == 407) || (je->response->status_code == 404)) {
								TspTrace("Authentication required ... will be handled by SIP stack");
								break;
							}
							this->ongoingcall = 0;
							counter = 0;
							if ( this->lineEvent != 0 ) {
								TSPTRACE("sending LINECALLSTATE_DISCONNECTED ...");
								this->lineEvent( this->htLine, this->htCall,
									LINE_CALLSTATE, LINECALLSTATE_DISCONNECTED,
									0, 0 /*or should iI use LINEMEDIAMODE_UNKNOWN ?*/);
							}
							if ( this->lineEvent != 0 ) {
								TSPTRACE("sending LINECALLSTATE_IDLE ...");
								this->lineEvent( this->htLine, this->htCall,
									LINE_CALLSTATE, LINECALLSTATE_IDLE,
									0, 0 /*or should iI use LINEMEDIAMODE_UNKNOWN ?*/);
							}
							break;
						}
					}
					if (je->type == EXOSIP_CALL_RINGING) {
						TspTrace("EXOSIP_CALL_RINGING received: (cid=%i did=%i) '%s'",je->cid,je->did,je->textinfo);
						this->cid = je->cid;
						this->did = je->did;
						if ( this->lineEvent != 0 ) {
							TSPTRACE("sending LINEDEVSTATE_RINGING ...");
							this->lineEvent(this->htLine, NULL,
								LINE_LINEDEVSTATE, LINEDEVSTATE_RINGING,
								0, 0 /* RingCount*/);
						}
						break;
					}
					if (je->type == EXOSIP_CALL_ANSWERED) {
						TspTrace("EXOSIP_CALL_ANSWERED received: (cid=%i did=%i) '%s'",je->cid,je->did,je->textinfo);
						this->cid = je->cid;
						this->did = je->did;
						if ( this->lineEvent != 0 ) {
							TSPTRACE("sending LINECALLSTATE_CONNECTED ...");
							this->lineEvent( this->htLine, this->htCall,
								LINE_CALLSTATE, LINECALLSTATE_CONNECTED,
								0, 0 /*or should iI use LINEMEDIAMODE_UNKNOWN ?*/);
						}
						osip_message_t *ack;
						eXosip_lock();
						i = eXosip_call_build_ack(je->did, &ack);
						if (i != 0) {
							TspTrace("eXosip_call_build_ack failed...");
							eXosip_unlock();
							break;
						}
						TspTrace("eXosip_call_build_ack succeeded...");
						i = eXosip_call_send_ack(je->did, ack);
						if (i != 0) {
							TspTrace("eXosip_call_send_ack failed...");
							eXosip_unlock();
							break;
						}
						TspTrace("eXosip_call_send_ack succeeded...");
						this->ongoingcall = 2;
						eXosip_unlock();
						counter = 0;
						did = je->did;
						break;
					}
				}
				break;
			case 2:
				TspTrace("Ongoing call = 2");
				counter ++;
				if (je != NULL) {
					if (je->type == EXOSIP_CALL_CLOSED) {
						TspTrace("EXOSIP_CALL_CLOSED received: (cid=%i did=%i) '%s'",je->cid,je->did,je->textinfo);
						this->ongoingcall = 0;
						counter = 0;
						if ( this->lineEvent != 0 ) {
							TSPTRACE("sending LINECALLSTATE_DISCONNECTED ...");
							this->lineEvent( this->htLine, this->htCall,
								LINE_CALLSTATE, LINECALLSTATE_DISCONNECTED,
								0, 0 /*or should iI use LINEMEDIAMODE_UNKNOWN ?*/);
						}
						if ( this->lineEvent != 0 ) {
							TSPTRACE("sending LINECALLSTATE_IDLE ...");
							this->lineEvent( this->htLine, this->htCall,
								LINE_CALLSTATE, LINECALLSTATE_IDLE,
								0, 0 /*or should iI use LINEMEDIAMODE_UNKNOWN ?*/);
						}
						break;
					}
					if (je->type == EXOSIP_CALL_RELEASED) {
						TspTrace("EXOSIP_CALL_RELEASED received: (cid=%i did=%i) '%s'",je->cid,je->did,je->textinfo);
						this->ongoingcall = 0;
						counter = 0;
						if ( this->lineEvent != 0 ) {
							TSPTRACE("sending LINECALLSTATE_DISCONNECTED ...");
							this->lineEvent( this->htLine, this->htCall,
								LINE_CALLSTATE, LINECALLSTATE_DISCONNECTED,
								0, 0 /*or should iI use LINEMEDIAMODE_UNKNOWN ?*/);
						}
						if ( this->lineEvent != 0 ) {
							TSPTRACE("sending LINECALLSTATE_IDLE ...");
							this->lineEvent( this->htLine, this->htCall,
								LINE_CALLSTATE, LINECALLSTATE_IDLE,
								0, 0 /*or should iI use LINEMEDIAMODE_UNKNOWN ?*/);
						}
						break;
					}
				}
				TspTrace("counter = '%i'", counter);
				if (counter < 20) //wait 20 cycles to make sure the ACK is received
					break;
				counter = 0;
				osip_message_t *refer;
				eXosip_lock();
				TspTrace("building REFER ...");
				i = eXosip_call_build_refer(this->did, this->to.data(), &refer);
				if (i != 0) {
					TspTrace("eXosip_call_build_refer failed...");
					eXosip_unlock();
					break;
				}
				TspTrace("eXosip_call_build_refer succeeded...");
				i = osip_message_set_header(refer, "Referred-By", this->from.data());
				if (i != 0) {
					TspTrace("osip_message_set_header failed...");
					eXosip_unlock();
					break;
				}
				TspTrace("osip_message_set_header succeeded...");
				i = eXosip_call_send_request(this->did, refer);
				if (i != 0) {
					TspTrace("eXosip_call_send_request failed...");
					eXosip_unlock();
					break;
				}
				TspTrace("eXosip_call_send_request succeeded...");
				this->ongoingcall = 3;
				eXosip_unlock();
				TspTrace("...sending REFER done ...");
				if ( this->lineEvent != 0 ) {
					TSPTRACE("sending LINECALLSTATE_ONHOLDPENDTRANSFER ...");
					this->lineEvent( this->htLine, this->htCall,
						LINE_CALLSTATE, LINECALLSTATE_ONHOLDPENDTRANSFER,
						0, 0 /*or should iI use LINEMEDIAMODE_UNKNOWN ?*/);
				}
				break;
			case 3:
				TspTrace("Ongoing call = 3");
				if (je != NULL) {
					if (je->type == EXOSIP_CALL_CLOSED) {
						TspTrace("EXOSIP_CALL_CLOSED received: (cid=%i did=%i) '%s'",je->cid,je->did,je->textinfo);
						this->ongoingcall = 0;
						if ( this->lineEvent != 0 ) {
							TSPTRACE("sending LINECALLSTATE_DISCONNECTED ...");
							this->lineEvent( this->htLine, this->htCall,
								LINE_CALLSTATE, LINECALLSTATE_DISCONNECTED,
								0, 0 /*or should iI use LINEMEDIAMODE_UNKNOWN ?*/);
						}
						if ( this->lineEvent != 0 ) {
							TSPTRACE("sending LINECALLSTATE_IDLE ...");
							this->lineEvent( this->htLine, this->htCall,
								LINE_CALLSTATE, LINECALLSTATE_IDLE,
								0, 0 /*or should iI use LINEMEDIAMODE_UNKNOWN ?*/);
						}
						break;
					}
					if (je->type == EXOSIP_CALL_RELEASED) {
						TspTrace("EXOSIP_CALL_RELEASED received: (cid=%i did=%i) '%s'",je->cid,je->did,je->textinfo);
						this->ongoingcall = 0;
						if ( this->lineEvent != 0 ) {
							TSPTRACE("sending LINECALLSTATE_IDLE ...");
							this->lineEvent( this->htLine, this->htCall,
								LINE_CALLSTATE, LINECALLSTATE_IDLE,
								0, 0 /*or should iI use LINEMEDIAMODE_UNKNOWN ?*/);
						}
						break;
					}
					// if (je->type == EXOSIP_CALL_REFER_STATUS) { // old exosip2 version
					if (je->type == EXOSIP_CALL_MESSAGE_NEW) { // new exosip2 version
						TspTrace("EXOSIP_CALL_MESSAGE_NEW received: (cid=%i did=%i) '%s'",je->cid,je->did,je->textinfo);
						// check if request is a NOTIFY
						if (0 != osip_strcasecmp (je->request->sip_method, "NOTIFY")) {
							TspTrace("non NOTIFY received, ignore ...");
							break;
						}
						// compare NOTIFY did with previous did
						if (je->did != this->did) {
							eXosip_lock();
							TspTrace("NOTIFY does not match the INVITE/REFER dialog ... 481");
							if (0 != eXosip_call_send_answer(je->tid, 481, NULL)) {
								TspTrace("error: eXosip_call_send_answer failed ...");
							}
							eXosip_unlock();
							break;
						}
						// respond to NOTIFY with 200 OK
						osip_message_t *answer;
						eXosip_lock();
						TspTrace("building answer to NOTIFY ...\n");
						if (0 != eXosip_call_build_answer(je->tid, 200, &answer)) {
							TspTrace("eXosip_call_build_answer failed...\n");
						} else {
							TspTrace("eXosip_call_build_answer succeeded...\n");
							if (0 != eXosip_call_send_answer(je->tid, 200, answer)) {
								TspTrace("eXosip_call_send_answer failed...\n");
							} else {
								TspTrace("sending answer ... done\n");
							}
						}
						eXosip_unlock();
						//ToDo: check sipfrag response code
						TspTrace("sending BYE (ToDo: check sipfrag response code)..");
						eXosip_lock();
						i = eXosip_call_terminate(je->cid, je->did);
						if (i != 0) {
							TspTrace("eXosip_call_terminate failed...");
							eXosip_unlock();
							break;
						}
						eXosip_unlock();
						TspTrace("eXosip_call_terminate succeeded...");
						if ( this->lineEvent != 0 ) {
							TSPTRACE("sending LINECALLSTATE_DISCONNECTED ...");
							this->lineEvent( this->htLine, this->htCall,
								LINE_CALLSTATE, LINECALLSTATE_DISCONNECTED,
								0, 0 /*or should iI use LINEMEDIAMODE_UNKNOWN ?*/);
						}
						break;
					}
				}
				break;
			case -1:
				TspTrace("ongoingcall == -1, stoping thread, break for() loop ...");
				break;
			case 4:
				TspTrace("ongoingcall == 4, makeCall reserved Line for new INVITE ...");
				break;
			default:
				TspTrace("Error: unkown ongoingcall state...");
				if (je != NULL) 
					eXosip_event_free(je);
		}
//		TspTrace("end of switch()");
//		TspTrace("this->ongoingcall after switch() = '%i'",this->ongoingcall);

		if (je != NULL) 
			eXosip_event_free(je);

		if (this->ongoingcall == -1) { //stop thread
			TspTrace("ongoingcall == -1, stoping thread...");
			break;
		}
	}

	TspTrace("end of for()");

/*	std::string strData;

	//make sure we are logged in
	//this->astConnect();
	//this->login();

	//TODO perhaps some checking on the login - although the connection is 
	//dropped by Asterisk if the login is not accepted
	astEvent *ev;
	while ( (ev = this->waitForMessage()) != NULL )
	{


		TSPTRACE("Unique ID: %s\r\n",ev->uniqueid.c_str());
		TSPTRACE("Unique1 ID: %s\r\n",ev->uniqueid1.c_str());
		TSPTRACE("Unique2 ID: %s\r\n",ev->uniqueid2.c_str());
		TSPTRACE("Caller ID: %s\r\n",ev->callerId.c_str());
		TSPTRACE("Channel: %s\r\n",ev->channel.c_str());
		TSPTRACE("Context: %s\r\n",ev->context.c_str());
		TSPTRACE("Event: %i\r\n",ev->event);
		TSPTRACE("Extension: %s\r\n",ev->extension.c_str());
		TSPTRACE("Priority: %s\r\n",ev->priority.c_str());
		TSPTRACE("State: %i\r\n",ev->state);

		//First off find the associated call object
		tspMut.Lock();    //ensure we are thread safe again.

		//first off we see if we are already interested in this channel - i.e.
		//already have this unique id stored
		mapCall::iterator it;
		TSPTRACE("Number of entries in trackCalls %i\r\n",trackCalls.size());

		for( it = trackCalls.begin() ; it != trackCalls.end() ; it++ )
		{
			TSPTRACE("unique stored in map id is :%s\r\n", (*it).first.c_str() );

			TSPTRACE("Comparing :%s\r\n", ev->uniqueid.c_str());
			if ( ev->uniqueid == (*it).first )
			{
				break;
			}
			TSPTRACE("Comparing :%s\r\n", ev->uniqueid1.c_str());
			if ( ev->uniqueid1 == (*it).first )
			{
				break;
			}
			TSPTRACE("Comparing :%s\r\n", ev->uniqueid2.c_str());
			if ( ev->uniqueid2 == (*it).first )
			{
				break;
			}
		}

		if ( it == trackCalls.end() )
		{
			//If we don't find the unique id stored then we need to see if it is 
			//a channel we are interested in, this is donw with our inbound channel string
			//string compare or regex
			TSPTRACE("looking for channel");

			std::string uchan(this->incomingChannel);
			std::string incom(ev->channel);

			if ( this->useInComingRegex == true )
			{
				try
				{
//					boost::regex e(uchan);
//					boost::smatch what;

//					if(!boost::regex_match(incom, what, e, boost::match_extra))
					{
						TSPTRACE("didn't find channel(regex)\r\n");
						//fetch our next message
						delete ev;
						tspMut.Unlock();
						continue;
					}
				}
				catch(std::exception e)
				{
					TSPTRACE("didn't find channel(regex) - exception caught\r\n");
					//fetch our next message
					delete ev;
					tspMut.Unlock();
					continue;
				}
			}
			else
			{
				transform(uchan.begin(), uchan.end(), uchan.begin(), std::toupper);
				transform(incom.begin(), incom.end(), incom.begin(), std::toupper);

				TSPTRACE("Looking for %s in %s\r\n",uchan.c_str(),incom.c_str());
				if ( incom.find(uchan) == -1 )
				{
					TSPTRACE("didn't find channel\r\n");
					//fetch our next message
					delete ev;
					tspMut.Unlock();
					continue;
				}
			}
			TSPTRACE("Found channel\r\n\r\n");
			astTspGlue tmp;
			//if it doesn't exsist, then we find one without a unique ID
			it = trackCalls.find("");

			if ( it == trackCalls.end() )
			{
				//If non of those exsiste then we create a new one - and signal the 
				//new call to TAPI.
				TSPTRACE("new call detected %s uniqueID",ev->uniqueid.c_str() );
				tmp.setAstChannelID(ev->channel);
				tmp.setTapiLine(this->htLine);
				tmp.setLineEvent(this->lineEvent);
				tmp.signalTapiNewCall();
				//need to check that this is actually incoming - not dialed.
				if ( ev->event == evNewchannel && ev->state == stRing )
				{
					tmp.signalTapiOutgoing();
					//tmp.signalTapiCallOffering();
				}
				else
				{
					tmp.signalTapiCallOffering();
				}
				TSPTRACE("Updating unique id :%s\r\n", ev->uniqueid.c_str());
				trackCalls[ev->uniqueid] = tmp;
			}
			else
			{
				//set the key to the unique ID - howto - for now just copy out and back in??
				TSPTRACE("inserting unique ID :%s\r\n", ev->uniqueid.c_str() );
				tmp = (*it).second;
				tmp.setAstChannelID(ev->channel);
				trackCalls.erase("");
				trackCalls[ev->uniqueid] = tmp;
				
			}
			//now make sure our iterator is correct.
			mapCall::iterator it = trackCalls.find(ev->uniqueid);

			if ( it == trackCalls.end() )
			{
				TSPTRACE("ERROR inserted but cannot find\r\n");
			}
		}
		tspMut.Unlock();

		//when we get to here we must the call object in some fashion.

		TSPTRACE("Looking for event :%i\r\n", ev->event);
		//if this is a channel we are interested in...
		if ( ev->event == evHangup )
		{
			TSPTRACE("Hangup\r\n");
			//Signal TAPI
			(*it).second.signalTapiIdle();
			//remove it from our memory
			tspMut.Lock();
			trackCalls.erase(it);
			tspMut.Unlock();
			
		}
		//The states we are interested in passing up to TAPI.
		else if ( ev->state == stRinging )
		{
			TSPTRACE("Caller ID is %s\r\n",ev->callerId.c_str() );
			if ( ev->callerId != "" )
			{
				//if ( ev->callerId.find("unknown") != -1 )
				//{
				(*it).second.signalTapiCallerID(ev->callerId);
				//}
			}

			(*it).second.signalTapiRinging();
		}
		else if ( ev->state == stUp )
		{
			TSPTRACE("Channel is up!!!!\r\n");
			//(*it).second.signalTapiConnected();
		}
		else if ( ev->event == evLink )
		{
			TSPTRACE("Channel is up (link)!!!!\r\n");
			(*it).second.signalTapiConnected();
		}
		else if ( ev->event == evNewexten )
		{
			TSPTRACE("Calling extension");
			(*it).second.signalTapiDialing(ev->extension);
		}
		
		delete ev;
	}

	*/

	TSPTRACE("completing tapiAstManager::processMessages, ExitThread()");
	ExitThread(0);
	//return 0;
}

DWORD tapiAstManager::addCall(astTspGlue call)
{
	BEGIN_PARAM_TABLE("TSPI_lineMakeCall")
		DWORD_IN_ENTRY(0)
	END_PARAM_TABLE()

	//we insert into th elist of possible calls
	//we have no defining link between this one and 
	//another... what to do!
	call.setTapiLine(this->htLine);
	call.setLineEvent(this->lineEvent);
	tspMut.Lock();
	trackCalls[""] = call;
	tspMut.Unlock();

	return EPILOG(0);
}

astTspGlue * tapiAstManager::findCall(HDRVCALL tspCallID)
{
	tspMut.Lock();
	mapCall::iterator it;

	for ( it = this->trackCalls.begin() ; it != this->trackCalls.end() ; it ++ )
	{
		if ( (*it).second.getTspiCallID() == tspCallID )
		{
			tspMut.Unlock();
			return &(*it).second;
		}

	}
	
	tspMut.Unlock();
	return NULL;
}

DWORD tapiAstManager::getNumCalls(void)
{
	return this->trackCalls.size();
}
DWORD tapiAstManager::dropCall(HDRVCALL tspiID)
{
	astTspGlue *ourCall;
	TspTrace("tapiAstManager::dropCall...");

	if ( ( ourCall = this->findCall(tspiID)) != NULL )
	{
		TspTrace("call found, drop channel...");
		this->dropChannel(ourCall->getAstChannelID());
	}

	return 0;
}


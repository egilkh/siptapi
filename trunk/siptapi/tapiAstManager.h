/*
  Name: tapiastmanager.h
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

#pragma once
#include "astmanager.h"
#include "asttspglue.h"
#include <tapi.h>
#include <tspi.h>
#include <string>
#include <map>

#ifndef TAPIASTMANAGER
#define TAPIASTMANAGER

class tapiAstManager :
	public astManager
{
public:
	tapiAstManager(void);
	~tapiAstManager(void);

	DWORD getNumCalls(void);

private:
	typedef std::map<std::string, astTspGlue> mapCall;
	mapCall trackCalls;
public:
	DWORD processMessages(void);
	DWORD addCall(astTspGlue call);
	astTspGlue * findCall(HDRVCALL tspCallID);
	DWORD dropCall(HDRVCALL tspiID);
};


#endif
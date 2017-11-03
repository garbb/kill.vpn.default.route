#include <winsock2.h>
#include <iphlpapi.h>
#include <string>
#include <vector>
#include <sstream>

using namespace std;

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

//flag to do pre-exit processing and then exit
volatile bool exitflag = false;
// needed for NotifyRouteChange
OVERLAPPED overlap;
HANDLE hand = NULL;

class DefaultRouteSplitter {
	private:
		//(store copy of default route. will modify dest and mask to make routes and later to re-create default route)
		PMIB_IPFORWARDROW pRowToSplit;
		
		//arrays of destination addresses and masks for split routes
		vector<string> destIPs;
		vector<string> maskIPs;
		
		//how many routes
		int numOfRoutes;
		
		int addModifiedRoute(const char* dest, const char* mask) {
			if (this->pRowToSplit == NULL) {
				printf("error: no stored default route\n");
				return 1;
			}
			
			this->pRowToSplit->dwForwardDest = inet_addr(dest);
			this->pRowToSplit->dwForwardMask = inet_addr(mask);
	
			printf("adding new route for Destination=%s, Mask=%s\n\t", dest, mask);
			DWORD dwStatus = CreateIpForwardEntry(this->pRowToSplit);
			if (dwStatus == ERROR_SUCCESS)
				printf("route add success\n");
			else if (dwStatus == ERROR_INVALID_PARAMETER)
				printf("Invalid parameter.\n");
			else {
				printf("route add error: %d", dwStatus);
				if (dwStatus == 5010) {
					printf(" (route already exists)\n");
				}
				else {
					printf("\n");
				}
			}
		}
		
		int deleteModifiedRoute(const char* dest, const char* mask) {
			if (this->pRowToSplit == NULL) {
				printf("error: no routes to delete\n");
				return 1;
			}
			
			this->pRowToSplit->dwForwardDest = inet_addr(dest);
			this->pRowToSplit->dwForwardMask = inet_addr(mask);
	
			printf("deleting route for Destination=%s, Mask=%s\n\t", dest, mask);
			DWORD dwStatus = DeleteIpForwardEntry(this->pRowToSplit);
			if (dwStatus == ERROR_SUCCESS)
				printf("delete success\n");
			else if (dwStatus == ERROR_INVALID_PARAMETER)
				printf("Invalid parameter.\n");
			else {
				printf("route delete error: %d", dwStatus);
				if (dwStatus == 1168) {
					printf(" (route does not exist)\n");
				}
				else {
					printf("\n");
				}
			}
		}
	
	public:
		DefaultRouteSplitter() {
			this->pRowToSplit = NULL;
		}
		
		int splitDefaultRoute(int index, vector<string> newDestIPs, vector<string> newMaskIPs) {
				
			/* variables used for GetIfForwardTable */
		    PMIB_IPFORWARDTABLE pIpForwardTable;
		    DWORD dwSize = 0;
		    //PMIB_IPFORWARDROW pRow = NULL;
		    DWORD dwStatus = 0;
		    
		    pIpForwardTable = (MIB_IPFORWARDTABLE *) malloc(sizeof (MIB_IPFORWARDTABLE));
		    if (pIpForwardTable == NULL) {
		        printf("Error allocating memory\n");
		        return 1;
		    }
		    
		    if (GetIpForwardTable(pIpForwardTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER) {
		        free(pIpForwardTable);
		        pIpForwardTable = (MIB_IPFORWARDTABLE *) malloc(dwSize);
		        if (pIpForwardTable == NULL) {
		            printf("Error allocating memory\n");
		            return 1;
		        }
		    }
		    
		    if (GetIpForwardTable(pIpForwardTable, &dwSize, 0) != ERROR_SUCCESS) {
				printf("GetIpForwardTable failed.\n");
				free(pIpForwardTable);
				return 1;
			}
			
			bool foundDefaultVPNRoute = false;
			for (int i = 0; i < (int) pIpForwardTable->dwNumEntries; i++) {
				
				//if default route (==0), and index is the index we are interested in
				if (pIpForwardTable->table[i].dwForwardDest == 0 && pIpForwardTable->table[i].dwForwardIfIndex == index) {
					foundDefaultVPNRoute = true;
					printf("found default route for interface index %d\n\t", index);
		
					//need to put ipaddress into in_addr struct in order to be able to use inet_ntoa() to convert it to a string
//					in_addr gateway_in_addr;
//					gateway_in_addr.S_un.S_addr = pIpForwardTable->table[i].dwForwardNextHop;
//					printf("Gateway=%s ", inet_ntoa(gateway_in_addr));
						
					//copy route row to class member so we can change it a bit and re-add it later
					this->pRowToSplit = (PMIB_IPFORWARDROW) malloc(sizeof (MIB_IPFORWARDROW));
					if (!this->pRowToSplit) {
						printf("Malloc failed. Out of memory.\n");
						free(pIpForwardTable);
						return 1;
					}
					memcpy(this->pRowToSplit, &(pIpForwardTable->table[i]), sizeof (MIB_IPFORWARDROW));
						
					printf("deleting... ");
					dwStatus = DeleteIpForwardEntry(&(pIpForwardTable->table[i]));
					if (dwStatus != ERROR_SUCCESS) {
						printf("delete FAILED\n");
						//free(this->pRowToSplit);
						free(pIpForwardTable);
						return 1;
					}
					printf("delete success\n");
					
					//store destination ips and masks into class members
					this->destIPs = newDestIPs;
					this->maskIPs = newMaskIPs;
					this->numOfRoutes = newDestIPs.size();
					
					//add new routes for VPN sites
					for (int i = 0; i < this->numOfRoutes; i++) {
						addModifiedRoute(this->destIPs[i].c_str(), this->maskIPs[i].c_str());
					}
		
				break;	//exit for loop
				}
			
			}
			//free(this->pRowToSplit);
			free(pIpForwardTable);
			if (foundDefaultVPNRoute == false) {
				printf("Couldn't find default route for interface index %d\n", index);
				return 1;
			}
			return 0;
		}

		// restore default route and delete all the split routes that we added
		int restoreDefaultRoute() {
			
			printf("restoring default route\n");
			// default route has destination and mask = 0.0.0.0
			addModifiedRoute("0.0.0.0", "0.0.0.0");
			
			// delete new routes for VPN sites that we added previously
			for (int i = 0; i < this->numOfRoutes; i++) {
				deleteModifiedRoute(this->destIPs[i].c_str(), this->maskIPs[i].c_str());
			}
			
		}

};
//make routesplitter object
DefaultRouteSplitter vpnDefaultRouteSplitter;

BOOL IsElevated() {
    BOOL fRet = FALSE;
    HANDLE hToken = NULL;
    if( OpenProcessToken( GetCurrentProcess( ), TOKEN_QUERY, &hToken ) ) {
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof( TOKEN_ELEVATION );
        if( GetTokenInformation( hToken, TokenElevation, &Elevation, sizeof( Elevation ), &cbSize ) ) {
            fRet = Elevation.TokenIsElevated;
        }
    }
    if( hToken ) {
        CloseHandle( hToken );
    }
    return fRet;
}

int getInterfaceIndexFromDesc(string Desc) {
	int Index = -1;
	
    ULONG buflen = sizeof(IP_ADAPTER_INFO);
    IP_ADAPTER_INFO *pAdapterInfo = (IP_ADAPTER_INFO *)malloc(buflen);

    if (GetAdaptersInfo(pAdapterInfo, &buflen) == ERROR_BUFFER_OVERFLOW) {
        free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO *)malloc(buflen);
    }

    if (GetAdaptersInfo(pAdapterInfo, &buflen) == ERROR_SUCCESS) {
        for (IP_ADAPTER_INFO *pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next) {
			
			string strDescription(pAdapter->Description);
			switch (strDescription.compare(Desc)) {
				case 0:
//					printf("found Interface\n");
//		            printf("desc=\"%s\"\nip=%s\nname=%s\nindex=%d\n", 
//					pAdapter->Description,
//					pAdapter->IpAddressList.IpAddress.String,
//					pAdapter->AdapterName,
//					pAdapter->Index);
					Index = pAdapter->Index;
					break;
			}
        }
    }

    if (pAdapterInfo) free(pAdapterInfo);
    return Index;
}


BOOL CtrlHandler( DWORD fdwCtrlType ) {
	  switch( fdwCtrlType ) {
	  	case CTRL_C_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			exitflag = true;

			// unregister to route change events
			CancelIPChangeNotify(&overlap);
			WSAResetEvent(overlap.hEvent);
			
			vpnDefaultRouteSplitter.restoreDefaultRoute();
			
			Sleep(1000);
			return FALSE;
	  		
	  	default:
	  		return FALSE;
	  }
}

//funcs for string splitting
template<typename Out>
void split(const std::string &s, char delim, Out result) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        *(result++) = item;
    }
}
std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
}

int main() {
	//check that this is run as admin
	if (!IsElevated()) {
		printf("This program must be run as administrator\n");
		system("pause");
		return 1;
	}
	
	
	//get folder that this exe is in
	char thisexepath[MAX_PATH]; //or wchar_t * buffer;
    GetModuleFileName(NULL, thisexepath, MAX_PATH);
    
	std::vector<std::string> thisexepath_v = split(thisexepath, '\\');
	std::string exefolder;
	for (int idx = 0; idx < thisexepath_v.size() - 1; idx++) {
		exefolder += thisexepath_v[idx] + "\\";
	}
	
	
	//ini filename
	std::string inifilename (exefolder + "config.ini");
	//printf("%s\n", inifilename.c_str());
	//ini section name
	std::string inisectionname ("config");
	//base names for ini keys
	std::string addrbasekeyname ("addr");
	std::string maskbasekeyname ("mask");
	// max num of keybasenameX to check
	int max_keynum = 100;
	
	std::string keyname;

	//buffers to temp hold number to be appended to keybasename and value read from ini
	char numstring[3];
	char valuestring[32];
	
	
	//make string vectors to hold addr and mask strings
	vector<string> newDestIPs_v;
	vector<string> newMaskIPs_v;
	//read addresses and masks from ini
	//try to read keybasenameX from 1 thru max_keynum
	for (int keynum = 1; keynum <= max_keynum; keynum++) {
	
		//convert keynum to string and append to keybasename
		keyname = addrbasekeyname + itoa(keynum, numstring, 10);
		//read value from ini. if not found, valuestring will get zero-length string "" and we will stop trying to read more keys
		GetPrivateProfileString(inisectionname.c_str(), keyname.c_str(), NULL, valuestring, sizeof(valuestring) / sizeof(valuestring[0]), inifilename.c_str());
		if (*valuestring == 0) {
			//printf("%s NOT FOUND\n", keyname.c_str());
			break;
		}
		//printf("%s = %s\n", keyname.c_str(), valuestring);
		//add value to end of valuevector
		newDestIPs_v.push_back(valuestring);
		
		//convert keynum to string and append to keybasename
		keyname = maskbasekeyname + itoa(keynum, numstring, 10);
		//read value from ini. if not found, valuestring will get zero-length string "" and we will stop trying to read more keys
		GetPrivateProfileString(inisectionname.c_str(), keyname.c_str(), NULL, valuestring, sizeof(valuestring) / sizeof(valuestring[0]), inifilename.c_str());
		if (*valuestring == 0) {
			//printf("%s NOT FOUND\n", keyname.c_str());
			newDestIPs_v.pop_back();	//delete last addr we added since there is no mask to go with it
			break;
		}
		//printf("%s = %s\n", keyname.c_str(), valuestring);
		//add value to end of valuevector
		newMaskIPs_v.push_back(valuestring);
	
	}
	
	GetPrivateProfileString(inisectionname.c_str(), "VPNDesc", NULL, valuestring, sizeof(valuestring) / sizeof(valuestring[0]), inifilename.c_str());
	//printf("%s\n", valuestring);
	if (*valuestring == 0) {
		printf("VPNDesc NOT FOUND\n");
		return 1;
	}
	string VPNDesc = valuestring;
	
	//printf("\n");
	
	//just output valuevector elements
//		for (int idx = 0; idx < newDestIPs_v.size(); idx++) {
//			printf("%s\n", newDestIPs_v[idx].c_str());
//			printf("%s\n", newMaskIPs_v[idx].c_str());
//		}
//	return 0;
	
	//set handler for close events
	SetConsoleCtrlHandler( (PHANDLER_ROUTINE) CtrlHandler, TRUE );

	//set initial route change notification
	overlap.hEvent = WSACreateEvent();
	NotifyRouteChange(&hand, &overlap);

	//main loop checking for route change
	while (true) {
		
		time_t t = time(NULL);
    	char mbstr[100];
    	strftime(mbstr, sizeof(mbstr), "%Y/%m/%d %H:%M:%S", localtime(&t));
        printf("%s\n", mbstr);

		int VPNIndex = getInterfaceIndexFromDesc(VPNDesc);
		if (VPNIndex != -1) {
			printf("Found %s index: %d\n", VPNDesc.c_str(), VPNIndex);
		}
		else {
			//this happens a bunch after resuming from sleep for some reason...
			printf("Couldn't find VPN interface\n");
			goto endloop;	// if we can't find the interface then just skip past changing routes
//			system("pause");
//			return 1;
		}
		
		//delete default route and replace with non-default routes
		vpnDefaultRouteSplitter.splitDefaultRoute(VPNIndex, newDestIPs_v, newMaskIPs_v);
		
		endloop:
			
		printf("\n");
		
		// this will block until route changes, at which point it will run everything in the loop again
		WaitForSingleObject(overlap.hEvent, INFINITE);
		
		// just get stuck here until close event finishes and the program exits
		if (exitflag) {
			while (true) { };
		}
		
		// must call this again to get next event
		NotifyRouteChange(&hand, &overlap);
		
		printf("TCP route change detected\n");
		
	}
}


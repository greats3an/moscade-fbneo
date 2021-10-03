#include <moscade.h>
#include <thread>
using namespace std;
#define BUFSIZE 4096 

struct GGPONexusProxy {	
	TCHAR sNexusCmd[1024] = { 0 };

	GGPONexusProxy(char* args) {	
		ANSIToTCHAR(args, sNexusCmd,strlen(args) + 1);
	}

	void Start() {
		TCHAR sNexusPath[1024] = { 0 };
		swprintf(sNexusPath, L"%s\\bin", GetExecutableDirectroy());
		ShellExecute(hScrnWnd, L"open", L"nexus.exe" , sNexusCmd, sNexusPath,SW_SHOWNORMAL);
		dprintf(L"** Nexus : Initialized w/cmd %s %s.\n", sNexusPath, sNexusCmd);
	}

};
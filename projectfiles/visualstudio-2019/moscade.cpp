#include <iostream>
#include <fstream>
#include <windows.h>

using namespace std;

char* GetGGPONetHost() {
	fstream dll("ggponet.dll", ios::binary | ios::in);
	char buffer[0x12] = { 0 };
	dll.seekp(0x32152);
	dll.read(buffer, 0x12);
	return buffer;
}

void SpawnOverwriteProcess(LPCWSTR mcade) {
	ShellExecute(NULL, L"open", L"externalwriter.exe", mcade, NULL, SW_SHOWNORMAL);
}

char* ConvertLPWSTRToLPSTR(LPWSTR lpwszStrIn)
{	
	int nInputStrLen = wcslen(lpwszStrIn);
	// Double NULL Termination
	int nOutputStrLen = WideCharToMultiByte(CP_UTF8, 0, lpwszStrIn, -1, NULL, 0, NULL, NULL) + 2;
	char * pszOut = new char[nOutputStrLen];	
	memset(pszOut, 0x00, nOutputStrLen);
	WideCharToMultiByte(CP_UTF8, 0, lpwszStrIn, -1, pszOut, nOutputStrLen, NULL, NULL);
	return pszOut;
}

bool InstallHandler() {
	/* Creates a temporary .reg file,and tries to register it via user prompts*/
	wchar_t buffer[MAX_PATH];
	wchar_t dbqoute[MAX_PATH];
	GetModuleFileNameW(NULL, buffer, MAX_PATH);
	GetModuleFileNameW(NULL, dbqoute, MAX_PATH);
	int l = 0, r = 0;
	while (*(buffer + l)) {
		dbqoute[r] = buffer[l];
		if (buffer[l] == '\\') {
			dbqoute[++r] = '\\';
		}
		l++; r++;
	}
	dbqoute[r] = '\0';
	OutputDebugStringW(L"Executable path:\n");
	OutputDebugStringW(dbqoute);
	wofstream tfile("register.reg");
	tfile << "Windows Registry Editor Version 5.00\n";
	tfile << "[HKEY_CLASSES_ROOT\\moscade]\n";
	tfile << "@=\"URL:moscade Protocol\"\n";
	tfile << "\"URL Protocol\"=\"\"\n";
	tfile << "[HKEY_CLASSES_ROOT\\moscade\\shell]\n\n";
	tfile << "[HKEY_CLASSES_ROOT\\moscade\\shell\\open]\n\n";
	tfile << "[HKEY_CLASSES_ROOT\\moscade\\shell\\open\\command]\n";
	tfile << "@=\"\\\"";
	tfile << dbqoute;
	tfile << "\\\" \\\"%1\\\"\"";
	ShellExecute(NULL, L"open", L"register.reg", NULL, NULL, SW_SHOWNORMAL);
	return true;
}

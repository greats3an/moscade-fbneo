#include <iostream>
#include <fstream>
#include <libloaderapi.h>
#include <debugapi.h>
using namespace std;

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
	wofstream tfile;
	tfile.open("register.reg");
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
	ShellExecute(NULL, _T("open"), _T("register.reg"), NULL, NULL, SW_SHOWNORMAL);
	return true;
}

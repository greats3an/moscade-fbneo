#include <iostream>
#include <fstream>
#include <windows.h>
#include <tchar.h>
#include <burner.h>

using namespace std;

/* The following functions are added because we are running in codepage 936*/
#define CP_GBK 936
#define NONNUL 0xF + 1 /* 0x00 -> 0x10 */

char* from_halves(const char* src) {
	int len = strlen(src);
	char* result = new char[len / 2 + 1];
	dprintf(_T("** RECV Length : %d\n** Message:"),len);
	memset(result, 0, len / 2 + 1);
	for (int i = 0; i < len; i+=2) {
		dprintf(_T("0x%x 0x%x "), src[i],src[i+1]);
		result[i / 2] = (src[i] == NONNUL ? 0 : src[i]) << 4 | (src[i + 1] == NONNUL ? 0 : src[i + 1]);
	}	
	return result;
}
char* to_halves(const char* src) {
	int len = strlen(src) * 2;
	char* result = new char[len + 2];
	dprintf(_T("** SEND Length : %d\n** Message:"), len);
	memset(result, 0, len + 2);
	for (int i = 0; i < len; i+=2) {		
		result[i] = (src[i/2] & 0xf0) >> 4;
		result[i+1] = src[i/2] & 0xf;		
		/* strlen() ends when NUL is detected, which when unpacking a non-nul char could lead to misreading
			we're using a hack to make the fake-NULs out-of-band,so server should deal with this as well
		*/
		result[i] = result[i] == 0 ? NONNUL : result[i];
		result[i + 1] = result[i + 1] == 0 ? NONNUL : result[i + 1];
		dprintf(_T("0x%x%x (0x%x), "), result[i], result[i + 1], src[i / 2]);		
	}
	return result;
}
WCHAR* wstring_from_gbk(const char* gbkstring)
{
	int char_count;
	WCHAR* result;

	char_count = MultiByteToWideChar(CP_GBK, 0, gbkstring, -1, NULL, 0);
	result = (WCHAR*)malloc(char_count * sizeof(*result));
	if (result != NULL)
		MultiByteToWideChar(CP_GBK, 0, gbkstring, -1, result, char_count);

	return result;
}

char* gbk_from_wstring(const WCHAR* wstring)
{	
	int len = WideCharToMultiByte(CP_GBK, 0, wstring, -1, NULL, 0, NULL, NULL);	
	char* result = new char[len + 1];
	memset(result, 0, len + 1);
	if (result != NULL)
		WideCharToMultiByte(CP_GBK, 0, wstring, -1, result, len, NULL, NULL);
	return result;
}


char* GetGGPONetHost() {
	fstream dll("ggponet.dll", ios::binary | ios::in);
	char buffer[0x12] = { 0 };
	dll.seekp(0x32152);
	dll.read(buffer, 0x12);
	return buffer;
}

WCHAR* decode_msg(char* src) {
	char* full = from_halves(src);
	WCHAR* decoded = wstring_from_gbk(full);
	return decoded;
}

void SpawnOverwriteProcess(LPCWSTR mcade) {
	ShellExecute(NULL, L"open", L"externalwriter.exe", mcade, NULL, SW_SHOWNORMAL);
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
	wofstream tfile("register.reg");
	tfile << _T("Windows Registry Editor Version 5.00\n");
	tfile << _T("[HKEY_CLASSES_ROOT\\moscade]\n");
	tfile << _T("@=\"URL:moscade Protocol\"\n");
	tfile << _T("\"URL Protocol\"=\"\"\n");
	tfile << _T("[HKEY_CLASSES_ROOT\\moscade\\shell]\n\n");
	tfile << _T("[HKEY_CLASSES_ROOT\\moscade\\shell\\open]\n\n");
	tfile << _T("[HKEY_CLASSES_ROOT\\moscade\\shell\\open\\command]\n");
	tfile << _T("@=\"\\\"");
	tfile << dbqoute;
	tfile << _T("\\\" \\\"%1\\\"\"");
	ShellExecute(NULL, L"open", L"register.reg", NULL, NULL, SW_SHOWNORMAL);
	return true;
}

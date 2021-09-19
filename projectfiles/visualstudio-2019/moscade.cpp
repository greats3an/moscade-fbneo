#include <iostream>
#include <fstream>
#include <windows.h>
#include <tchar.h>
#include <burner.h>
#include <wincrypt.h>
using namespace std;

#define CP_GBK 936

#pragma comment(lib, "crypt32.lib")
/* Decodes BASE64 ANSI string, returns NULL if failed. */
char* from_base64(const char* src) {
	DWORD len = 0;

	CryptStringToBinaryA(src, 0, CRYPT_STRING_BASE64, NULL, &len, NULL, NULL);

	BYTE* buffer = new BYTE[len + 1];

	CryptStringToBinaryA(src, 0, CRYPT_STRING_BASE64, buffer, &len, NULL, NULL);

	if (!len) return NULL;

	char* result = new char[len + 1];
	memset(result, 0, len + 1);
	memcpy(result, buffer, len);
	return result;
}

/* Encodes BASE64 ANSI string, returns NULL if failed. */
char* to_base64(const char* src) {
	DWORD len = strlen(src);
	BYTE* buffer = new BYTE[len + 1];

	memcpy(buffer, src, len);

	CryptBinaryToStringA(buffer, strlen(src), CRYPT_STRING_BASE64, NULL, &len);

	if (!len) return NULL;

	char* result = new char[len + 1];
	memset(result, 0, len + 1);
	CryptBinaryToStringA(buffer, strlen(src), CRYPT_STRING_BASE64, result, &len);

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

char* encode_msg(WCHAR* src) {
	char* encoded = gbk_from_wstring(src);
	return to_base64(encoded);
}

WCHAR* decode_msg(char* src) {
	char* full = from_base64(src);
	WCHAR* decoded = wstring_from_gbk(full);
	return decoded;
}

void SpawnOverwriteProcess(LPCWSTR mcade) {
	ShellExecute(NULL, L"open", L"externalwriter.exe", mcade, NULL, SW_SHOWNORMAL);
}


bool MOSCadeCheckIsHandlerInstalled() {
	TCHAR buffer[1024] = { 0 }; DWORD len = 1024;
	LSTATUS stat = RegGetValue(HKEY_CLASSES_ROOT, _T("moscade\\shell\\open\\command"), NULL, RRF_RT_ANY, NULL, buffer, &len);
	if (stat != 0) return false;
	// To check if the executable is ours. If not, we still flag the installation invalid.	
	wchar_t path[MAX_PATH];
	GetModuleFileNameW(NULL, path, MAX_PATH);
	int i = 1;
	bool flag = true;
	while (buffer[i] != NULL && path[i - 1] != NULL)
		if (buffer[i] != path[i++ - 1]) { flag = false; break; }
	return flag;
}

bool MOSCadePromptInstallHandler() {
	/* Creates a temporary .reg file,and prompts the user to install.*/	
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



#pragma once
char * GetGGPONetHost();
void SpawnOverwriteProcess(LPCWSTR mcade);
bool InstallHandler();
WCHAR* wstring_from_gbk(const char* gbkstring);
char* gbk_from_wstring(const WCHAR* wstring);
char* from_halves(const char* src);
char* to_halves(const char* src);
WCHAR* decode_msg(char* src);
// externalwriter.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <windows.h>
using namespace std;

TCHAR* ANSIToTCHAR(const char* pszInString, TCHAR* pszOutString, int nOutSize)
{
	static TCHAR szStringBuffer[1024];

	TCHAR* pszBuffer = pszOutString ? pszOutString : szStringBuffer;
	int nBufferSize = pszOutString ? nOutSize * 2 : sizeof(szStringBuffer);

	if (MultiByteToWideChar(CP_ACP, 0, pszInString, -1, pszBuffer, nBufferSize)) {
		return pszBuffer;
	}

	return NULL;
}

int main(int argc, char* argv[])
{
	char mode[128], game[128], host[128], dport[128], quark[128];
	#pragma warning(suppress : 4996)
	sscanf(argv[1], "moscade://%[^,],%[^,],%[^:]:%[^@]@%s", mode, game, host, dport, quark);

	cout << "Overwritting with " << host;	
	cout << "\nWaiting for main executable to exit...\n";
	char buffer[0x12] = { 0 };
	strcpy_s(buffer, host);
	while (true) {
		ofstream dll("ggponet.dll", ios::binary | ios::out | ios::in);
		dll.seekp(0x32152);
		dll.write(buffer, 0x12);		
		if (!dll.bad()) break;
		cout << dll.bad() << "...";
		Sleep(1000);
	}
	cout << "Launching moscadefbneo.exe";
	ShellExecute(NULL, L"open", L"moscadefbneo.exe", ANSIToTCHAR(argv[1],NULL,0), NULL, SW_SHOWDEFAULT);
	exit(0);
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file

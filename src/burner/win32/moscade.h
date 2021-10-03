#pragma once

struct MOSCadeURI {
	char mode[8];
	char game[128];
	char host[128];
	char quark[128];
	char dport[8] = "8000";
	int from_uri(char* uri) {
		return sscanf(uri, "moscade://%[^,],%[^,],%[^@]@%s", mode, game, host, quark);
	}

	char* to_quark() {
		char buffer[1024] = { 0 };

		if (strncmp(mode, "match", strlen("match")) == 0) {
			sprintf(buffer, "quark:served,%s,%s,%s,0,3", game, quark, dport);
		}
		else {
			sprintf(buffer, "quark:stream,%s,%s,%s", game, quark, dport);
		}

		return buffer;
	}
};

char *GetGGPONetHost();

bool MOSCadeCheckIsHandlerInstalled();
bool MOSCadePromptInstallHandler();

WCHAR *wstring_from_gbk(const char* gbkstring);
char *gbk_from_wstring(const WCHAR* wstring);

char *from_base64(const char* src);
char *to_base64(const char* src);

WCHAR *encode_msg(char* src);
WCHAR *decode_msg(char* src);

TCHAR* GetExecutableDirectroy();
#include "burner.h"
#include "ggponet.h"
#include "ggpoclient.h"
#include "ggpo_perfmon.h"
#include "moscade.h"
GGPOSession *ggpo = NULL;
bool bSkipPerfmonUpdates = false;

void QuarkInitPerfMon();
void QuarkPerfMonUpdate(GGPONetworkStats *stats);

extern int nAcbVersion;
extern int bMediaExit;
extern int nAcbLoadState;

static bool bDirect = false;
static bool bReplaySupport = false;
static bool bReplayStarted = false;
static bool bReplayRecord = false;
static bool bReplayRecording = false;
static int iRanked = 0;
static int iPlayer = 0;
static int iDelay = 0;
static int iSeed = 0;

const int ggpo_state_header_size = 6 * sizeof(int);

bool DisableForNetplay(){
	return kNetGame && kNetLua;
}

int GetHash(const char *id, int len)
{
	unsigned int hash = 1315423911;
	for (int i = 0; i < len; i++) {
		hash ^= ((hash << 5) + id[i] + (hash >> 2));
	}
	return (hash & 0x7FFFFFFF);
}

void SetBurnFPS(const char *name, int version)
{
	if (kNetVersion < NET_VERSION_60FPS) {
		// Version 1: use old framerate (59.94fps)
		nBurnFPS = 5994;
		return;
	}

	if (kNetVersion >= NET_VERSION_MK_FRAMERATE) {
		// Version 5: 
		if (!strcmp(name, "mk") || !strcmp(name, "mk2") || !strcmp(name, "mk2p") || !strcmp(name, "mk3") ||
			!strcmp(name, "umk3") || !strcmp(name, "umk3pb1") || !strcmp(name, "umk3uc")) {
			bForce60Hz = 0;
			return;
		}
	}
}

bool __cdecl ggpo_on_client_event_callback(GGPOClientEvent *info)
{
	switch (info->code)
	{
	case GGPOCLIENT_EVENTCODE_CONNECTING:
		VidOverlaySetSystemMessage(_T("连接中..."));
		VidSSetSystemMessage(_T("Connecting..."));
		break;

	case GGPOCLIENT_EVENTCODE_CONNECTED:
		VidOverlaySetSystemMessage(_T("已连接挑战调谐端"));
		VidSSetSystemMessage(_T("Connected to challenge coordinator."));
		break;

	case GGPOCLIENT_EVENTCODE_RETREIVING_MATCHINFO:
		VidOverlaySetSystemMessage(_T("下载比赛信息，请稍后..."));
		VidSSetSystemMessage(_T("Retrieving Match Info..."));
		break;

	case GGPOCLIENT_EVENTCODE_DISCONNECTED:
		VidOverlaySetSystemMessage(_T("失去连接!"));
		VidSSetSystemMessage(_T("Disconnected!"));
		QuarkFinishReplay();
		break;

	case GGPOCLIENT_EVENTCODE_MATCHINFO: {
		VidOverlaySetSystemMessage(_T(""));
		VidSSetSystemMessage(_T(""));
		if (kNetSpectator) {
			kNetVersion = strlen(info->u.matchinfo.blurb) > 0 ? atoi(info->u.matchinfo.blurb) : NET_VERSION;
		}
		SetBurnFPS(TCHARToANSI(BurnDrvGetText(DRV_NAME), NULL, 0), kNetVersion);
		/* expect such encoding since this will only be sent by ggposrv3*/
		TCHAR szUser1[128] = { 0 };
		wcscpy_s(szUser1,decode_msg(info->u.matchinfo.p1));
		TCHAR szUser2[128] = { 0 };
		wcscpy_s(szUser2, decode_msg(info->u.matchinfo.p2));

		dprintf(_T("** P1 ：%s , P2 : %s\n"), szUser1, szUser2);
		VidOverlaySetGameInfo(szUser1, szUser2 , kNetSpectator, iRanked, iPlayer);
		VidSSetGameInfo(szUser1, szUser2 , kNetSpectator, iRanked, iPlayer);
		break;
	}

	case GGPOCLIENT_EVENTCODE_SPECTATOR_COUNT_CHANGED:
		VidOverlaySetGameSpectators(info->u.spectator_count_changed.count);
		VidSSetGameSpectators(info->u.spectator_count_changed.count);
		break;

	case GGPOCLIENT_EVENTCODE_CHAT:
		if (strlen(info->u.chat.text) > 0) {
			TCHAR szUser[128] = { 0 };
			TCHAR szText[1024] = { 0 };			
			wcscpy_s(szUser, ANSIToTCHAR(info->u.chat.username,NULL,0));
			if (wcscmp(szUser, _T("System")) == 0){
				// Everything sent by system are plain ANSI text
				wcscpy_s(szText, ANSIToTCHAR(info->u.chat.text, NULL, 0));				 
				// Handling commands
				if (wcscmp(szText, _T("client_leave")) == 0) {
					// our client left the server, emulator should be terminated with it
					dprintf(_T("** GGPO closing manaully.\n"));					
					exit(0);
				}
				break; 
			} else {
				// chat message, encoded by gbk w/ halfstring				
				wcscpy_s(szUser, decode_msg(info->u.chat.username));
				wcscpy_s(szText, decode_msg(info->u.chat.text));
			}
			dprintf(_T("** %s : %s\n"), szUser, szText);
			VidOverlayAddChatLine(szUser, szText);
			VidSAddChatLine(szUser, 0XFFA000, szText, 0xEEEEEE);
		}
		break;

	default:
		break;
	}
	return true;
}

bool __cdecl ggpo_on_client_game_callback(GGPOClientEvent *info)
{
	// DEPRECATED
	return true;
}

bool __cdecl ggpo_on_event_callback(GGPOEvent *info)
{
	if (ggpo_is_client_eventcode(info->code)) {
		return ggpo_on_client_event_callback((GGPOClientEvent *)info);
	}
	if (ggpo_is_client_gameevent(info->code)) {
		return ggpo_on_client_game_callback((GGPOClientEvent *)info);
	}
	switch (info->code) {
	case GGPO_EVENTCODE_CONNECTED_TO_PEER:
		VidOverlaySetSystemMessage(_T("尝试与对手建立连接..."));
		VidSSetSystemMessage(_T("Connected to Peer"));
		break;

	case GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER:
		//_stprintf(status, _T("Synchronizing with Peer (%d/%d)..."), info->u.synchronizing.count, info->u.synchronizing.total);
		VidOverlaySetSystemMessage(_T("重新同步"));
		VidSSetSystemMessage(_T("Synchronizing with Peer..."));
		break;

	case GGPO_EVENTCODE_RUNNING: {
		VidOverlaySetSystemMessage(_T(""));
		VidSSetSystemMessage(_T(""));
		// send ReceiveVersion message
		char temp[16];
		sprintf(temp, "%d", NET_VERSION);
		QuarkSendChatCmd(temp, 'V');
		break;
	}

	case GGPO_EVENTCODE_DISCONNECTED_FROM_PEER:
		exit(0); // kills our emulator when peer disconnects
		VidOverlaySetSystemMessage(_T("你与对手已断开连接！"));		
		VidSSetSystemMessage(_T("Disconnected from Peer"));
		if (bReplayRecording) {
			AviStop();
			bMediaExit = true;
		}
		SetPauseMode(1);
		break;

	case GGPO_EVENTCODE_TIMESYNC:
		break;

	default:
		break;
	}

	return true;
}

bool __cdecl ggpo_begin_game_callback(char *name)
{
	WIN32_FIND_DATA fd;
	TCHAR tfilename[MAX_PATH];
	TCHAR tname[MAX_PATH];
	ANSIToTCHAR(name, tname, MAX_PATH);
	SetBurnFPS(name, kNetVersion);

	// ranked savestate
	if (iRanked) {
		_stprintf(tfilename, _T("savestates\\%s_fbneo_ranked.fs"), tname);
		if (FindFirstFile(tfilename, &fd) != INVALID_HANDLE_VALUE) {
			// Load our save-state file (freeplay, event mode, etc.)
			BurnStateLoad(tfilename, 1, &DrvInitCallback);
			// if playing a direct game, we never get match information, so put anonymous
			if (bDirect) {
				VidOverlaySetGameInfo(_T("Player1#0,0"), _T("Player2#0,0"), false, iRanked, iPlayer);
				VidSSetGameInfo(_T("Player1#0,0"), _T("Player2#0,0"), false, iRanked, iPlayer);
			}
			// detector			
			DetectorLoad(name, false, iSeed);
			return 0;
		}
	}

	// regular savestate
	_stprintf(tfilename, _T("savestates\\%s_fbneo.fs"), tname);
	if (FindFirstFile(tfilename, &fd) != INVALID_HANDLE_VALUE) {
		// Load our save-state file (freeplay, event mode, etc.)
		BurnStateLoad(tfilename, 1, &DrvInitCallback);
		// if playing a direct game, we never get match information, so put anonymous
		if (bDirect) {
			VidOverlaySetGameInfo(_T("Player1#0,0"), _T("Player2#0,0"), false, iRanked, iPlayer);
			VidSSetGameInfo(_T("Player1#0,0"), _T("Player2#0,0"), false, iRanked, iPlayer);
		}
		// detector			
		DetectorLoad(name, false, iSeed);
		return 0;
	}

	// no savestate
	UINT32 i;
	for (i = 0; i < nBurnDrvCount; i++) {
		nBurnDrvActive = i;
		if ((_tcscmp(BurnDrvGetText(DRV_NAME), tname) == 0) && (!(BurnDrvGetFlags() & BDF_BOARDROM))) {
			MediaInit();
			DrvInit(i, true);		
			// if playing a direct game, we never get match information, so play anonymous
			if (bDirect) {
				VidOverlaySetGameInfo(_T("player 1#0,0"), _T("player 2#0,0"), false, iRanked, iPlayer);
				VidSSetGameInfo(_T("Player1#0,0"), _T("Player2#0,0"), false, iRanked, iPlayer);
			}
			// detector			
			DetectorLoad(name, false, iSeed);
			return 1;
		}
	}

	return 0;
}

bool __cdecl ggpo_advance_frame_callback(int flags)
{
	dprintf(_T("** GGPO frame advance\n"));
	bSkipPerfmonUpdates = true;
	nFramesEmulated--;
	RunFrame(1, 0, 0);
	bSkipPerfmonUpdates = false;
	return true;
}


void QuarkProcessEndOfFrame()
{
}

static char gAcbBuffer[16 * 1024 * 1024];
static char *gAcbScanPointer;
static int gAcbChecksum;
static FILE *gAcbLogFp;

void ComputeIncrementalChecksum(struct BurnArea *pba)
{
	/*
	 * Ignore checksums in release builds for now.  It takes a while.
	 */
#if defined(FBA_DEBUG)
	int i;

#if 0
	static char *soundAreas[] = {
	   "Z80",
	   "YM21",
	   "nTicksDone",
	   "nCyclesExtra",
	};
	/*
	 * This is a really crappy checksum routine, but it will do for
	 * our purposes
	 */
	for (i = 0; i < ARRAYSIZE(soundAreas); i++) {
		if (!strncmp(soundAreas[i], pba->szName, strlen(soundAreas[i]))) {
			return;
		}
	}
#endif

	for (i = 0; i < pba->nLen; i++) {
		int b = ((unsigned char *)pba->Data)[i];
		if (b) {
			if (i % 2)
				gAcbChecksum *= b;
			else
				gAcbChecksum += b * 317;
		}
		else
			gAcbChecksum++;
	}
#endif
}

static int QuarkLogAcb(struct BurnArea* pba)
{
	fprintf(gAcbLogFp, "%s:", pba->szName);

	int col = 10, row = 30;
	for (int i = 0; i < (int)pba->nLen; i++) {
		if ((i % row) == 0)
			fprintf(gAcbLogFp, "\noffset %9d :", i);
		else if ((i % col) == 0)
			fprintf(gAcbLogFp, " - ");
		fprintf(gAcbLogFp, " %02x", ((unsigned char*)pba->Data)[i]);
	}
	fprintf(gAcbLogFp, "\n");
	ComputeIncrementalChecksum(pba);
	return 0;
}

static int QuarkReadAcb(struct BurnArea* pba)
{
	//ComputeIncrementalChecksum(pba);
	memcpy(gAcbScanPointer, pba->Data, pba->nLen);
	gAcbScanPointer += pba->nLen;
	return 0;
}
static int QuarkWriteAcb(struct BurnArea* pba)
{
	memcpy(pba->Data, gAcbScanPointer, pba->nLen);
	gAcbScanPointer += pba->nLen;
	return 0;
}

bool __cdecl ggpo_save_game_state_callback(unsigned char **buffer, int *len, int *checksum, int frame)
{
	int payloadsize;

	gAcbChecksum = 0;
	gAcbScanPointer = gAcbBuffer;
	BurnAcb = QuarkReadAcb;
	BurnAreaScan(ACB_FULLSCANL | ACB_READ, NULL);
	payloadsize = gAcbScanPointer - gAcbBuffer;

	*checksum = gAcbChecksum;
	*len = payloadsize + ggpo_state_header_size;
	*buffer = (unsigned char *)malloc(*len);

	int *data = (int *)*buffer;
	data[0] = 'GGPO';
	data[1] = ggpo_state_header_size;
	data[2] = nBurnVer;
	data[3] = 0;
	data[4] = 0;
	data[5] = 0;
	// save game state in ranked match (so spectators can get the actual score)
	if (!kNetSpectator) {
		// 32 bits for state, scores and ranked
		int state, score1, score2, start1, start2;
		DetectorGetState(state, score1, score2, start1, start2);
		data[3] = state | ((score1 & 0xff) << 8) | ((score2 & 0xff) << 16) | (iRanked << 24);
		data[4] = (start1 & 0xff) | ((start2 & 0xff) << 8);
	}
	memcpy((*buffer) + ggpo_state_header_size, gAcbBuffer, payloadsize);
	return false;
}

bool __cdecl ggpo_load_game_state_callback(unsigned char *buffer, int len)
{
	int *data = (int *)buffer;
	if (data[0] == 'GGPO') {
		int headersize = data[1];
		int num = headersize / sizeof(int);
		// version
		nAcbVersion = data[2];
		int state = (data[3]) & 0xff;
		int score1 = (data[3] >> 8) & 0xff;
		int score2 = (data[3] >> 16) & 0xff;
		int ranked = (data[3] >> 24) & 0xff;
		int start1 = 0;
		int start2 = 0;
		if (num > 4) {
			start1 = (data[4]) & 0xff;
			start2 = (data[4] >> 8) & 0xff;
		}
		// if spectating, set ranked flag and score
		if (kNetSpectator) {
			iRanked = ranked;
			DetectorSetState(state, score1, score2, start1, start2);
			VidOverlaySetGameInfo(0, 0, kNetSpectator, iRanked, 0);
			VidOverlaySetGameScores(score1, score2);
			VidSSetGameInfo(0, 0, kNetSpectator, iRanked, 0);
			VidSSetGameScores(score1, score2);
		}
		buffer += headersize;
	}
gAcbScanPointer = (char*)buffer;
BurnAcb = QuarkWriteAcb;
nAcbLoadState = kNetSpectator;
BurnAreaScan(ACB_FULLSCANL | ACB_WRITE, NULL);
nAcbLoadState = 0;
nAcbVersion = nBurnVer;
return true;
}

bool __cdecl ggpo_log_game_state_callback(char* filename, unsigned char* buffer, int len)
{
	/*
	 * Note: this is destructive since it relies on loading game
	 * state before scanning!  Luckily, we only call the logging
	 * routine for fatal errors (we should still fix this, though).
	 */
	ggpo_load_game_state_callback(buffer, len);

	gAcbLogFp = fopen(filename, "w");

	gAcbChecksum = 0;
	BurnAcb = QuarkLogAcb;
	BurnAreaScan(ACB_FULLSCANL | ACB_READ, NULL);
	fprintf(gAcbLogFp, "\n");
	fprintf(gAcbLogFp, "Checksum:       %d\n", gAcbChecksum);
	fprintf(gAcbLogFp, "Buffer Pointer: %p\n", buffer);
	fprintf(gAcbLogFp, "Buffer Len:     %d\n", len);

	fclose(gAcbLogFp);

	return true;
}

void __cdecl ggpo_free_buffer_callback(void* buffer)
{
	free(buffer);
}

// ggpo_set_frame_delay from lib
typedef INT32(_cdecl* f_ggpo_set_frame_delay)(GGPOSession*, int frames);
static f_ggpo_set_frame_delay ggpo_set_frame_delay;

static bool ggpo_init()
{
	// load missing ggpo_set_frame_delay from newer ggponet.dll, not available on ggponet.lib
	HINSTANCE hLib = LoadLibrary(_T("ggponet.dll"));
	if (!hLib) {
		return false;
	}
	ggpo_set_frame_delay = (f_ggpo_set_frame_delay)GetProcAddress(hLib, "ggpo_set_frame_delay");
	if (!ggpo_set_frame_delay) {
		return false;
	}

	FreeLibrary(hLib);
	return true;
}

void QuarkInit(TCHAR* tconnect)
{
	char connect[MAX_PATH];
	TCHARToANSI(tconnect, connect, MAX_PATH);
	char game[128], quarkid[128], server[128];
	int port = 0;
	int delay = 0;
	int ranked = 0;
	int live = 0;
	int frames = 0;
	int player = 0;
	int localPort, remotePort;

	kNetVersion = NET_VERSION;
	kNetGame = 1;
	kNetLua = 0;
	kNetSpectator = 0;
	bForce60Hz = 1;
	iRanked = 0;
	iPlayer = 0;
	iDelay = 0;

#ifdef _DEBUG
	kNetLua = 1;
#endif


	/*@moscade */
	if (strncmp(connect, "moscade://", strlen("moscade://")) == 0) {
		MOSCadeURI uri; uri.from_uri(connect);

		if (strcmp(GetGGPONetHost(), uri.host) != 0) {
			dprintf(_T("** Overwriting host:"));
			dprintf(ANSIToTCHAR(uri.host, NULL, 0));
			SpawnOverwriteProcess(ANSIToTCHAR(connect, NULL, 0));
			exit(0); // despawns our process & lets overwritter do the job
		}
		else {
			dprintf(_T("** Host match! Procceding to connect.\n"));
		}
		dprintf(_T("\n** Connecting via moscade:// protocol\n"));
		dprintf(_T("** MOSCade quark: "));
		dprintf(ANSIToTCHAR(uri.quark,NULL,0));		
		dprintf(_T("\n"));
		return QuarkInit(ANSIToTCHAR(uri.to_quark(),NULL,0));
	}
	
	ggpo_init();
	GGPOSessionCallbacks cb = { 0 };

	cb.begin_game = ggpo_begin_game_callback;
	cb.load_game_state = ggpo_load_game_state_callback;
	cb.save_game_state = ggpo_save_game_state_callback;
	cb.log_game_state = ggpo_log_game_state_callback;
	cb.free_buffer = ggpo_free_buffer_callback;
	cb.advance_frame = ggpo_advance_frame_callback;
	cb.on_event = ggpo_on_event_callback;

	if (strncmp(connect, "quark:served", strlen("quark:served")) == 0) {
		sscanf(connect, "quark:served,%[^,],%[^,],%d,%d,%d", game, quarkid, &port, &delay, &ranked);
		kNetLua = 1;
		iRanked = ranked;
		iPlayer = atoi(&quarkid[strlen(quarkid) - 1]);
		iDelay = delay;
		iSeed = GetHash(quarkid, strlen(quarkid) - 2);
		ggpo = ggpo_client_connect(&cb, game, quarkid, port);
		ggpo_set_frame_delay(ggpo, delay);
		dprintf(_T("** Quark"));
		dprintf(ANSIToTCHAR(connect,NULL,0));
		dprintf(_T("\n"));
		VidOverlaySetSystemMessage(_T("Connecting..."));
	}
	else if (strncmp(connect, "quark:direct", strlen("quark:direct")) == 0) {
		sscanf(connect, "quark:direct,%[^,],%d,%[^,],%d,%d,%d,%d", game, &localPort, server, &remotePort, &player, &delay, &ranked);
		kNetLua = 1;
		bDirect = true;
		iRanked = 0;
		iPlayer = player;
		iDelay = delay;
		iSeed = 0;
		ggpo = ggpo_start_session(&cb, game, localPort, server, remotePort, player);
		ggpo_set_frame_delay(ggpo, delay);
		VidOverlaySetSystemMessage(_T("Connecting..."));
	}
	/*
	else if (strncmp(connect, "quark:synctest", strlen("quark:synctest")) == 0) {
	  sscanf(connect, "quark:synctest,%[^,],%d", game, &frames);
	  ggpo = ggpo_start_synctest(&cb, game, frames);
	}
	*/
	else if (strncmp(connect, "quark:stream", strlen("quark:stream")) == 0) {
		sscanf(connect, "quark:stream,%[^,],%[^,],%d", game, quarkid, &remotePort);
		bVidAutoSwitchFullDisable = true;
		kNetSpectator = 1;
		kNetLua = 1;
		iSeed = 0;
		ggpo = ggpo_start_streaming(&cb, game, quarkid, remotePort);
		VidOverlaySetSystemMessage(_T("Connecting..."));
	}
	else if (strncmp(connect, "quark:replay", strlen("quark:replay")) == 0) {
		bVidAutoSwitchFullDisable = true;
		kNetSpectator = 1;
		kNetLua = 1;
		iSeed = 0;
		ggpo = ggpo_start_replay(&cb, connect + strlen("quark:replay,"));
	}
	else if (strncmp(connect, "quark:debugdetector", strlen("quark:debugdetector")) == 0) {
		sscanf(connect, "quark:debugdetector,%[^,]", game);
		kNetGame = 0;
		iRanked = 1;
		iPlayer = 0;
		iSeed = 0x133;
		kNetLua = 1;
		// load game
		TCHAR tgame[128];
		ANSIToTCHAR(game, tgame, 128);
		UINT32 i;
		for (i = 0; i < nBurnDrvCount; i++) {
			nBurnDrvActive = i;
			if ((_tcscmp(BurnDrvGetText(DRV_NAME), tgame) == 0) && (!(BurnDrvGetFlags() & BDF_BOARDROM))) {
				// Load game
				MediaInit();
				DrvInit(i, true);
				// Load our save-state file (freeplay, event mode, etc.)
				WIN32_FIND_DATA fd;
				TCHAR tfilename[MAX_PATH];
				_stprintf(tfilename, _T("savestates\\%s_fbneo.fs"), tgame);
				if (FindFirstFile(tfilename, &fd) != INVALID_HANDLE_VALUE) {
					BurnStateLoad(tfilename, 1, &DrvInitCallback);
				}
				// load game detector in editor mode
				DetectorLoad(game, true, iSeed);
				VidOverlaySetGameInfo(_T("Detector1#0,0,0"), _T("Detector2#0,0,0"), false, iRanked, iPlayer);
				VidOverlaySetGameSpectators(0);
				VidSSetGameInfo(_T("Detector1"), _T("Detector2"), false, iRanked, iPlayer);
				VidSSetGameSpectators(0);
				break;
			}
		}
	}
}

void QuarkEnd()
{
	ConfigGameSave(bSaveInputs);
	ggpo_close_session(ggpo);
	kNetGame = 0;
	bMediaExit = true;

}

void QuarkTogglePerfMon()
{
	static bool initialized = false;
	if (!initialized) {
		ggpoutil_perfmon_init(hScrnWnd);
	}
	ggpoutil_perfmon_toggle();
}

void QuarkRunIdle(int ms)
{
	ggpo_idle(ggpo, ms);
}

bool QuarkGetInput(void *values, int size, int players)
{
	return ggpo_synchronize_input(ggpo, values, size, players);
}

bool QuarkIncrementFrame()
{
	// start auto replay
	if (bReplayRecord) {
		bReplayRecord = false;
		bReplayRecording = true;
		AviStart();
	}

	ggpo_advance_frame(ggpo);

	if (!bSkipPerfmonUpdates) {
		GGPONetworkStats stats;
		ggpo_get_stats(ggpo, &stats);
		ggpoutil_perfmon_update(ggpo, stats);
	}

	if (!bReplaySupport && !bReplayStarted) {
		bReplayStarted = true;
	}

	return true;
}

void QuarkSendChatText(char *text)
{
	QuarkSendChatCmd(text, 'T');
}
/* Sends command to server. Where commands are encoded w/ base-64 first*/
void QuarkSendChatCmd(char *text, char cmd)
{
	char buffer[1024]; // command chat
	buffer[0] = cmd;
	strcpy(&buffer[1], to_base64(text));
	ggpo_client_chat(ggpo, buffer);
}

void QuarkUpdateStats(double fps)
{
	GGPONetworkStats stats;
	ggpo_get_stats(ggpo, &stats);
	VidSSetStats(fps, stats.network.ping, iDelay);
	VidOverlaySetStats(fps, stats.network.ping, iDelay);
}

void QuarkRecordReplay()
{
	bReplayRecord = true;
	bReplayRecording = false;
}

void QuarkFinishReplay()
{
	if (!bReplaySupport && bReplayStarted) {
		bReplayStarted = false;
		kNetGame = 0;
		if (bReplayRecording) {
			AviStop();
			bMediaExit = true;
		}
	}
}


/*********************************/
/* (c) HairyH 2025               */
/*********************************/
#include "main.h"

using namespace std;
//#undef DEBUG
//#define DEBUG

// Global Variables
HINSTANCE	hInst = 0;
HWND		ghMain = 0;
HWND        ghPtMain = 0;
HWND		ghEdit = 0;
HWND		ghInterval = 0;
HWND		ghLimit = 0;
HWND		ghClock = 0;
HWND		ghCbSend = 0;

// Debug related globals
#ifdef DEBUG
char gszDebugMsg[MAX_PATH] = { '\0' };
wchar_t gwcDebugMsg[MAX_PATH] = { '\0' };
#endif // DEBUG

// Paltalk Handles
HWND ghPtRoom = NULL, ghPtLv = NULL;

// Font handles
HFONT ghFntClk = NULL;

// Timer related globals
BOOL gbMonitor = FALSE;
BOOL gbSendTxt = TRUE;
BOOL gbSendLimit = FALSE;
BOOL gbSendBold = TRUE;
BOOL gbBeep = FALSE;
BOOL gbAutoDot = FALSE;
int giMicTimerSeconds = 0;
int giInterval = 30;
int giLimit = 120;
std::map<std::string, int> gNickLimits;
std::atomic<bool> gRunning(true);
std::string gLimitsFile = "limits.txt";
bool gbNickLimitsLoaded = false;

// Nick related globals
char gszSavedNick[MAX_PATH] = { '\0' };
char gszCurrentNick[MAX_PATH] = { '\0' };
char gszDotMicUser[MAX_PATH] = { '\0' };
wchar_t gwcRoomTitle[MAX_PATH] = { '\0' };
int iMaxNicks = 0;
int iDrp = 0;
// UIAutomation related globals
CComPtr<IUIAutomationElement> emojiTextEditElement;
CComPtr<IUIAutomationElement> automationElementRoom;
CComPtr<IUIAutomation> g_pUIAutomation;

// Function prototypes
BOOL CALLBACK DlgMain(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
void GetPaltalkWindows(void);
BOOL CALLBACK EnumPaltalkWindows(HWND hWnd, LPARAM lParam);
void RestoreAndBringToFront(HWND hWnd);
BOOL InitClockDis(void);
BOOL InitIntervals(void);
BOOL InitMicLimits(void);
BOOL GetMicUser(void);
void CreateContextMenu(WPARAM wParam, LPARAM lparam);
void StartStopMonitoring(void);
void MicTimerStart(void);
void MicTimerReset(void);
void MicTimerTick(void);
void MonitorTimerTick(void);
wstring ConvertToBold(const wstring& inString);
void CopyPaste2Paltalk(char* szMsg);
// UIAutomation and Dot Mic User related functions
HRESULT __stdcall InitUIAutomation(void);
HRESULT __stdcall UninitUIAutomation(void);
HRESULT __stdcall GetUIAutomationElementFromHWNDAndClassName(HWND hwnd, const wchar_t* className, IUIAutomationElement** foundElement);
HRESULT __stdcall FindWindowByTitle(const std::wstring& title, IUIAutomationElement** outElement);
static void SimulateRightClick(int x, int y);
static void SimulateLeftClick(int x, int y);
static void SendEnterTwice();
HRESULT __stdcall DotAndUnDotMicUser(char* szMicUser);
// Nicks limits related functions
std::string trim(const std::string& s);
bool LoadNickLimits(const std::string& filename);
bool SetLimitForNick(const std::string& nickname);

/// Main entry point of the app 
int APIENTRY WinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd)
{
	hInst = hInstance;
	InitCommonControls();
	LoadLibraryW(L"riched20.dll"); // comment if richedit is not used

	//OutputDebugStringA("[COM] Attempting to initialize COM...\n");
	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(hr)) {
#ifdef DEBUG
		char szMsg[128];
		sprintf_s(szMsg, sizeof(szMsg), "[COM] CoInitializeEx FAILED! HRESULT=0x%08X\n", hr);
		OutputDebugStringA(szMsg);
#endif // DEBUG
		msga("CoInitializeEx Failed!");
		return 2;
	}
	else {
#ifdef DEBUG
	OutputDebugStringA("[COM] COM initialized successfully (STA mode).\n");
#endif // DEBUG
	}

	// Run the main dialog
	INT_PTR ret = DialogBox(hInst, MAKEINTRESOURCE(IDD_DLGMAIN), NULL, (DLGPROC)DlgMain);
		
	return 0;
}

/// Callback main message loop for the Application
BOOL CALLBACK DlgMain(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		ghMain = hwndDlg;
		ghInterval = GetDlgItem(hwndDlg, IDC_COMBO_INTERVAL);
		ghClock = GetDlgItem(hwndDlg, IDC_EDIT_CLOCK);
		ghCbSend = GetDlgItem(hwndDlg, IDC_CHECK1);
		ghLimit = GetDlgItem(hwndDlg, IDC_COMBO_MCLIMIT);
		InitClockDis();
		InitIntervals();
		InitMicLimits();
		SendDlgItemMessageW(hwndDlg, IDC_CHECK1, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
		SendDlgItemMessageW(hwndDlg, IDC_CHECK_BOLD, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);

	}
	break; // return TRUE;
	case WM_CLOSE:
	{
		// Cleaning up before exit
		UninitUIAutomation();
		EndDialog(hwndDlg,IDCANCEL);
	}
	return TRUE;
	case WM_CONTEXTMENU:
	{
		CreateContextMenu(wParam, lParam);
	}
	return TRUE;
	case WM_COMMAND:
	{
		switch (LOWORD(wParam))
		{
		case IDCANCEL:
		{
			// Cleaning up before exit
			UninitUIAutomation();
			EndDialog(hwndDlg, IDCANCEL);
		}
		return TRUE;
		case IDOK:
		{
			GetPaltalkWindows();
		}
		return TRUE;
		case IDC_BUTTON_START:
		{
			StartStopMonitoring();
		}
		return TRUE;
		case IDC_CHECK1: // Send text to Paltalk
		{
			gbSendTxt = IsDlgButtonChecked(hwndDlg, IDC_CHECK1);
		}
		return TRUE;
		case IDC_CHECK2: // Send Mic time limit to Paltalk
		{
			gbSendLimit = IsDlgButtonChecked(hwndDlg, IDC_CHECK2);
		}
		return TRUE;
		case IDC_CHECK_BOLD: // Send Bold text to Paltalk
		{
			gbSendBold = IsDlgButtonChecked(hwndDlg, IDC_CHECK_BOLD);
		}
		return TRUE;
		case IDM_BEEP: // enable/disable beeps
		{
			if (gbBeep)
			{
				CheckMenuItem(GetMenu(hwndDlg), IDM_BEEP, MF_UNCHECKED);
				gbBeep = FALSE;
			}
			else
			{
				CheckMenuItem(GetMenu(hwndDlg), IDM_BEEP, MF_CHECKED);
				gbBeep = TRUE;
			}
		}
		return TRUE;
		case IDC_CHECKDOT: // Set Auto Dotting 
		{
			gbAutoDot = IsDlgButtonChecked(hwndDlg, IDC_CHECKDOT);
		}
		return TRUE;
		case IDC_COMBO_INTERVAL:
		{
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				int iSel = SendMessage(ghInterval, CB_GETCURSEL, 0, 0);
				giInterval = SendMessage(ghInterval, CB_GETITEMDATA, (WPARAM)iSel, 0);
				char szTemp[100] = { 0 };
				sprintf_s(szTemp, "giInterval = %d \n", giInterval);
				OutputDebugStringA(szTemp);
			}
		}
		return TRUE;
		case IDC_COMBO_MCLIMIT:
		{
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				int iSel = SendMessage(ghLimit, CB_GETCURSEL, 0, 0);
				giLimit = SendMessage(ghLimit, CB_GETITEMDATA, (WPARAM)iSel, 0);
				char szTemp[100] = { 0 };
				sprintf_s(szTemp, "giLimit = %d \n", giLimit);
				OutputDebugStringA(szTemp);
			}
		}
		return TRUE;
		}
	}
	case WM_TIMER:
	{
		if (wParam == IDT_MICTIMER)
		{
			MicTimerTick();
		}
		else if (wParam == IDT_MONITORTIMER)
		{
			MonitorTimerTick();
		}
	}
	return TRUE;
	default:
		return FALSE;
	}
	return FALSE;
}
/// Initialise Paltalk Control Handles
void GetPaltalkWindows(void)
{
	char szWinText[200] = { '0' };
	//wchar_t wcTitle[256] = { '0' };

	// Paltalk chat room window
	ghPtRoom = NULL; ghPtLv = NULL;

	// Getting the chat room window handle
	ghPtRoom = FindWindowW(L"DlgGroupChat Window Class", 0); // this is to find nick list
	if (GetWindowTextW(ghPtRoom, gwcRoomTitle, 254) < 1)
	{
		msga("No Paltalk Room Found!");
		return;
	}
	// Getting the main Paltalk window handle
	ghPtMain = FindWindowW(L"Qt6111QWindowOwnDCIcon", gwcRoomTitle);  // this is to send text 
	if (ghPtMain == NULL)
	{
		msga("No Paltalk Main Window Found!");
		return;
	}
	// Initialise UIAutomation
	if (FAILED(InitUIAutomation())) {
		msga("Initializing UI Automation failed!");
		return;
	 }
	// Getting the Emoji Text Edit control UIAutomation element to send text to Paltalk
	HRESULT hr = GetUIAutomationElementFromHWNDAndClassName(ghPtMain, L"ui::controls::EmojiTextEdit", &emojiTextEditElement);
	if (FAILED(hr)) {
#ifdef DEBUG
		OutputDebugStringA("GetUIAutomationElementFromHWNDAndClassName failed");
#endif // DEBUG

	}
	// Get the Chat Room windows handle
	if (ghPtRoom) 
	{   // we got it
		// Get the current thread ID and the target window's thread ID
		DWORD targetThreadId = GetWindowThreadProcessId(ghPtMain, 0);
		DWORD currentThreadId = GetCurrentThreadId();
		// Attach the input of the current thread to the target window's thread
		AttachThreadInput(currentThreadId, targetThreadId, true);
		// Bring the window to the foreground
		bool bRet = SetForegroundWindow(ghPtMain);
		// Give some time for the window to come into focus
		Sleep(500);
		// Detach input once done
		AttachThreadInput(currentThreadId, targetThreadId, false);
		// Make the Paltalk Room window the HWND_TOPMOST 
		SetWindowPos(ghPtMain, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

		GetWindowTextA(ghPtRoom, szWinText, 200);
		// Set the title text to indicate which room we are timing
		wprintf_s(gwcRoomTitle, MAX_PATH, L"Timing: %s", szWinText);
		SendDlgItemMessageW(ghMain, IDC_STATIC_TITLE, WM_SETTEXT, (WPARAM)0, (LPARAM)gwcRoomTitle);
		// Finding the chat room window controls handles
		EnumChildWindows(ghPtRoom, EnumPaltalkWindows, 0);
		// Load Nick Limits from file
		gbNickLimitsLoaded = LoadNickLimits(gLimitsFile);
		if (!gbNickLimitsLoaded)
		{
			MessageBoxA(NULL, "Could not open limits.txt", "Error", MB_ICONERROR);
			return;
		}
	}
	else
	{
		msga("No Paltalk Window Found!");
	}

}

/// Enumeration Callback to Find the Control Windows
BOOL CALLBACK EnumPaltalkWindows(HWND hWnd, LPARAM lParam)
{
	char szListViewClass[] = "SysHeader32";
	char szMsg[MAX_PATH] = { 0 };
	char szClassNameBuffer[MAX_PATH] = { 0 };

	GetClassNameA(hWnd, szClassNameBuffer, MAX_PATH);

#ifdef DEBUG
	LONG lgIdCnt;
	lgIdCnt = GetWindowLongW(hWnd, GWL_ID);
	wsprintfA(szMsg, "windows class name: %s Control ID: %d \n", szClassNameBuffer, lgIdCnt);
	OutputDebugStringA(szMsg);
#endif // DEBUG

	if (strcmp(szListViewClass, szClassNameBuffer) == 0)
	{
		ghPtLv = hWnd;
#ifdef DEBUG
		sprintf_s(szMsg, "List View window handle: %p \n", ghPtLv);
		OutputDebugStringA(szMsg);
#endif // DEBUG
		return FALSE;
	}

	return TRUE;
}

/// Initialise Intervals Combo
BOOL InitIntervals(void)
{
	wchar_t wsTemp[256] = { '\0' };
	int iIndex = 0;
	int iSec = 0;
	int iMin = 0;

	for (int i = 30; i <= 180; i = i + 30)
	{
		iMin = i / 60;
		iSec = i % 60;
		swprintf_s(wsTemp, L"%d:%02d", iMin, iSec);
		iIndex = SendMessageW(ghInterval, CB_ADDSTRING, (WPARAM)0, (LPARAM)wsTemp);
		SendMessageW(ghInterval, CB_SETITEMDATA, (WPARAM)iIndex, (LPARAM)i);
	}

	SendMessageW(ghInterval, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
	giInterval = SendMessageW(ghInterval, CB_GETITEMDATA, (WPARAM)0, (LPARAM)0);
	return TRUE;
}

/// Initialise Mic time Limit selector Combo
BOOL InitMicLimits(void)
{
	wchar_t wsTemp[256] = { '\0' };
	int iIndex = 0;
	int iSec = 0;
	int iMin = 0;

	for (int i = 60; i <= 600; i = i + 30)
	{
		iMin = i / 60;
		iSec = i % 60;
		swprintf_s(wsTemp, L"%d:%02d", iMin, iSec);
		iIndex = SendMessageW(ghLimit, CB_ADDSTRING, (WPARAM)0, (LPARAM)wsTemp);
		giLimit = SendMessageW(ghLimit, CB_SETITEMDATA, (WPARAM)iIndex, (LPARAM)i);
	}

	SendMessageW(ghLimit, CB_SETCURSEL, (WPARAM)2, (LPARAM)2);
	giLimit = SendMessageW(ghLimit, CB_GETITEMDATA, (WPARAM)2, (LPARAM)0);
	return TRUE;
}

/// Initialise the Clock Display Window
BOOL InitClockDis(void)
{
	HDC hDC;
	int nHeight = 0;

	hDC = GetDC(ghMain);
	nHeight = -MulDiv(20, GetDeviceCaps(hDC, LOGPIXELSY), 72);
	ghFntClk = CreateFont(nHeight, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, TEXT("Courier New"));
	SendMessageA(ghClock, WM_SETFONT, (WPARAM)ghFntClk, (LPARAM)TRUE);
	SendMessageA(ghClock, WM_SETTEXT, (WPARAM)0, (LPARAM)"00:00");

	return TRUE;
}
/// Starts the Mic timer
void MicTimerStart(void)
{
	SendMessageA(ghClock, WM_SETTEXT, 0, (LPARAM)"00:00");
	giMicTimerSeconds = 0;
	// Get the selected mic limit
	int iSel = SendMessage(ghLimit, CB_GETCURSEL, 0, 0);
	giLimit = SendMessage(ghLimit, CB_GETITEMDATA, (WPARAM)iSel, 0);
	if(gbNickLimitsLoaded)
		{
		// Check if there is a specific limit for the current nick
		if (SetLimitForNick(std::string(gszCurrentNick)))
		{
#ifdef DEBUG
			char szTempNickLimit[100] = { 0 };
			sprintf_s(szTempNickLimit, "Nick %s has specific mic limit of %d seconds\n", gszCurrentNick, giLimit);
			OutputDebugStringA(szTempNickLimit);
#endif // DEBUG

		}
	}
#ifdef DEBUG	
	char szTemp[100] = { 0 };
	sprintf_s(szTemp, "giLimit = %d \n", giLimit);
	OutputDebugStringA(szTemp);
#endif // DEBUG
	SetTimer(ghMain, IDT_MICTIMER, 1000, 0);
}

/// Reset the Timer
void MicTimerReset(void)
{
	KillTimer(ghMain, IDT_MICTIMER);
	giMicTimerSeconds = 0;
	SendMessageA(ghClock, WM_SETTEXT, 0, (LPARAM)"00:00");
}

/// Thi is called every 1sec. 
void MicTimerTick(void)
{
	char szClock[MAX_PATH] = { '\0' };
	char szMsg[MAX_PATH] = { '\0' };
	int iX = 60;
	int iMin;
	int iSec;

	// Reset dotted mic user
	sprintf_s(gszDotMicUser, MAX_PATH, "");
	// Advance the Timer Seconds
	giMicTimerSeconds++;

	iMin = giMicTimerSeconds / iX;
	iSec = giMicTimerSeconds % iX;
	// Refreshing the Clock display
	sprintf_s(szClock, MAX_PATH, "%02d:%02d", iMin, iSec);
	SendMessageA(ghClock, WM_SETTEXT, 0, (LPARAM)szClock);

	// Mic time limit
	if (giMicTimerSeconds == giLimit)
	{
		if (gbBeep)
		{
			//MessageBeep(MB_ICONEXCLAMATION);
			Beep(440, 1000);
		}
		// if mic time limit sending is enabled 
		if (gbSendLimit)
		{
			sprintf_s(szMsg, MAX_PATH, "%s on Mic for: %d:%02d min. TIME LIMIT!", gszCurrentNick, iMin, iSec);
			CopyPaste2Paltalk(szMsg);
		}
		else // just send the time on mic message 
		{
			sprintf_s(szMsg, MAX_PATH, "%s on Mic for: %d:%02d min.", gszCurrentNick, iMin, iSec);
			CopyPaste2Paltalk(szMsg);
		}
	}
	// if mic time = limit + 30 seconds
	else if (giMicTimerSeconds == giLimit + 30)
	{
		if (gbBeep)
		{
			//MessageBeep(MB_ICONEXCLAMATION);
			Beep(880, 1000);
		}
		if (gbSendLimit && gbAutoDot)
		{
			// If there is no current nick, nothing to do here, we return
			if (strlen(gszCurrentNick) < 2) return;
			// Sending to the room auto dot is coming 
			sprintf_s(szMsg, MAX_PATH, "%s on Mic for: %d:%02d min. TIME LIMIT AUTO DOT!", gszCurrentNick, iMin, iSec);
			CopyPaste2Paltalk(szMsg);
			// Save the current nick for dotting 
			sprintf_s(gszDotMicUser, MAX_PATH, "%s", gszCurrentNick);
			// Reset the timer
			MicTimerReset(); // Reset the mic timer
			// Reset saved nick
			sprintf_s(gszSavedNick, ""); // Reset the saved nick

			// Make sure we have Paltalk main window in focus
			RestoreAndBringToFront(ghPtMain);
			Sleep(1000); // Give some time for the window to come into focus
			// First call to dot
			HRESULT hr = DotAndUnDotMicUser(gszDotMicUser);
			if (hr == E_NOTIMPL)
			{
#ifdef DEBUG
				char szErrMsg[MAX_PATH] = { '\0' };
				sprintf_s(szErrMsg, "DotAndUnDotMicUser Dot failed USER not found: %08X\n", hr);
				OutputDebugStringA(szErrMsg);
#endif // DEBUG
				//sprintf_s(szMsg, MAX_PATH, "TIMER ADMIN ALERT: AUTO DOT FAILED! USER %s USER NOT FOUND!", gszDotMicUser);
				//CopyPaste2Paltalk(szMsg);
				sprintf_s(gszDotMicUser, MAX_PATH, "");
				return;
			}
			else if (FAILED(hr))
			{
#ifdef DEBUG
				char szErrMsg[MAX_PATH] = { '\0' };
				sprintf_s(szErrMsg, "DotAndUnDotMicUser Dot failed: %08X\n", hr);
				OutputDebugStringA(szErrMsg);
#endif // DEBUG
				sprintf_s(szMsg, MAX_PATH, "ALERT: AUTO DOT FAILED! on %s | Stopping Auto Dotting | RESTART the TIMER!", gszDotMicUser);
				CopyPaste2Paltalk(szMsg);
				sprintf_s(gszDotMicUser, MAX_PATH, "");
				// Stop the Auto Dot Mic User
				SendDlgItemMessageW(ghMain, IDC_CHECKDOT, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
				gbAutoDot = FALSE; // Disable auto dotting
				return;
			}
			Sleep(1000);
			// Second call remove the dot 
			hr = DotAndUnDotMicUser(gszDotMicUser);
			if(hr == E_NOTIMPL)
			{
				// If the DotAndUnDotMicUser returns E_NOTIMPL, it means the mic user is not found
				sprintf_s(szMsg, MAX_PATH, "ADMIN ALERT: AUTO UNDOT FAILED! User %s  may has left the room dotted!", gszDotMicUser);
				CopyPaste2Paltalk(szMsg);
			}
			else if(FAILED(hr)) 
			{ 
#ifdef DEBUG			
				char szErrMsg[MAX_PATH] = { '\0' };
				sprintf_s(szErrMsg, "DotAndUnDotMicUser Undot failed: %08X\n", hr);
				OutputDebugStringA(szErrMsg);
#endif // DEBUG
				sprintf_s(szMsg, MAX_PATH, "ALERT: AUTO UNDOT FAILED! on %s | Stopping Auto Dotting | RESTART the TIMER!", gszDotMicUser);
				CopyPaste2Paltalk(szMsg);
				// Stop the Auto Dot Mic User
				SendDlgItemMessageW(ghMain, IDC_CHECKDOT, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
				gbAutoDot = FALSE; // Disable auto dotting	
			}
			
			// Reset dotted mic user
			sprintf_s(gszDotMicUser, MAX_PATH, "");
			
			return; // No need to send text every second
		}
		else
		{
			sprintf_s(szMsg, MAX_PATH, "%s on Mic for: %d:%02d min.", gszCurrentNick, iMin, iSec);
			CopyPaste2Paltalk(szMsg);

		}

	}

	// Work out when to send text to Paltalk
	else if (giMicTimerSeconds % giInterval == 0)
	{
		sprintf_s(szMsg, MAX_PATH, "%s on Mic for: %d:%02d min.", gszCurrentNick, iMin, iSec);
		CopyPaste2Paltalk(szMsg);
	}
}
/// Every sec checks mic user
void MonitorTimerTick(void)
{
	GetMicUser();
	// Failed to get current nick
	if (strlen(gszCurrentNick) < 2 && strlen(gszSavedNick) < 2) return;
	// no change keep going
	if (strcmp(gszCurrentNick, gszSavedNick) == 0)
	{
		iDrp = 0; // Reset dropout counter
	}
	// No nick but there was one before
	else if (strlen(gszCurrentNick) < 2 && strlen(gszSavedNick) > 2)
	{
		iDrp++; //to tolerate mic dropout
//#ifdef DEBUG
//		char szTemp[MAX_PATH] = { '\0' };
//		sprintf_s(szTemp, "Mic dropout count %d \n", iDrp);
//		OutputDebugStringA(szTemp);
//#endif // DEBUG
		
		if (iDrp > 3) // 5 second dropout
		{
			MicTimerReset(); //Stop the mic timing
			sprintf_s(gszSavedNick, ""); // Reset the saved nick
//#ifdef DEBUG
//			sprintf_s(szTemp, "5 dropouts: %d Reset Mic timer \n", iDrp);
//			OutputDebugStringA(szTemp);
//#endif // DEBUG
					
			iDrp = 0;
		}
	}
	// New nick on mic -> Start Mic Timer
	else if (strcmp(gszCurrentNick, gszSavedNick) != 0)
	{
		SYSTEMTIME sytUtc;
		char szMsg[MAX_PATH] = { '\0' };

		MicTimerStart();

		GetSystemTime(&sytUtc);
		int iLimitMin = giLimit / 60;
		int iLimitSec = giLimit % 60;
		sprintf_s(szMsg, "Start: %s on a %d:%02d min. Mic Session", gszCurrentNick, iLimitMin, iLimitSec);
		CopyPaste2Paltalk(szMsg);
#ifdef DEBUG
		sprintf_s(szMsg, "Start: %s at: %02d:%02d:%02d UTC\n", gszCurrentNick, sytUtc.wHour, sytUtc.wMinute, sytUtc.wSecond);
		OutputDebugStringA(szMsg);
#endif // DEBUG
		strcpy_s(gszSavedNick, gszCurrentNick);
	}
}

/// Get the Mic user
BOOL GetMicUser(void)
{
	if (!ghPtLv) return FALSE;

	char szOut[MAX_PATH] = { '\0' };
	char szMsg[MAX_PATH] = { '\0' };
	wchar_t wsMsg[MAX_PATH] = { '\0' };
	BOOL bRet = FALSE;

	const int iSizeOfItemNameBuff = MAX_PATH * sizeof(char); //wchar_t
	LPSTR pXItemNameBuff = NULL;

	LVITEMA lviNick = { 0 };
	DWORD dwProcId;
	VOID* vpMemLvi;
	HANDLE hProc;
	int iNicks;
	int iImg = 0;

	LVITEMA lviRead = { 0 };

	GetWindowThreadProcessId(ghPtLv, &dwProcId);
	hProc = OpenProcess(
		PROCESS_VM_OPERATION |
		PROCESS_VM_READ |
		PROCESS_VM_WRITE, FALSE, dwProcId);

	if (hProc == NULL)return FALSE;

	vpMemLvi = VirtualAllocEx(hProc, NULL, sizeof(LVITEMA),
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (vpMemLvi == NULL) return FALSE;

	pXItemNameBuff = (LPSTR)VirtualAllocEx(hProc, NULL, iSizeOfItemNameBuff,
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (pXItemNameBuff == NULL) return FALSE;

	iNicks = ListView_GetItemCount(ghPtLv);
	if (iMaxNicks < iNicks)
	{
		iMaxNicks = iNicks;
		char szTemp[100] = { 0 };
		sprintf_s(szTemp, "%d", iMaxNicks);
		SendDlgItemMessageA(ghMain, IDC_EDIT2, WM_SETTEXT, (WPARAM)0, (LPARAM)szTemp);
	}
//#ifdef DEBUG
//	sprintf_s(szMsg, "Number of nicks: %d \n", iNicks);
//	OutputDebugStringA(szMsg);
//#endif // DEBUG
	for (int i = 0; i < iNicks; i++)
	{
		lviNick.mask = LVIF_IMAGE | LVIF_TEXT;
		lviNick.pszText = pXItemNameBuff;
		lviNick.cchTextMax = MAX_PATH - 1;
		lviNick.iItem = i;
		lviNick.iSubItem = 0;

		WriteProcessMemory(hProc, vpMemLvi, &lviNick, sizeof(lviNick), NULL);

		SendMessage(ghPtLv, 4171, (WPARAM)i, (LPARAM)vpMemLvi); // 4171

		ReadProcessMemory(hProc, vpMemLvi, &lviRead, sizeof(lviRead), NULL);

		iImg = lviRead.iImage;
		//sprintf_s(szMsg, "Item index: %d iImg: %d \n", i, iImg);
		//OutputDebugStringA(szMsg);

		if (iImg == 10)
		{
			SendMessage(ghPtLv, 4141, (WPARAM)i, (LPARAM)vpMemLvi);    // 4141

			ReadProcessMemory(hProc, lviRead.pszText, &gszCurrentNick, sizeof(gszCurrentNick), NULL);
			bRet = TRUE;
			break;
		}
		else
		{
			sprintf_s(gszCurrentNick, "");
			bRet = FALSE;
		}

	}
//#ifdef DEBUG
//	sprintf_s(szMsg, "Image: %d Nickname: %s \n", iImg, gszCurrentNick);
//	OutputDebugStringA(szMsg);
//#endif // DEBUG
	// Cleanup 
	if (vpMemLvi != NULL) VirtualFreeEx(hProc, vpMemLvi, 0, MEM_RELEASE);
	if (pXItemNameBuff != NULL) VirtualFreeEx(hProc, pXItemNameBuff, 0, MEM_RELEASE);
	CloseHandle(hProc);

	return bRet;
}

/// Make popup menu for beep
void CreateContextMenu(WPARAM wParam, LPARAM lparam)
{
	HMENU hMenu = CreatePopupMenu();
	POINT pt;
	GetCursorPos(&pt);
	if (gbBeep)
		InsertMenuA(hMenu, 0, MF_BYCOMMAND | MF_STRING | MF_CHECKED, IDM_BEEP, "Beep");
	else
		InsertMenuA(hMenu, 0, MF_BYCOMMAND | MF_STRING | MF_UNCHECKED, IDM_BEEP, "Beep");

	TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, ghMain, NULL);
}

/// Start Monitoring the mic que
void StartStopMonitoring(void)
{
	if (!ghPtRoom)
	{
		msga("Error: No Paltalk Room!\n [Get Pt] first!");
		return;
	}
	if (!gbMonitor)
	{
		SetTimer(ghMain, IDT_MONITORTIMER, 1000, NULL);
		gbMonitor = TRUE;
		SendDlgItemMessageW(ghMain, IDC_BUTTON_START, WM_SETTEXT, 0, (LPARAM)L"Stop Timing");
	}
	else
	{
		KillTimer(ghMain, IDT_MONITORTIMER);
		sprintf_s(gszSavedNick, "");
		sprintf_s(gszCurrentNick, "");
		MicTimerReset();
		gbMonitor = FALSE;
		SendDlgItemMessageW(ghMain, IDC_BUTTON_START, WM_SETTEXT, 0, (LPARAM)L"Start Timing");
	}
}

/// Sending Text to Paltalk 
void CopyPaste2Paltalk(char* szMsg)
{
	char szOut[MAX_PATH] = { '\0' };
	wchar_t wcOut[MAX_PATH] = { '\0' };
	HRESULT hr = S_OK;

	if (strlen(gszCurrentNick) < 2) return;
	else if (!gbSendTxt) return; // if text sending is disabled

	CComPtr<IUIAutomationLegacyIAccessiblePattern> pattern;
	hr = emojiTextEditElement->GetCurrentPatternAs(UIA_LegacyIAccessiblePatternId, IID_IUIAutomationLegacyIAccessiblePattern, (void**)&pattern);
	//GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&pattern));
	if (SUCCEEDED(hr)) {
		BSTR bstrOut = NULL;

		sprintf_s(szOut, MAX_PATH, "*** %s ***", szMsg);
		MultiByteToWideChar(CP_ACP, 0, szOut, -1, wcOut, MAX_PATH);
		if (gbSendBold) {
			wstring wstrOutBold = ConvertToBold(wcOut);
			bstrOut = SysAllocString(wstrOutBold.c_str());
		}
		else {
			bstrOut = SysAllocString(wcOut);
		}
		emojiTextEditElement->SetFocus();
		pattern->SetValue(bstrOut);
		SendMessageA(ghPtMain, WM_KEYDOWN, (WPARAM)VK_RETURN, 0);
		SendMessageA(ghPtMain, WM_KEYUP, (WPARAM)VK_RETURN, 0);
		SysFreeString(bstrOut);
	}
	else {
		std::cerr << "GetCurrentPatternAs failed: " << std::hex << hr << std::endl;
	}

}
/// Convert the string to bold
wstring ConvertToBold(const wstring& inString) {
	const int boldLowercaseOffset = 119737; // 'a' to bold 'a'
	const int boldUppercaseOffset = 119743; // 'A' to bold 'A'
	const int boldDigitOffset = 120734;      // '0' to bold '0'

	wstring result;

	for (wchar_t c : inString) {
		if (iswlower(c)) { // Lowercase letters
			DWORD codePoint = c + boldLowercaseOffset;
			if (codePoint <= 0xFFFF) {
				result += static_cast<wchar_t>(codePoint);
			}
			else {
				// Handle surrogate pairs for Unicode characters above U+FFFF
				wchar_t highSurrogate = (wchar_t)(0xD800 + (codePoint - 0x10000) / 0x400);
				wchar_t lowSurrogate = (wchar_t)(0xDC00 + (codePoint - 0x10000) % 0x400);
				result += highSurrogate;
				result += lowSurrogate;
			}
		}
		else if (iswupper(c)) { // Uppercase letters
			DWORD codePoint = c + boldUppercaseOffset;
			if (codePoint <= 0xFFFF) {
				result += static_cast<wchar_t>(codePoint);
			}
			else {
				wchar_t highSurrogate = (wchar_t)(0xD800 + (codePoint - 0x10000) / 0x400);
				wchar_t lowSurrogate = (wchar_t)(0xDC00 + (codePoint - 0x10000) % 0x400);
				result += highSurrogate;
				result += lowSurrogate;
			}
		}
		else if (iswdigit(c)) { // Digits
			DWORD codePoint = c + boldDigitOffset;
			if (codePoint <= 0xFFFF) {
				result += static_cast<wchar_t>(codePoint);
			}
			else {
				wchar_t highSurrogate = (wchar_t)(0xD800 + (codePoint - 0x10000) / 0x400);
				wchar_t lowSurrogate = (wchar_t)(0xDC00 + (codePoint - 0x10000) % 0x400);
				result += highSurrogate;
				result += lowSurrogate;
			}
		}
		else {
			result += c;
		}
	}

	return result;
}

/// Get the UIAutomation element from HWND and ClassName
HRESULT __stdcall GetUIAutomationElementFromHWNDAndClassName(HWND hwnd, const wchar_t* className, IUIAutomationElement** foundElement) {
	HRESULT hr = S_OK;

	CComPtr<IUIAutomationElement> elementRoom;
	hr = g_pUIAutomation->ElementFromHandle(hwnd, &elementRoom);
	if (FAILED(hr)) {
#ifdef DEBUG
		char szDebug[] = "ElementFromHandle failed: elementRoom\n";
		OutputDebugStringA(szDebug);
#endif // DEBUG
		return hr;
	}

	CComPtr<IUIAutomationCondition> classNameCondition;
	CComVariant classNameVariant(className);
	hr = g_pUIAutomation->CreatePropertyCondition(UIA_ClassNamePropertyId, classNameVariant, &classNameCondition);
	if (FAILED(hr)) {
#ifdef DEBUG
		char szDebug[] = "CreatePropertyCondition failed : className\n";
		OutputDebugStringA(szDebug);
#endif // DEBUG

		return hr;
	}

	hr = elementRoom->FindFirst(TreeScope_Subtree, classNameCondition, foundElement);
	if (FAILED(hr)) {
#ifdef DEBUG
		char szDebug[] = "FindFirst failed: elementRoom\n";
		OutputDebugStringA(szDebug);
#endif // DEBUG

		return hr;
	}

	return S_OK;
}
/// Find the Chat window by title
HRESULT __stdcall FindWindowByTitle(const std::wstring& title, IUIAutomationElement** outElement)
{
	HRESULT hr = S_OK;

	CComPtr<IUIAutomationElement> root = nullptr; // Desktop
	hr = g_pUIAutomation->GetRootElement(&root);
	if (FAILED(hr)) {
#ifdef DEBUG
		swprintf_s(gwcDebugMsg, MAX_PATH, L"Error getting root element");
		OutputDebugStringW(gwcDebugMsg);
#endif // DEBUG
		return hr;
	}

	CComPtr<IUIAutomationCondition> cond = nullptr;
	CComVariant NameVariant(title.c_str());
	hr = g_pUIAutomation->CreatePropertyCondition(UIA_NamePropertyId, NameVariant, &cond);
		
	hr = root->FindFirst(TreeScope_Children, cond, outElement);
	if (FAILED(hr)) {
#ifdef DEBUG
		sprintf_s(gszDebugMsg, MAX_PATH, "Failed to find the Room!");
		OutputDebugStringA(gszDebugMsg);
#endif // DEBUG
		return hr;
	}
			
	return hr; 
}
/// Initialize UI Automation
HRESULT __stdcall InitUIAutomation(void)
{
	OutputDebugStringA("[UIAutomation] Creating CUIAutomation instance...\n");
	HRESULT hr = CoCreateInstance(__uuidof(CUIAutomation), NULL,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&g_pUIAutomation));
	if (FAILED(hr)) {
		char szMsg[128];
		sprintf_s(szMsg, sizeof(szMsg), "[UIAutomation] CoCreateInstance FAILED! HRESULT=0x%08X\n", hr);
		OutputDebugStringA(szMsg);
		return hr; 
	}

	OutputDebugStringA("[UIAutomation] CUIAutomation instance created successfully.\n");
	return S_OK;
}
HRESULT __stdcall UninitUIAutomation(void)
{
	if (ghFntClk)
		DeleteObject(ghFntClk);

	if (g_pUIAutomation) {
		g_pUIAutomation.Release();
		OutputDebugStringA("[UIAutomation] CUIAutomation instance released.\n");
		return S_OK;
	}
	else {
		OutputDebugStringA("[UIAutomation] CUIAutomation instance was not initialized.\n");
	}
	// If we reach here, it means the instance was not created or already released.

	if(automationElementRoom) {
		automationElementRoom.Release();
		OutputDebugStringA("[UIAutomation] automationElementRoom released.\n");
	}
	else{
		OutputDebugStringA("[UIAutomation] automationElementRoom was not initialized.\n");
	}
	// If we reach here, it means the instance was not created or already released.

	if(emojiTextEditElement) {
		emojiTextEditElement.Release();
		OutputDebugStringA("[UIAutomation] emojiTextEditElement released.\n");
	}
	else {
		OutputDebugStringA("[UIAutomation] emojiTextEditElement was not initialized.\n");
	}
	// If we reach here, it means the instance was not created or already released.

	OutputDebugStringA("[COM] Uninitializing COM...\n");

	CoUninitialize();

	OutputDebugStringA("[COM] COM uninitialized.\n");

	return E_NOTIMPL; 
}
/// Simulate mouse right click
static void SimulateRightClick(int x, int y) {
	SetCursorPos(x, y);
	mouse_event(MOUSEEVENTF_RIGHTDOWN, x, y, 0, 0);
	mouse_event(MOUSEEVENTF_RIGHTUP, x, y, 0, 0);
}
/// Simulate mouse left click
static void SimulateLeftClick(int x, int y) {
	SetCursorPos(x, y);
	mouse_event(MOUSEEVENTF_LEFTDOWN, x, y, 0, 0);
	mouse_event(MOUSEEVENTF_LEFTUP, x, y, 0, 0);
}
/// Simulate Enter button press twice
static void SendEnterTwice() {
	INPUT input[2] = {};
	for (int i = 0; i < 2; ++i) {
		input[0].type = INPUT_KEYBOARD;
		input[0].ki.wVk = VK_RETURN;
		SendInput(1, &input[0], sizeof(INPUT));

		input[0].ki.dwFlags = KEYEVENTF_KEYUP;
		SendInput(1, &input[0], sizeof(INPUT));
		input[0].ki.dwFlags = 0;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}
/// New doting mic user function
HRESULT __stdcall DotAndUnDotMicUser(char* szMicUser)
{
	HRESULT hr = S_OK;
	POINT ptCur = { 0 }; 
	VARIANT vRect = { 0 };
	vRect.vt = VT_ARRAY | VT_R8; // The variant type is set for an array of doubles.

	// Getting the chat room element
	CComPtr<IUIAutomationElement> elementRoom = nullptr;
	std::wstring strRoomTitle(gwcRoomTitle);
	hr = FindWindowByTitle(strRoomTitle, &elementRoom);
	if (elementRoom == nullptr) {
#ifdef DEBUG
		swprintf_s(gwcDebugMsg, MAX_PATH, L"FindWindowByTitle failed: %s\n", strRoomTitle.c_str());
		OutputDebugStringW(gwcDebugMsg);
#endif // DEBUG
		return E_FAIL; // Return an error if the room element is not found
	}

	elementRoom->SetFocus();
	GetCursorPos(&ptCur);

	// Finding the mic queue title widget element
	CComPtr<IUIAutomationElement> micQueueTitleEl;
	CComPtr<IUIAutomationCondition> classCondTitleItem;
	CComVariant ClassNameVariant(L"ui::rooms::member_list::MicQueueTitleItemWidget");
	// Setting up class name condition for TitleWidget
	hr = g_pUIAutomation->CreatePropertyCondition(UIA_ClassNamePropertyId, ClassNameVariant, &classCondTitleItem);
	if (classCondTitleItem == nullptr) {
#ifdef DEBUG
		swprintf_s(gwcDebugMsg, MAX_PATH, L"TitleWidget Condition Failed!\n");
		OutputDebugStringW(gwcDebugMsg);
#endif // DEBUG
		return E_FAIL;
	}
	// Getting the micQueueTitleEl element
	hr = elementRoom->FindFirst(TreeScope_Descendants, classCondTitleItem, &micQueueTitleEl);
	if (micQueueTitleEl == nullptr) {
#ifdef DEBUG
		swprintf_s(gwcDebugMsg, MAX_PATH, L"Getting the micQueueTitleEl element  failed!\n");
		OutputDebugStringW(gwcDebugMsg);
#endif // DEBUG
		return E_FAIL;
	}
	// Finding the Base Button element by class name
	CComPtr<IUIAutomationElement> baseButtonEl;
	CComPtr<IUIAutomationCondition> condBaseButton;
	CComVariant BaseButtonVariant(L"qtctrl::BaseButton");
	// Setting up the condition for base button 
	hr = g_pUIAutomation->CreatePropertyCondition(UIA_ClassNamePropertyId, BaseButtonVariant, &condBaseButton);
	if (condBaseButton == nullptr) {
#ifdef DEBUG
		swprintf_s(gwcDebugMsg, MAX_PATH, L"Failed to create BaseButton Condition!\n");
		OutputDebugStringW(gwcDebugMsg);
#endif // DEBUG
		return E_FAIL;
	}
	// We are looking for the BaseButton element inside the micQueueTitleEl
	hr = micQueueTitleEl->FindFirst(TreeScope_Descendants, condBaseButton, &baseButtonEl);
	if (baseButtonEl == nullptr) {
#ifdef DEBUG
		swprintf_s(gwcDebugMsg, MAX_PATH, L"Failed to Find BaseButton element!\n");
		OutputDebugStringW(gwcDebugMsg);
#endif // DEBUG
		return E_FAIL;
	}
	//Now we got the BaseButton element
	
	//Start finding the user in the nick list
	baseButtonEl->GetCurrentPropertyValue(UIA_BoundingRectanglePropertyId, &vRect);
	POINT ptBaseButton = { 0 };

	SAFEARRAY* psa2 = nullptr;

	if (vRect.vt == (VT_ARRAY | VT_R8) && vRect.parray != nullptr) {
		psa2 = vRect.parray;
		double* pData = nullptr;
		SafeArrayAccessData(psa2, (void**)&pData);

		if (pData != nullptr) {
			RECT r;
			r.left = static_cast<LONG>(pData[0]);
			r.top = static_cast<LONG>(pData[1]);
			r.right = static_cast<LONG>(pData[2]);
			r.bottom = static_cast<LONG>(pData[3]);

			ptBaseButton.x = (r.left + (r.right / 2));
			ptBaseButton.y = (r.top + (r.bottom / 2));
						
#ifdef DEBUG
			sprintf_s(gszDebugMsg, MAX_PATH, "X position = %d Y position = %d\n", ptBaseButton.x, ptBaseButton.y);
			OutputDebugStringA(gszDebugMsg);
#endif // DEBUG
			
			baseButtonEl->SetFocus();

			// Activate the user list search box
			SimulateLeftClick(ptBaseButton.x, ptBaseButton.y);
			std::this_thread::sleep_for(std::chrono::milliseconds(300));
			// Send the dotted user name to the search box
			for (int i = 0; i < strlen(szMicUser); i++)
			{
				SendMessageA(ghPtMain, WM_CHAR, (WPARAM)szMicUser[i], 0);
			}
			// Wait to search 			
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
		SafeArrayUnaccessData(psa2);
	}
	// Getting the ui::rooms::member_list::MemberItemWidget
	CComPtr<IUIAutomationElement> memberItemEl;
	CComPtr<IUIAutomationCondition> condMemberItem;
	CComVariant vcondMemberItem(L"ui::rooms::member_list::MemberItemWidget");
	// Setting up the condition for memberItemEl
	hr = g_pUIAutomation->CreatePropertyCondition(UIA_ClassNamePropertyId, vcondMemberItem, &condMemberItem);
	if (condMemberItem == nullptr) {
#ifdef DEBUG
		swprintf_s(gwcDebugMsg, MAX_PATH, L"Failed to create MemberItemWidget Condition!\n");
		OutputDebugStringW(gwcRoomTitle);
#endif // DEBUG

		return E_FAIL;
	}
	// We are looking for the MemberItemWidget element inside the room
	hr = elementRoom->FindFirst(TreeScope_Descendants, condMemberItem, &memberItemEl);
	if (memberItemEl == nullptr) {
#ifdef DEBUG
		swprintf_s(gwcDebugMsg, MAX_PATH, L"Failed to create MemberItemWidget Condition!\n");
		OutputDebugStringW(gwcRoomTitle);
#endif // DEBUG
		// If we cannot find the member item, we left click on the BaseButton element
		SimulateLeftClick(ptBaseButton.x, ptBaseButton.y);
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		// Set cursor back to the original position
		SetCursorPos(ptCur.x, ptCur.y);
		return E_NOTIMPL;// E_FAIL;
	}
	
	// Getting the bounding rect of member item.
	vRect = { 0 };
	vRect.vt = VT_ARRAY | VT_R8; // The variant type is set for an array of doubles.
	memberItemEl->GetCurrentPropertyValue(UIA_BoundingRectanglePropertyId, &vRect);
	SAFEARRAY* psa3 = nullptr;
	if (vRect.vt == (VT_ARRAY | VT_R8) && vRect.parray != nullptr) {
		psa3 = vRect.parray;
		double* pData = nullptr;
		SafeArrayAccessData(psa3, (void**)&pData);

		if (pData != nullptr) {
			RECT r;
			r.left = static_cast<LONG>(pData[0]);
			r.top = static_cast<LONG>(pData[1]);
			r.right = static_cast<LONG>(pData[2]);
			r.bottom = static_cast<LONG>(pData[3]);

			int x = (r.left + (r.right / 2));
			int y = (r.top + (r.bottom / 2));
#ifdef DEBUG
			sprintf_s(gszDebugMsg, MAX_PATH, "X: %d Y: %d   memberItemEl\n", x, y);
			OutputDebugStringA(gszDebugMsg);
#endif // DEBUG
			elementRoom->SetFocus();
			
			SimulateRightClick(x, y);
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			SendEnterTwice();
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			SimulateLeftClick(ptBaseButton.x, ptBaseButton.y);
		}

		SafeArrayUnaccessData(psa3);
	}
	// Setting the cursor back where it was
	SetCursorPos(ptCur.x,ptCur.y);
	// End of DotMicPerson 
	return hr;
}

// Restore the window and bring it to the front
void RestoreAndBringToFront(HWND hWnd)
{
	if (!IsWindow(hWnd))
		return;

	// Restore if minimized
	if (IsIconic(hWnd))
		ShowWindow(hWnd, SW_RESTORE);

	// Get current foreground window
	HWND hForeground = GetForegroundWindow();
	if (hForeground == hWnd)
		return; // already in front

	// Get thread IDs
	DWORD dwFgThread = GetWindowThreadProcessId(hForeground, NULL);
	DWORD dwOurThread = GetCurrentThreadId();

	// Temporarily attach input
	AttachThreadInput(dwOurThread, dwFgThread, TRUE);

	// Bring to front
	SetForegroundWindow(hWnd);
	BringWindowToTop(hWnd);
	SetFocus(hWnd);

	// Detach input
	AttachThreadInput(dwOurThread, dwFgThread, FALSE);
}

// ------------------------------------------------------------------
// Helper: Trim whitespace
// ------------------------------------------------------------------
std::string trim(const std::string& s)
{
	size_t start = s.find_first_not_of(" \t\r\n");
	size_t end = s.find_last_not_of(" \t\r\n");
	if (start == std::string::npos) return "";
	return s.substr(start, end - start + 1);
}

// ------------------------------------------------------------------
// Load limits from file into gNickLimits
// ------------------------------------------------------------------
bool LoadNickLimits(const std::string& filename)
{
	std::ifstream file(filename);
	if (!file.is_open())
		return false;

	gNickLimits.clear();

	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty()) continue;
		std::istringstream iss(line);

		std::string nickname;
		int limit;
		if (std::getline(iss, nickname, ','))
		{
			nickname = trim(nickname);
			std::string limitStr;
			if (std::getline(iss, limitStr, ';'))
			{
				limitStr = trim(limitStr);
				try {
					limit = std::stoi(limitStr);
					gNickLimits[nickname] = limit;
				}
				catch (...) {
					// ignore malformed lines
				}
			}
		}
	}

	return true;
}

// ------------------------------------------------------------------
// Set giLimit based on nickname
// ------------------------------------------------------------------
bool SetLimitForNick(const std::string& nickname)
{
	auto it = gNickLimits.find(nickname);
	if (it != gNickLimits.end())
	{
		giLimit = it->second;
		return true;
	}
	return false;
}

// End of File
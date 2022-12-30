#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <CommCtrl.h>
#include <winternl.h>
#include "resource.h"

enum LDR_DLL_NOTIFICATION_REASON {
	LDR_DLL_NOTIFICATION_REASON_LOADED = 1,
	LDR_DLL_NOTIFICATION_REASON_UNLOADED
};

struct LDR_DLL_NOTIFICATION_DATA {
	ULONG Flags;                    //Reserved.
	PCUNICODE_STRING FullDllName;   //The full path name of the DLL module.
	PCUNICODE_STRING BaseDllName;   //The base file name of the DLL module.
	PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
	ULONG SizeOfImage;              //The size of the DLL image, in bytes.
};

typedef const LDR_DLL_NOTIFICATION_DATA * PCLDR_DLL_NOTIFICATION_DATA;

typedef VOID (CALLBACK *PLDR_DLL_NOTIFICATION_FUNCTION)(
	_In_     LDR_DLL_NOTIFICATION_REASON NotificationReason,
	_In_     PCLDR_DLL_NOTIFICATION_DATA NotificationData,
	_In_opt_ PVOID                       Context
	);

EXTERN_C_START

NTSYSAPI
ULONG
__cdecl
DbgPrint (
		  _In_z_ _Printf_format_string_ PCSTR Format,
		  ...
		  );

NTSYSAPI
void 
NTAPI 
RtlGetNtVersionNumbers(_Out_ PULONG Major, _Out_ PULONG Minor, _Out_ PULONG Build);

NTSYSAPI
NTSTATUS 
NTAPI 
LdrRegisterDllNotification(
						   _In_     ULONG                          Flags,
						   _In_     PLDR_DLL_NOTIFICATION_FUNCTION NotificationFunction,
						   _In_opt_ PVOID                          Context,
						   _Out_    PVOID                          *Cookie
						   );

NTSYSAPI
NTSTATUS 
NTAPI 
LdrUnregisterDllNotification(_In_ PVOID Cookie);

PVOID __imp_LdrRegisterDllNotification, __imp_LdrUnregisterDllNotification;

extern IMAGE_DOS_HEADER __ImageBase;

EXTERN_C_END

#pragma warning(disable : 4706)

class CDemoWnd
{
	PVOID m_cookie = 0;
	HWND m_hwnd = 0;
	BOOL m_bInMenuLoop = FALSE;
	ULONG m_dwThreadId = GetCurrentThreadId();

	static VOID CALLBACK OnDllNotify(
		_In_     LDR_DLL_NOTIFICATION_REASON NotificationReason,
		_In_     [[maybe_unused]] PCLDR_DLL_NOTIFICATION_DATA NotificationData,
		_In_opt_ PVOID                       Context
		)
	{
		if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED &&
			reinterpret_cast<CDemoWnd*>(Context)->m_dwThreadId == GetCurrentThreadId() &&
			reinterpret_cast<CDemoWnd*>(Context)->m_bInMenuLoop)
		{
			reinterpret_cast<CDemoWnd*>(Context)->m_dwThreadId = 0;
			ULONG n = 60;
			wchar_t txt[0x80];
			HWND hwnd = reinterpret_cast<CDemoWnd*>(Context)->m_hwnd;
			do 
			{
				if (0 < swprintf_s(txt, _countof(txt), L"Check ALT+TAB and task switch. wait %u seconds...", n))
				{
					SetWindowTextW(hwnd, txt);
					Sleep(1000);
				}
			} while (--n);

			SetWindowTextW(hwnd, L"Done");
		}
	}

	BOOL OnNcCreate(HWND hwnd, [[maybe_unused]] CREATESTRUCT* lpcs)
	{
		m_hwnd = hwnd;
		return 0 <= LdrRegisterDllNotification(0, OnDllNotify, this, &m_cookie);
	}

	LRESULT WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_NCDESTROY:
			if (PVOID cookie = m_cookie)
			{
				LdrUnregisterDllNotification(m_cookie);
			}
			PostQuitMessage(0);
			break;

		case WM_ENTERMENULOOP:
			m_bInMenuLoop = TRUE;
			break;

		case WM_EXITMENULOOP:
			m_bInMenuLoop = FALSE;
			break;

		case WM_COMMAND:
			switch (wParam)
			{
			case ID_PRESSME_EXIT:
				DestroyWindow(hwnd);
				break;
			}
			break;
		}

		return DefWindowProcW(hwnd, uMsg, wParam, lParam);
	}

	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		return reinterpret_cast<CDemoWnd*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA))->WndProc(hwnd, uMsg, wParam, lParam);
	}

public:

	static LRESULT CALLBACK StartWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_NCCREATE:
			if (CDemoWnd* This = reinterpret_cast<CDemoWnd*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams))
			{
				SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)This);
				SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)StaticWndProc);
				if (This->OnNcCreate(hwnd, reinterpret_cast<CREATESTRUCT*>(lParam)))
				{
					break;
				}
			}
			return FALSE;
		}

		return DefWindowProcW(hwnd, uMsg, wParam, lParam);
	}
};

#define Class_Name L"1BE8C048C24A488aB51D6C674E58F5B4"

void CALLBACK ep(PVOID )
{
	if (HMODULE hmod = GetModuleHandleW(L"ntdll.dll"))
	{
		if ((__imp_LdrRegisterDllNotification = GetProcAddress(hmod, "LdrRegisterDllNotification")) && 
			(__imp_LdrUnregisterDllNotification = GetProcAddress(hmod, "LdrUnregisterDllNotification")))
		{
			WNDCLASSW cls = {
				0, CDemoWnd::StartWndProc, 0, 0, (HINSTANCE)&__ImageBase, 
				0, LoadCursor(0, IDC_NO),
				(HBRUSH)(1 + COLOR_INFOBK),
				MAKEINTRESOURCEW(IDR_MENU1), 
				Class_Name
			};

			LoadIconWithScaleDown(0, IDI_WARNING, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), &cls.hIcon);

			if (RegisterClassW(&cls))
			{
				CDemoWnd wnd;

				ULONG M, m, b;
				RtlGetNtVersionNumbers(&M, &m, &b);
				wchar_t txt[0x80];
				swprintf_s(txt, _countof(txt), L"[%u.%u.%u] Press Menu !!", M, m, b & ~0xF0000000);

				if (HWND hwnd = CreateWindowExW(0, Class_Name, txt, 
					WS_OVERLAPPEDWINDOW|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
					0, 0, (HINSTANCE)&__ImageBase, &wnd))
				{
					MSG msg;
					while (GetMessageW(&msg, 0, 0, 0))
					{
						TranslateMessage(&msg);
						DispatchMessageW(&msg);
					}
				}
				UnregisterClassW(Class_Name, (HINSTANCE)&__ImageBase);
			}
		}
	}
	ExitProcess(0);
}
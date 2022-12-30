#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>

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

EXTERN_C_END

VOID CALLBACK OnDllNotify(
						  _In_     LDR_DLL_NOTIFICATION_REASON NotificationReason,
						  _In_     PCLDR_DLL_NOTIFICATION_DATA NotificationData,
						  _In_opt_ PVOID                       Context
						  )
{
	if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED)
	{
		MessageBoxW((HWND)Context, NotificationData->BaseDllName->Buffer, 0, MB_OK|MB_ICONINFORMATION);
	}
}

#pragma warning(disable : 4706)

void CALLBACK ep(PVOID cookie)
{
	if (HMODULE hmod = GetModuleHandleW(L"ntdll.dll"))
	{
		if ((__imp_LdrRegisterDllNotification = GetProcAddress(hmod, "LdrRegisterDllNotification")) && 
			(__imp_LdrUnregisterDllNotification = GetProcAddress(hmod, "LdrUnregisterDllNotification")))
		{
			if (HMENU hmenu = CreatePopupMenu())
			{
				if (InsertMenu(hmenu, 0, MF_BYPOSITION, 1, L"yyy"))
				{
					if (HWND hwnd = CreateWindowExW(0, L"Edit", 0, WS_OVERLAPPEDWINDOW|WS_VISIBLE, 
						CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, 0, 0))
					{
						RECT rc;
						if (GetWindowRect(hwnd, &rc))
						{
							if (0 <= LdrRegisterDllNotification(0, OnDllNotify, hwnd, &cookie))
							{
								TrackPopupMenu(hmenu, TPM_LEFTALIGN|TPM_TOPALIGN, rc.left, rc.top, 0, hwnd, 0);
								LdrUnregisterDllNotification(cookie);
							}
						}
						DestroyWindow(hwnd);
					}
				}
				DestroyMenu(hmenu);
			}
		}
	}
	ExitProcess(0);
}
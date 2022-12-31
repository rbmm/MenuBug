#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <CommCtrl.h>
#include <winternl.h>
#include <atlthunk.h>

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

struct YYY 
{
	HWND m_hwnd;
	HANDLE m_hEvent;
	ULONG m_dwThreadId = GetCurrentThreadId();

	YYY(HWND hwnd, HANDLE hEvent) : m_hEvent(hEvent), m_hwnd(hwnd)
	{
	}
};

VOID CALLBACK OnDllNotify(
						  _In_     LDR_DLL_NOTIFICATION_REASON NotificationReason,
						  _In_     [[maybe_unused]] PCLDR_DLL_NOTIFICATION_DATA NotificationData,
						  _In_opt_ PVOID                       Context
						  )
{
	if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED &&
		reinterpret_cast<YYY*>(Context)->m_dwThreadId == GetCurrentThreadId())
	{
		reinterpret_cast<YYY*>(Context)->m_dwThreadId = 0;

		ULONG Length = NotificationData->BaseDllName->Length + sizeof(WCHAR);
		if (PWSTR psz = (PWSTR)LocalAlloc(LMEM_FIXED, Length))
		{
			if (0 >= swprintf_s(psz, Length / sizeof(WCHAR), L"%wZ", NotificationData->BaseDllName) ||
				!PostMessageW(reinterpret_cast<YYY*>(Context)->m_hwnd, WM_NULL, (WPARAM)OnDllNotify, (LPARAM)psz))
			{
				LocalFree(psz);
			}
		}

		WaitForSingleObject(reinterpret_cast<YYY*>(Context)->m_hEvent, INFINITE);
	}
}

class YSubClass
{
	HANDLE _hEvent = 0;
	AtlThunkData_t* _Thunk = 0;
	WNDPROC _prevWndProc = 0;
	HWND _hWnd = 0;
	LONG _dwRefCount = 1;
	BOOL _bStarted = FALSE;

	static ULONG CALLBACK SecondThread(YSubClass* This)
	{
		POINT pt{};
		HWND hwndBox = This->_hWnd;
		if (ClientToScreen(hwndBox, &pt))
		{
			if (HMENU hmenu = CreatePopupMenu())
			{
				if (InsertMenu(hmenu, 0, MF_BYPOSITION, 1, L"yyy"))
				{
					if (HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, WC_EDITW, 0, WS_VISIBLE|WS_POPUP, pt.x, pt.y, 0, 0, 0, 0, 0, 0))
					{
						PVOID cookie;
						YYY y(hwndBox, This->_hEvent);
						if (0 <= LdrRegisterDllNotification(0, OnDllNotify, &y, &cookie))
						{
							TrackPopupMenu(hmenu, TPM_LEFTALIGN|TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, 0);
							LdrUnregisterDllNotification(cookie);
						}
						DestroyWindow(hwnd);
					}
				}
				DestroyMenu(hmenu);
			}
		}
		This->Release();
		return 0;
	}

	LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_NCDESTROY:
			SetEvent(_hEvent);
			Unsubclass(hWnd);
			break;

		case WM_CREATE:
			SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
			break;

		case WM_WINDOWPOSCHANGED:
			if (SWP_SHOWWINDOW & reinterpret_cast<WINDOWPOS*>(lParam)->flags)
			{
				if (!_bStarted)
				{
					_bStarted = TRUE;
					Start();
				}
			}
			break;

		case WM_NULL:
			if (wParam == (WPARAM)OnDllNotify)
			{
				SetDlgItemTextW(hWnd, MAXWORD, (PWSTR)lParam);
				LocalFree((PVOID)lParam);
				return 0;
			}
			break;
		}

		return CallWindowProc(_prevWndProc, hWnd, uMsg, wParam, lParam);
	}

	static LRESULT StaticWindowProc(YSubClass* This, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		This->AddRef();

		lParam = This->WindowProc(This->_hWnd, uMsg, wParam, lParam);

		This->Release();

		return lParam;
	}

	~YSubClass()
	{
		if (AtlThunkData_t* Thunk = _Thunk) AtlThunk_FreeData(Thunk);
		if (HANDLE hEvent = _hEvent) CloseHandle(hEvent);
	}

public:

	ULONG Start()
	{
		AddRef();
		if (HANDLE hThread = CreateThread(0, 0, (PTHREAD_START_ROUTINE)SecondThread, this, 0, 0))
		{
			CloseHandle(hThread);
			return NOERROR;
		}
		Release();
		return GetLastError();
	}

	void AddRef()
	{
		InterlockedIncrementNoFence(&_dwRefCount);
	}

	void Release()
	{
		if (!InterlockedDecrement(&_dwRefCount))
		{
			delete this;
		}
	}

	BOOL Subclass(HWND hWnd)
	{
		if (AtlThunkData_t* Thunk = AtlThunk_AllocateData())
		{
			AddRef();
			_Thunk = Thunk, _hWnd = hWnd;
			AtlThunk_InitData(Thunk, StaticWindowProc, (size_t)this);
			_prevWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)AtlThunk_DataToCode(Thunk));
			return TRUE;
		}

		return FALSE;
	}

	void Unsubclass(HWND hWnd)
	{
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)_prevWndProc);
		_hWnd = 0;
		Release();
	}

	ULONG Init()
	{
		if (HANDLE hEvent = CreateEventW(0, 0, 0, 0))
		{
			_hEvent = hEvent;
			return NOERROR;
		}

		return GetLastError();
	}

	void operator delete(void* pv)
	{
		LocalFree(pv);
	}

	void* operator new(size_t uBytes)
	{
		return LocalAlloc(LMEM_FIXED, uBytes);
	}
};

LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HCBT_CREATEWND)
	{
		if (reinterpret_cast<CBT_CREATEWND*>(lParam)->lpcs->lpszClass == WC_DIALOG)
		{
			if (YSubClass* p = new YSubClass)
			{
				if (NOERROR == p->Init())
				{
					p->Subclass((HWND)wParam);
				}
				p->Release();
			}
		}
	}

	return CallNextHookEx(0, nCode, wParam, lParam);
}

void CALLBACK ep(PVOID )
{
	if (HMODULE hmod = GetModuleHandleW(L"ntdll.dll"))
	{
		if ((__imp_LdrRegisterDllNotification = GetProcAddress(hmod, "LdrRegisterDllNotification")) && 
			(__imp_LdrUnregisterDllNotification = GetProcAddress(hmod, "LdrUnregisterDllNotification")))
		{
			if (HHOOK hhk = SetWindowsHookExW(WH_CBT, CBTProc, 0, GetCurrentThreadId()))
			{
				ULONG M, m, b;
				RtlGetNtVersionNumbers(&M, &m, &b);
				wchar_t txt[0x20];
				swprintf_s(txt, _countof(txt), L"[%u.%u.%u]", M, m, b & ~0xF0000000);
				MessageBoxW(0, L"Demo......................", txt, MB_ICONINFORMATION);
				UnhookWindowsHookEx(hhk);
			}
		}
	}
	ExitProcess(0);
}
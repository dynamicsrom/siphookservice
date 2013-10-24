/*
  SipHookService project - SIP Button fix for EzInput
  This code is provided "AS IS" without any warranties.
  (C) UltraShot
*/
#include "stdafx.h"
#include "Regext.h"
#include "Service.h"
#include "SipHookService.h"

LRESULT CALLBACK SipWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_COMMAND:
		if (LOWORD(wParam) == COMMAND_SIPBUTTON)
		{
			DWORD dwEnabled=0;
			RegistryGetDWORD(EZINPUT_REG_BRANCH, 
							EZINPUT_REG_KEY,
							EZINPUT_BIOTOSHOWMENU,
							&dwEnabled);
			if (dwEnabled)
				return FALSE;
		}
		break;
	case WM_NOTIFY:
		{
			LPNMHDR pnmh = (LPNMHDR) lParam;

			if (pnmh->code == TBN_DROPDOWN || pnmh->code == TBN_DROPDOWN_653)
			{
				DWORD dwEnabled = 0;
				RegistryGetDWORD(SIPHOOK_REG_BRANCH,
								SIPHOOK_REG_KEY,
								SIPHOOK_REG_ENABLED,
								&dwEnabled);
				if (dwEnabled)
				{
					if (GetServiceHandle(EZINPUT_SVC_PREFIX, NULL, NULL) != INVALID_HANDLE_VALUE)
					{
						SIPINFO sipInfo;
						sipInfo.cbSize = sizeof(SIPINFO);
						if (SipGetInfo(&sipInfo) == TRUE)
						{
							int flags = sipInfo.fdwFlags;
							if (flags & SIPF_ON)
							{
								RegistrySetDWORD(EZINPUT_REG_BRANCH, 
												EZINPUT_REG_KEY,
												EZINPUT_BIOTOSHOWMENU,
												1);
							}
						}
						return FALSE;		
					}
				}
			}
		}
		break;
	}
	LONG oldWndProc = (LONG)GetProp(hWnd, SIPHOOK_WINDOW_PROPERTY);
	if (oldWndProc)
		return CallWindowProc((WNDPROC)oldWndProc, hWnd, uMsg, wParam, lParam);
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

DWORD SHK_Close(DWORD dwData)
{
    return 0;
}

DWORD SHK_Deinit(DWORD dwData)
{
	HWND sipButtonWindow = FindWindow(MS_SIPBUTTON_CLASS, NULL);
	if (sipButtonWindow)
	{
		LONG oldWndProc = (LONG) RemoveProp(sipButtonWindow, SIPHOOK_WINDOW_PROPERTY);
		if (oldWndProc != NULL)
		{
			SetWindowLong(sipButtonWindow, GWL_WNDPROC, oldWndProc);
		}
	}
    return 1;
}

ULONG SipHookThreadProc( LPVOID pParam )
{
	static ApiState state = APISTATE_UNKNOWN;
	HANDLE hEvent;
	if (state == APISTATE_UNKNOWN)
	{
		hEvent = OpenEvent(EVENT_ALL_ACCESS, 0, EVENT_NAME);
		if (hEvent)
		{
			WaitForSingleObject(hEvent, INFINITE);
			CloseHandle(hEvent);
			state = APISTATE_READY;
		}
		else
		{
			state = APISTATE_NEVER;
		}
	}
	if (state == APISTATE_READY)
	{
		HWND sipButtonWindow = NULL;
		while (sipButtonWindow == NULL)
		{
			sipButtonWindow = FindWindow(L"MS_SIPBUTTON", NULL);
		}
		if (sipButtonWindow)
		{
			LONG oldWindowProc = GetWindowLong(sipButtonWindow, GWL_WNDPROC);

			SetProp(sipButtonWindow, SIPHOOK_WINDOW_PROPERTY, (HANDLE)oldWindowProc);
			SetWindowLong(sipButtonWindow, GWL_WNDPROC, (LONG)SipWndProc);
		}
	}
	MSG msg;
	while (GetMessage(&msg, 0, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}


DWORD SHK_Init(DWORD dwData)
{
	CloseHandle(CreateThread( 0, 0, SipHookThreadProc, 0, 0, 0));
    return 1;
}

DWORD SHK_IOControl(DWORD AData, DWORD ACode, void *ABufIn, 
				   DWORD ASzIn, void *ABufOut, DWORD ASzOut, 
				   DWORD *ARealSzOut) 
{
   	switch (ACode) 
	{
	case IOCTL_SERVICE_START:
		return TRUE;
	case IOCTL_SERVICE_STOP:
		return TRUE;
	case IOCTL_SERVICE_STARTED:
		return TRUE;
	case IOCTL_SERVICE_INSTALL: 
		{
			HKEY hKey;
			DWORD dwValue;

			if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Services\\SipHookSrv", 0, 
								NULL, 0, 0, NULL, &hKey, &dwValue)) 
				return FALSE;
			WCHAR dllname[] = L"SipHookService.dll";
			RegSetValueExW(hKey, L"Dll", 0, REG_SZ, 
				(const BYTE *)dllname, wcslen(dllname) << 1);

			RegSetValueExW(hKey, L"Prefix", 0, REG_SZ, (const BYTE *)L"SHK",6);

			dwValue = 0;
			RegSetValueExW(hKey, L"Flags", 0, REG_DWORD, (const BYTE *) &dwValue, 4);
			RegSetValueExW(hKey, L"Index", 0, REG_DWORD, (const BYTE *) &dwValue, 4);
			RegSetValueExW(hKey, L"Context", 0, REG_DWORD, (const BYTE *) &dwValue, 4);
			dwValue = 1;
			RegSetValueExW(hKey, L"Keep", 0, REG_DWORD, (const BYTE *) &dwValue, 4);

			dwValue = 99;
			RegSetValueExW(hKey, L"Order", 0, REG_DWORD, (const BYTE *) &dwValue, 4);

			RegCloseKey(hKey);
			return TRUE;
		}
	case IOCTL_SERVICE_UNINSTALL: 
		{
			HKEY rk;
			if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Services", 0, NULL, &rk)) 
				return FALSE;

			RegDeleteKeyW(rk, L"SipHookSrv");
			RegCloseKey(rk);

			return TRUE;
		}

	case IOCTL_SERVICE_QUERY_CAN_DEINIT:
		{
			memset(ABufOut, 1, ASzOut);
			return TRUE;
		}
	case IOCTL_SERVICE_CONTROL:
		{
			return TRUE;
		}
	default:
		return FALSE;
	}
	return TRUE;
}

DWORD SHK_Open(DWORD dwData,
			DWORD dwAccess,
			DWORD dwShareMode)
{
   return 0;
}

DWORD SHK_Read(
  DWORD dwData,
  LPVOID pBuf,
  DWORD dwLen)
{
   
   return 0;
}

DWORD SHK_Seek(
  DWORD dwData,
  long pos,
  DWORD type)
{
   
   return 0;
}

DWORD SHK_Write(
  DWORD dwData,
  LPCVOID pInBuf,
  DWORD dwInLen)
{
   
   return 0;
}

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    return TRUE;
}

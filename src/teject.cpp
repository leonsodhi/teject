// teject.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <stdlib.h>

#define MAX_DEV_NAME 256
TCHAR devName[MAX_DEV_NAME+1];

#define MAX_WIN_ERR_TXT 256
TCHAR windowsErrTxt[MAX_WIN_ERR_TXT];


bool getWindowsErrTxt(DWORD winErrorCode, TCHAR *winErrTxt, DWORD errTxtLen)
{
	bool success = true;

	HLOCAL hlocal = NULL;	
	DWORD systemLocale = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);

	// Get the error code's textual description
	BOOL gotMsg = FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL, 
		winErrorCode, 
		systemLocale,
		(PTSTR) &hlocal,
		0, 
		NULL);

	if (!gotMsg) 
	{
		// Is it a network-related error?
		HMODULE hDll = LoadLibraryEx(_T("netmsg.dll"), NULL,
			DONT_RESOLVE_DLL_REFERENCES);

		if (hDll != NULL) 
		{
			gotMsg = FormatMessage(
				FORMAT_MESSAGE_FROM_HMODULE |
				FORMAT_MESSAGE_ALLOCATE_BUFFER,
				hDll,
				winErrorCode, 
				systemLocale,
				(PTSTR) &hlocal,
				0, 
				NULL);
			FreeLibrary(hDll);
		}
	}

	
	if( gotMsg && hlocal )
	{				
		PCTSTR msg = (PCTSTR) LocalLock(hlocal);
		if(msg) 
		{ 
			_tcsncpy(winErrTxt, msg, errTxtLen);			
		}
		else
		{ success = false; }
	}
	else { success = false; }

	if(hlocal) { LocalFree(hlocal); }		

	return success;
}


void reportError(bool getLastErrorStr, const TCHAR *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	_vtprintf(fmt,ap);
	va_end(ap);	

	if(getLastErrorStr)
	{	
		bool gotErrTxt = getWindowsErrTxt(GetLastError(), windowsErrTxt, MAX_WIN_ERR_TXT);
		if(gotErrTxt)
		{ _tprintf(_T("Error code %d = %s\n"), GetLastError(), windowsErrTxt); }
	}
}


void die(bool getLastErrorStr, const TCHAR *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	_vtprintf(fmt,ap);
	va_end(ap);	

	if(getLastErrorStr)
	{	
		bool gotErrTxt = getWindowsErrTxt(GetLastError(), windowsErrTxt, MAX_WIN_ERR_TXT);
		if(gotErrTxt)
		{ _tprintf(_T("Error code %d = %s\n"), GetLastError(), windowsErrTxt); }
	}
	
	_exit(1);
}


void exitSuccess()
{
	printf("Tape ejected successfully!\n");
	_exit(0);
}


void showUsage()
{

	printf("teject 0.1 by Leon Sodhi (compiled: %s @ %s)\n", __DATE__, __TIME__ );
	printf("Usage:\n");
	printf("teject.exe DEVICENAME [-m NUM]\n");
	printf("  -m Specifies which ejection method to use. Valid values are 1, 2 or 3.\n");
	printf("  1 = lock, unmount, check, eject, unlock\n");
	printf("  2 = Legacy eject\n");
	printf("  3 = Unloads the tape (may only rewind it)\n");
	printf("  NOTE: If no method is specified each is attempted in sequence\n");
	printf("\n");
	printf("Examples:\n");
	printf("teject.exe TAPE0\n");	
	printf("teject.exe TAPE0 -m 2");
	printf("\n\n");
	printf("To find your tape device name see:\nhttp://www.codeproject.com/KB/system/Tape_drive_operator.aspx?display=Print\n");
	
}


bool ejectMethodOne(HANDLE hTape)
{
	_tprintf(_T("Ejecting tape method 1\n"));
	DWORD notUsed;
	BOOL success;

	_tprintf(_T("Locking volume\n"));
	success = DeviceIoControl(hTape, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &notUsed, NULL);
	if(!success)
	{ 
		reportError(true, _T("Could not lock tape drive %s, error code: %d. Is another application using it?\n"), devName, GetLastError()); 
		return false;
	}

	
	_tprintf(_T("Unmounting volume\n"));
	success = DeviceIoControl(hTape, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &notUsed, NULL);
	if(!success)
	{ 
		reportError(true, _T("Could not unmount tape drive %s, error code: %d\n"), devName, GetLastError()); 
		goto CleanUp;
	}


	_tprintf(_T("Ensuring media can be removed\n"));
	PREVENT_MEDIA_REMOVAL PMRBuffer;
	ZeroMemory(&PMRBuffer, sizeof(PMRBuffer));
	PMRBuffer.PreventMediaRemoval = FALSE;
	success = DeviceIoControl(hTape, IOCTL_STORAGE_MEDIA_REMOVAL, &PMRBuffer, sizeof(PREVENT_MEDIA_REMOVAL), NULL, 0, &notUsed, NULL);
	if(!success)
	{ 
		reportError(true, _T("Media does not appear to be removable, error code: %d\n"), devName, GetLastError()); 
		goto CleanUp;
	}


	_tprintf(_T("Performing eject\n"));
	success = DeviceIoControl(hTape,  IOCTL_STORAGE_EJECT_MEDIA, NULL, 0, NULL, 0, &notUsed, NULL);
	if(!success)
	{ 
		reportError(true, _T("Could not eject tape drive %s, error code: %d\n"), devName, GetLastError()); 
		goto CleanUp;
	}


CleanUp:
	_tprintf(_T("Unlocking volume\n"));
	success = DeviceIoControl(hTape, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &notUsed, NULL);
	if(!success)
		{ reportError(true, _T("Could not unlock tape drive %s, error code: %d\n"), devName, GetLastError()); }

	return (TRUE == success);
}


bool ejectMethodTwo(HANDLE hTape)
{
	_tprintf(_T("Ejecting tape method 2\n"));	
	DWORD notUsed;
	BOOL success = DeviceIoControl(hTape, IOCTL_DISK_EJECT_MEDIA, NULL, 0, NULL, 0, &notUsed, NULL);
	if(!success)
		{ reportError(true, _T("Could not eject tape %s, error code: %d\n"), devName, GetLastError()); }

	return (TRUE == success);
}


const TCHAR *parseTapeErr(DWORD errCode)
{
	static struct {
		DWORD dw; const TCHAR *e;
	} errors[] = {
		{ERROR_BEGINNING_OF_MEDIA,		_T("An attempt to access data before the beginning-of-medium marker failed.")},
		{ERROR_BUS_RESET,				_T("A reset condition was detected on the bus.")},
		{ERROR_DEVICE_NOT_PARTITIONED,	_T("The partition information could not be found when a tape was being loaded.")},
		{ERROR_END_OF_MEDIA,			_T("The end-of-tape marker was reached during an operation.")},
		{ERROR_FILEMARK_DETECTED,		_T("A filemark was reached during an operation.")},
		{ERROR_INVALID_BLOCK_LENGTH,	_T("The block size is incorrect on a new tape in a multivolume partition.")},
		{ERROR_MEDIA_CHANGED,			_T("The tape that was in the drive has been replaced or removed.")},
		{ERROR_NO_DATA_DETECTED,		_T("The end-of-data marker was reached during an operation.")},
		{ERROR_NO_MEDIA_IN_DRIVE,		_T("There is no media in the drive.")},
		{ERROR_NOT_SUPPORTED,			_T("The tape driver does not support a requested function.")},
		{ERROR_PARTITION_FAILURE,		_T("The tape could not be partitioned.")},
		{ERROR_SETMARK_DETECTED,		_T("A setmark was reached during an operation.")},
		{ERROR_UNABLE_TO_LOCK_MEDIA,	_T("An attempt to lock the ejection mechanism failed.")},
		{ERROR_UNABLE_TO_UNLOAD_MEDIA,	_T("An attempt to unload the tape failed.")},
		{ERROR_WRITE_PROTECT,			_T("The media is write protected.")}	
	};

	const int szError = sizeof(errors)/sizeof(errors[0]);
	for(int i = 0; i < szError; i++) 
	{
		if(errCode == errors[i].dw)
		{
			return errors[i].e;
		}
	}

	return NULL;
}


bool ejectMethodThree(HANDLE hTape)
{
	_tprintf(_T("Ejecting tape method 3. NOTE: This may also rewind the tape\n"));

	DWORD ret = PrepareTape(hTape, TAPE_UNLOAD, FALSE);
	if(ret != NO_ERROR)
	{
		const TCHAR *errStr = parseTapeErr(ret);
		if(errStr)
			{ reportError(false, _T("Could not eject tape %s, error text: %s\n"), devName, errStr); }
		else
			{ reportError(false, _T("Could not eject tape %s, error code: %d\n"), devName, ret); }		
	}

	return (NO_ERROR == ret);
}


int _tmain(int argc, _TCHAR* argv[])
{
	if(1 == argc)
	{
		showUsage();
		return 0;
	}

	
	int methodNum = 0;

	for(int i = 1; i < argc; i++) 
	{
		if(argv[i][0] == '-')
		{			
			 if(_tcscmp(argv[i], _T("-m")) == 0)
			 {
				 if(i < argc - 1)
				 {
					 TCHAR *endptr;				 
					 methodNum = _tcstol(argv[i+1], &endptr, 0);
					 if(endptr && *endptr)
						{ methodNum = 0; }
				 }

				 if(methodNum < 1 || methodNum > 3)
					{ die(false, _T("Invalid method number specified must be either 1, 2 or 3\n")); }
			 }
		}
	}
	
	swprintf(devName, MAX_DEV_NAME, _T("\\\\.\\%s"), argv[1]);
	
	_tprintf(_T("Opening tape %s\n"), devName);
	HANDLE hTape  = CreateFile(
		devName,
		GENERIC_READ|GENERIC_WRITE,
		FILE_SHARE_READ|FILE_SHARE_WRITE,
		0,
		OPEN_EXISTING,
		0,
		NULL
		);
	if(INVALID_HANDLE_VALUE == hTape)
		{ die(true, _T("Could not open tape device %s, error code: %d\n"), devName, GetLastError()); }
	

	bool success;

	if(methodNum != 0)
	{
		switch(methodNum)
		{
			case 1:
			{
				success = ejectMethodOne(hTape);
				if(success) { exitSuccess(); }
			}
			break;

			case 2:
			{
				success = ejectMethodTwo(hTape);
				if(success) { exitSuccess(); }
			}
			break;

			case 3:
			{
				success = ejectMethodThree(hTape);
				if(success) { exitSuccess(); }
			}

			default:
				break;
		}
	}
	else
	{
		success = ejectMethodOne(hTape);
		if(success) { exitSuccess(); }

		success = ejectMethodTwo(hTape);
		if(success) { exitSuccess(); }

		success = ejectMethodThree(hTape);
		if(success) { exitSuccess(); }
	}


	return 1;
}


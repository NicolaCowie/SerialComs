/******************************************************************************
* SEND.C	Transmit a file over a serial port that uses
*			RTS/CTS	hand-shaking through a 5-way crossed cable.
*
* Build (Developer Command Prompt	for	VS 2022, x64):
*	cl /EHsc /MD /Zi /W4 serial_send_rts.c
*
* Usage:
*	SEND <PathToFile> <BaudRate> [COMx]
*
* Example:
*	send "C:\data\image.bin" 4800 COM2
*
* The program opens the requested COM port, enables RTS/CTS flow control,
* streams the file to the port in 64 KiB blocks and prints progress.
*
* The code is pure Win32, compiles with VS2022 and runs on Windows 11 x64.
******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/******************************************************************************
* Helper – print the text of GetLastError() *
********************************************/
static void PrintLastError(const char* msg)
{
	DWORD err = GetLastError();
	LPVOID buf;
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, err, 0, (LPSTR)&buf, 0, NULL);
	fprintf(stderr, "%s: %s (0x%08lx)\n", msg, (const char*)buf, err);
	LocalFree(buf);
}


/********************************************************************
** Open a COM port, configure DCB for RTS/CTS flow control and the **
** requested baud rate. Returns TRUE on success.                   **
********************************************************************/
static BOOL OpenComPort(LPCSTR pszPort,
	HANDLE* phCom,
	DWORD baudRate,
	BOOL enableRTSCTS)
{
	*phCom = CreateFileA(pszPort,
		GENERIC_READ | GENERIC_WRITE,
		0,						// exclusive
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (*phCom == INVALID_HANDLE_VALUE)
	{
		PrintLastError("CreateFile");
		return FALSE;
	}

	DCB	dcb = { 0 };
	dcb.DCBlength = sizeof(dcb);

	if (!GetCommState(*phCom, &dcb))
	{
		PrintLastError("GetCommState");
		CloseHandle(*phCom);
		return FALSE;
	}

	// 1. Basic data format
	dcb.BaudRate = baudRate;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;

	// 2. Flow-control – use RTS/CTS handshake
	dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;	// Enable RTS/CTS handshaking - the important line!
	dcb.fDtrControl = DTR_CONTROL_DISABLE;		// often left disabled for a null-modem
	dcb.fOutxCtsFlow = FALSE;					// **MUST be FALSE** for hand-shake usage
	dcb.fOutxDsrFlow = FALSE;
	dcb.fInX = FALSE;							// no software XON/XOFF
	dcb.fOutX = FALSE;

	// 3. Apply the DCB
	if (!SetCommState(hCom, &dcb)) {
		PrintLastError("SetCommState");
		CloseHandle(*phCom);
		return FALSE;
	}

	// 4. Set Timeouts
	COMMTIMEOUTS to = { 0 };
	to.ReadIntervalTimeout = 0;					// nonspecified
	to.ReadTotalTimeoutConstant = 50;			// 50 ms max per read
	to.ReadTotalTimeoutMultiplier = 0;
	to.WriteTotalTimeoutConstant = 0;
	to.WriteTotalTimeoutMultiplier = 0;

	if (!SetCommTimeouts(*phCom, &to))
	{
		PrintLastError("SetCommTimeouts");
		CloseHandle(*phCom);
		return FALSE;
	}

	return TRUE;
}

/*******************************************
** Simple Transmission Progress Indicator **
*******************************************/
static void PrintProgress(ULONGLONG sentBytes)
{
	// \r returns the cursor to the start of the line & fflush forces the output
	printf("\rSent %llu bytes", sentBytes);
	fflush(stdout);
}

/*****************
** Main Program **
*****************/
int __cdecl main(int argc, char* argv[])
{
	if (argc < 3 || argc > 4)
	{
		printf("Usage: %s <PathToFile> <BaudRate> [COMx]\n", argv[0]);
		return 1;
	}

	const char*	pszFile = argv[1];								// file to send
	const char*	pszBaudStr = argv[2];							// e.g. "4800"
	const char*	pszPortSpec = (argc == 4) ? argv[3] : "COM2";	// default COM port
	DWORD		baudRate;

	baudRate = (DWORD)strtoul(pszBaudStr, NULL, 10);
	if (baudRate == 0)
	{
		fprintf(stderr, "Invalid baud-rate \"%s\".\n", pszBaudStr);
		return 1;
	}

	// Open the source file (binary) */
	HANDLE hFile = CreateFileA(pszFile,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	
	if (hFile == INVALID_HANDLE_VALUE)
	{
		PrintLastError("CreateFile (source file)");
		return 1;
	}

	// Open the serial port with RTS/CTS enabled
	HANDLE hCom = INVALID_HANDLE_VALUE;
	if (!OpenComPort_RTS(pszPortSpec,
		&hCom,
		baudRate,
		TRUE))			// TRUE = enable RTS/CTS flow control
	{
		CloseHandle(hFile);
		return 1;		// error already printed
	}

	// Print program info
	printf("Sending	\"%s\" at %lu baud on %s (RTS/CTS enabled)\n",
		pszFile,
		(unsigned long)baudRate,
		pszPortSpec
	);

	// ----------  Stream the file to the Serial Port
	const DWORD	CHUNK_SIZE = 1 * 1024;  // 1KiB
	char		chunk[CHUNK_SIZE];
	DWORD		bytesRead;
	DWORD		bytesWritten;
	ULONGLONG	totalSent = 0;

	while (TRUE) {
		if (!ReadFile(hFile, chunk, CHUNK_SIZE, &bytesRead, NULL) || bytesRead == 0)
		{
			if (GetLastError() == ERROR_HANDLE_EOF) {
				break;			// normal end of file
			}
			PrintLastError("ReadFile");
			break;
		}

		if (!WriteFile(hCom, chunk, bytesRead, &bytesWritten, NULL) ||
			bytesWritten != bytesRead) {
			PrintLastError("WriteFile");
			break;
		}

		totalSent += bytesWritten;
		PrintProgress(totalSent);
	}

	printf("\n\nTransmission finished –	%llu bytes sent.\n", (unsigned long	long)totalSent);

	// Clean up and exit
	CloseHandle(hFile);
	CloseHandle(hCom);
	return 0;
}

/*********************************************************************
 *  INP – a tiny Win32 console program that reads data from a COM port
 *         and writes it to a file. Once transmission ends press ESC
 *         to terminate.
 *  Run:
 *      INP.exe  <FilePath>  [COM-Port] [BaudRate]
 *
 *  Example:
 *      INP.exe C:\Temp\myfile.bin COM1 9600
 *
 *  The code uses only the Win32 API and therefore compiles with
 *  Visual Studio 2022 on Windows 11.
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>   // Required for _kbhit() and _getch()
#include <windows.h>

void PrintUsage() {
    printf("Usage: INP <filename> [COM_port] [baud_rate]\n");
    printf("Defaults: COM_port = COM2, baud_rate = 4800\n");
    printf("Press ESC at any time to stop receiving and close the file.\n");
}

int main(int argc, char* argv[]) {
    // Validate arguments (1 required, 2 optional)
    if (argc < 2 || argc > 4)
    {
        PrintUsage();
        return 1;
    }

    const char* filename = argv[1];
    const char* portName = "COM2";      // Default port
    DWORD baudRate = 4800;              // Default baud rate

    if (argc >= 3)
    {
        portName = argv[2];
    }

    if (argc == 4)
    {
        baudRate = (DWORD)strtoul(argv[3], NULL, 10);
    }

    // 1. Open the destination file safely using fopen_s
    FILE* file = NULL;
    errno_t errNum = fopen_s(&file, filename, "wb"); // 'wb' creates or overwrites a binary file
    if (errNum != 0 || file == NULL)
    {
        fprintf(stderr, "Error: Could not create output file '%s' (Error code: %d)\n", filename, errNum);
        return 1;
    }

    // 2. Format the COM port name safely using _snprintf_s
    char formattedPort[MAX_PATH];
    _snprintf_s(formattedPort, sizeof(formattedPort), _TRUNCATE, "\\\\.\\%s", portName);

    // 3. Open the serial port for Reading
    HANDLE hSerial = CreateFileA(
        formattedPort,
        GENERIC_READ,   // Request Read access
        0,              // Exclusive access
        NULL,
        OPEN_EXISTING,
        0,              // Synchronous I/O
        NULL
    );

    if (hSerial == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        fprintf(stderr, "Error: Could not open %s", portName);
        if (err == ERROR_ACCESS_DENIED)
        {
            fprintf(stderr, " (Access Denied. Is another program using it?)");
        }
        else if (err == ERROR_FILE_NOT_FOUND)
        {
            fprintf(stderr, " (Port not found)");
        }
        else
        {
            fprintf(stderr, " (Error code: %lu)", err);
        }
        fprintf(stderr, "\n");
        fclose(file);
        return 1;
    }

    // 4. Configure the serial port parameters (8N1 + CTS/RTS Flow Control)
    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hSerial, &dcbSerialParams)) {
        fprintf(stderr, "Error: Could not get device state.\n");
        CloseHandle(hSerial);
        fclose(file);
        return 1;
    }

    dcbSerialParams.BaudRate = baudRate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    // Retaining hardware flow control configuration
    dcbSerialParams.fOutxCtsFlow = TRUE;
    dcbSerialParams.fRtsControl = RTS_CONTROL_HANDSHAKE;

    if (!SetCommState(hSerial, &dcbSerialParams))
    {
        fprintf(stderr, "Error: Could not set serial port parameters.\n");
        CloseHandle(hSerial);
        fclose(file);
        return 1;
    }

    // 5. Configure Comm Timeouts for Non-Blocking polling
    COMMTIMEOUTS timeouts = { 0 };
    // Setting ReadIntervalTimeout to MAXDWORD makes ReadFile return instantly 
    // with whatever bytes are in the hardware buffer, without waiting.
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;

    if (!SetCommTimeouts(hSerial, &timeouts))
    {
        fprintf(stderr, "Error: Could not set serial timeouts.\n");
        CloseHandle(hSerial);
        fclose(file);
        return 1;
    }

    // 6. Receive Data Loop
    printf("Listening on %s at %lu baud...\n", portName, baudRate);
    printf("Writing to file: %s\n", filename);
    printf("--> PRESS ESC TO STOP AND SAVE <--\n\n");

    char rxBuffer[512]; // 512-byte chunk buffer
    DWORD bytesRead = 0;
    long long totalBytesReceived = 0;
    int runLoop = 1;

    while (runLoop) {
        // Check if a keyboard key has been pressed
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 27)   // 27 is the ASCII value for the ESC key
            {
                printf("\nESC pressed. Stopping capture.\n");
                runLoop = 0;
                break;
            }
        }

        // Poll the serial hardware buffer
        if (ReadFile(hSerial, rxBuffer, sizeof(rxBuffer), &bytesRead, NULL))
        {
            if (bytesRead > 0)
            {
                // Write the raw binary bytes directly to the file
                size_t bytesWritten = fwrite(rxBuffer, 1, bytesRead, file);
                if (bytesWritten != bytesRead)
                {
                    fprintf(stderr, "\nError: Failed to write data to disk storage.\n");
                    break;
                }

                totalBytesReceived += bytesRead;
                printf("\rReceived: %lld bytes...", totalBytesReceived);
                fflush(stdout); // Keep console counter fluid
            }
        }
        else
        {
            fprintf(stderr, "\nError: Failed to read from serial port.\n");
            break;
        }

        // Yield slightly to prevent 100% CPU core utilization during idle loops
        Sleep(1);
    }

    // 7. Resource Cleanup and flush to disk
    printf("\nFinalizing... Total data captured: %lld bytes.\n", totalBytesReceived);
    CloseHandle(hSerial);
    fclose(file);
    return 0;
}

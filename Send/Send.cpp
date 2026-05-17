/*********************************************************************
 *  SEND – a tiny Win32 console program that sends a file to a COM port
 *         using CTS (Clear To Send) hardware hand-shake.
 *
 *  Run:
 *      SEND.exe  <FilePath>  [COM-Port] [BaudRate]
 *
 *  Example:
 *      SEND.exe C:\Temp\myfile.bin COM1 9600
 *
 *  The code uses only the Win32 API and therefore compiles with
 *  Visual Studio 2022 on Windows 11.
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

void PrintUsage() {
    printf("Usage: SEND <filename> [COM_port] [baud_rate]\n");
    printf("Defaults: COM_port = COM2, baud_rate = 4800\n");
    printf("Examples:\n");
    printf("  SEND data.bin\n");
    printf("  SEND data.bin COM3\n");
    printf("  SEND data.bin COM3 115200\n");
}

int main(int argc, char* argv[]) {
    // We now require at least the filename (argc >= 2) and at most 3 parameters (argc <= 4)
    if (argc < 2 || argc > 4) {
        PrintUsage();
        return 1;
    }

    const char* filename = argv[1];
    const char* portName = "COM2";      // Default port
    DWORD baudRate = 4800;              // Default baud rate

    // Evaluate optional arguments based on count
    if (argc >= 3) {
        portName = argv[2];
    }
    if (argc == 4) {
        baudRate = (DWORD)strtoul(argv[3], NULL, 10);
    }

    // 1. Open and read the source file safely using fopen_s
    FILE* file = NULL;
    errno_t errNum = fopen_s(&file, filename, "rb");
    if (errNum != 0 || file == NULL) {
        fprintf(stderr, "Error: Could not open file '%s' (Error code: %d)\n", filename, errNum);
        return 1;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (fileSize <= 0) {
        fprintf(stderr, "Error: File is empty or invalid.\n");
        fclose(file);
        return 1;
    }

    // Allocate memory block for file data
    char* buffer = (char*)malloc(fileSize);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        fclose(file);
        return 1;
    }

    size_t bytesRead = fread(buffer, 1, fileSize, file);
    fclose(file);

    if (bytesRead != (size_t)fileSize) {
        fprintf(stderr, "Error: Failed to read entire file.\n");
        free(buffer);
        return 1;
    }

    // 2. Format the COM port name safely using _snprintf_s
    char formattedPort[MAX_PATH];
    _snprintf_s(formattedPort, sizeof(formattedPort), _TRUNCATE, "\\\\.\\%s", portName);

    // 3. Open the serial port
    HANDLE hSerial = CreateFileA(
        formattedPort,
        GENERIC_WRITE,
        0,              // Exclusive access
        NULL,           // Security attributes
        OPEN_EXISTING,  // Must exist
        0,              // Synchronous I/O
        NULL
    );

    if (hSerial == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        fprintf(stderr, "Error: Could not open %s", portName);
        if (err == ERROR_ACCESS_DENIED) {
            fprintf(stderr, " (Access Denied. Is another program using it?)");
        }
        else if (err == ERROR_FILE_NOT_FOUND) {
            fprintf(stderr, " (Port not found)");
        }
        else {
            fprintf(stderr, " (Error code: %lu)", err);
        }
        fprintf(stderr, "\n");
        free(buffer);
        return 1;
    }

    // 4. Configure the serial port hardware state (8N1 + CTS/RTS Flow Control)
    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hSerial, &dcbSerialParams)) {
        fprintf(stderr, "Error: Could not get device state.\n");
        CloseHandle(hSerial);
        free(buffer);
        return 1;
    }

    dcbSerialParams.BaudRate = baudRate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    // Retaining hardware flow control
    dcbSerialParams.fOutxCtsFlow = TRUE;
    dcbSerialParams.fRtsControl = RTS_CONTROL_HANDSHAKE;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        fprintf(stderr, "Error: Could not set serial port parameters.\n");
        CloseHandle(hSerial);
        free(buffer);
        return 1;
    }

    // 5. Send the data
    printf("Sending %ld bytes to %s at %lu baud (Hardware Flow Control enabled)...\n", fileSize, portName, baudRate);

    DWORD bytesWritten = 0;
    if (!WriteFile(hSerial, buffer, (DWORD)fileSize, &bytesWritten, NULL)) {
        fprintf(stderr, "Error: Failed to write data to serial port.\n");
        CloseHandle(hSerial);
        free(buffer);
        return 1;
    }

    printf("Success: Sent %lu bytes successfully.\n", bytesWritten);

    // 6. Resource Clean up
    CloseHandle(hSerial);
    free(buffer);
    return 0;
}

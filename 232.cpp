// 232.cpp : Определяет точку входа для приложения.
//

#include "framework.h"
#include "232.h"

#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <ctime>
#include <stdint.h>
#include <stdio.h>
#include <iomanip>

#define ID_LISTBOX 101
#define ID_BUTTON_OPEN 102
#define ID_BUTTON_CLOSE 103
#define ID_BUTTON_CLEAR 104
#define ID_COMBO_PORTS 105
#define ID_BUTTON_REFRESH 106
#define ID_EDIT_BAUDRATE 107
#define ID_EDIT_BITDATA 1071
#define ID_EDIT_BITPARITY 1072
#define ID_EDIT_BITSTOP 1073
#define ID_BUTTON_SEND 108
#define ID_EDIT_SEND 109
#define ID_TIMER_READ 110
#define ID_STATIC_MODE 111
#define ID_RADIO_TEXT 112
#define ID_RADIO_HEX 113
#define ID_BUTTON_SAVE 114
#define ID_CHECK_AUTOSAVE 115
#define ID_EDIT_FILENAME 116

#define ID_BUTTON_PREPARATION 117
//#define ID_EDIT_CRC 118
#define ID_EDIT_CMD 119
#define ID_EDIT_DATA 120
#define ID_BUTTON_WRITEDATA 121
#define ID_TEST_ASCII 122
#define ID_TEST_HEX 123

#define fon RGB(0,200,200)
HFONT hFont1, hFont2, hFont3;

HWND g_hList, g_hComboPorts, g_hEditBaud, g_hEditbitdata, g_hEditbitparity, g_hEditbitstop, g_hEditSend, Editcrc, Edit_cmd, Edit_data, button_writedata;
HWND g_hRadioText, g_hRadioHex, g_hCheckAutoSave, g_hEditFileName;
HANDLE g_hComPort = INVALID_HANDLE_VALUE;
bool g_bPortOpened = false;
bool g_bHexMode = true;
bool g_bAutoSave = false;
std::wofstream g_logFile;
std::wstring g_currentLogFile;

// контрольная сумма
uint16_t crc(uint8_t* data) {
    uint16_t crc = 0xFFFF;  // Начальное значение
    return crc & 0xFFFF;  // Обеспечиваем 16-битный результат
}

// преобразование чисел в строку
std::string BYTES_TO_HEX_STRING(const uint8_t* DATA, size_t SIZE) {
    std::stringstream SS;
    SS << std::hex << std::uppercase << std::setfill('0');

    for (size_t I = 0; I < SIZE; I++) {
        SS << std::setw(2) << static_cast<int>(DATA[I]);
        if (I < SIZE - 1) SS << " ";
    }

    return SS.str();
}

// Простая функция добавления в список
void AddToList(const std::wstring& text, bool logToFile = true) {
    // Добавляем в список
    SendMessage(g_hList, LB_ADDSTRING, 0, (LPARAM)text.c_str());
    int count = (int)SendMessage(g_hList, LB_GETCOUNT, 0, 0);
    if (count > 0) {
        SendMessage(g_hList, LB_SETTOPINDEX, count - 1, 0);
    }

    // Логируем в файл, если включено
    if (logToFile && g_logFile.is_open()) {
        // Добавляем временную метку
        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t timestamp[64];
        wsprintf(timestamp, L"[%02d:%02d:%02d.%03d] ",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

        g_logFile << timestamp << text << std::endl;
        g_logFile.flush();
    }
}

// Получение текущего времени для имени файла
std::wstring GetCurrentDateTimeForFilename() {
    SYSTEMTIME st;
    GetLocalTime(&st);

    wchar_t buffer[64];
    wsprintf(buffer, L"%04d%02d%02d_%02d%02d%02d",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);

    return buffer;
}

// Открытие файла для логирования
bool OpenLogFile(const std::wstring& filename) {
    if (g_logFile.is_open()) {
        g_logFile.close();
    }

    g_logFile.open(filename, std::ios::app);
    if (g_logFile.is_open()) {
        g_currentLogFile = filename;

        // Добавляем заголовок
        SYSTEMTIME st;
        GetLocalTime(&st);
        g_logFile << L"=== "
            << st.wYear << L"/" << st.wMonth << L"/" << st.wDay
            << L" " << st.wHour << L":" << st.wMinute << L":" << st.wSecond
            << L" ===" << std::endl;
        g_logFile.flush();

        return true;
    }
    return false;
}

// Закрытие файла логирования
void CloseLogFile() {
    if (g_logFile.is_open()) {
        g_logFile << L"=== end ===" << std::endl;
        g_logFile.close();
    }
    g_currentLogFile.clear();
}

// HEX представление байта
std::wstring ByteToHex(BYTE b) {
    wchar_t hex[3];
    static const wchar_t* hexDigits = L"0123456789ABCDEF";
    hex[0] = hexDigits[(b >> 4) & 0x0F];
    hex[1] = hexDigits[b & 0x0F];
    hex[2] = L'\0';
    return hex;
}

// Преобразование HEX символа в число
BYTE HexCharToByte(wchar_t c) {
    if (c >= L'0' && c <= L'9') return c - L'0';
    if (c >= L'A' && c <= L'F') return c - L'A' + 10;
    if (c >= L'a' && c <= L'f') return c - L'a' + 10;
    return 0;
}

// Проверка, является ли символ HEX
bool IsHexChar(wchar_t c) {
    return (c >= L'0' && c <= L'9') ||
        (c >= L'A' && c <= L'F') ||
        (c >= L'a' && c <= L'f');
}

// Преобразование HEX строки в байты
std::vector<BYTE> HexStringToBytes(const std::wstring& hexStr) {
    std::vector<BYTE> bytes;
    std::wstring cleanStr;

    // Очищаем строку от пробелов и других разделителей
    for (wchar_t c : hexStr) {
        if (IsHexChar(c)) {
            cleanStr += towupper(c); // Преобразуем в верхний регистр
        }
    }

    // Конвертируем пары символов в байты
    for (size_t i = 0; i < cleanStr.length(); i += 2) {
        if (i + 1 < cleanStr.length()) {
            BYTE high = HexCharToByte(cleanStr[i]);
            BYTE low = HexCharToByte(cleanStr[i + 1]);
            bytes.push_back((high << 4) | low);
        }
        else {
            // Нечетное количество символов - добавляем 0 в качестве младшей тетрады
            BYTE high = HexCharToByte(cleanStr[i]);
            bytes.push_back(high << 4);
        }
    }

    return bytes;
}

// Преобразование байтов в HEX строку с форматированием
std::wstring BytesToHexString(const BYTE* data, size_t length, bool formatSpaces = true) {
    std::wstring result;

    for (size_t i = 0; i < length; i++) {
        result += ByteToHex(data[i]);
        if (formatSpaces && i < length - 1) {
            result += L" ";
            if ((i + 1) % 8 == 0) {
                result += L" ";
            }
        }
    }

    return result;
}

// Получение списка портов
std::vector<std::wstring> GetPorts() {
    std::vector<std::wstring> ports;

    // Проверяем COM1-COM16
    for (int i = 1; i <= 16; i++) {
        std::wstring port = L"COM" + std::to_wstring(i);
        std::wstring portName = port;

        // Для портов выше COM9 нужно использовать префикс
        if (i > 9) {
            portName = L"\\\\.\\" + port;
        }

        HANDLE h = CreateFile(portName.c_str(), GENERIC_READ | GENERIC_WRITE,
            0, NULL, OPEN_EXISTING, 0, NULL);

        if (h != INVALID_HANDLE_VALUE) {
            ports.push_back(port);
            CloseHandle(h);
        }
    }

    return ports;
}

// Чтение из порта
void ReadPort() {
    if (g_hComPort == INVALID_HANDLE_VALUE) return;

    DWORD errors;
    COMSTAT comStat;
    BYTE buffer[512];
    DWORD bytesRead;

    ClearCommError(g_hComPort, &errors, &comStat);

    if (comStat.cbInQue > 0) {
        DWORD toRead = (comStat.cbInQue < sizeof(buffer)) ? comStat.cbInQue : sizeof(buffer);

        if (ReadFile(g_hComPort, buffer, toRead, &bytesRead, NULL)) {
            if (bytesRead > 0) {
                // Время фиксируем в момент получения данных
                SYSTEMTIME time_log;
                GetLocalTime(&time_log);
                // Всегда показываем входящие данные в HEX
                std::wstring hexStr = BytesToHexString(buffer, bytesRead, true);

                // Также показываем ASCII представление
                std::wstring asciiStr;
                for (DWORD i = 0; i < bytesRead; i++) {
                    if (buffer[i] >= 32 && buffer[i] < 127) {
                        asciiStr += (wchar_t)buffer[i];
                    }
                    else {
                        asciiStr += L".";
                    }
                }
                // Метка времени отдельным буфером: дата + время
                wchar_t timeBuf[64];
                swprintf(timeBuf, 64, L"%04d-%02d-%02d %02d:%02d:%02d.%03d",
                    time_log.wYear, time_log.wMonth, time_log.wDay,
                    time_log.wHour, time_log.wMinute,
                    time_log.wSecond, time_log.wMilliseconds);

                // Отображаем результат
                std::wstring msg = std::wstring(timeBuf) + L" RX [" + std::to_wstring(bytesRead) + L"]: " + hexStr;
                if (!asciiStr.empty()) {
                    msg += L"    [" + asciiStr + L"]";
                }
                AddToList(msg, true);
            }
        }
    }
}

// Открытие порта
bool OpenPort(const std::wstring& portName, unsigned long baud, int bit_data, int bit_parity, int bit_stop) {
    if (g_hComPort != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hComPort);
    }

    // Формируем правильное имя порта
    std::wstring fullPortName = portName;
    if (portName.length() > 4) { // COM10 и выше
        fullPortName = L"\\\\.\\" + portName;
    }

    g_hComPort = CreateFile(fullPortName.c_str(), GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, 0, NULL);

    if (g_hComPort == INVALID_HANDLE_VALUE) {
        return false;
    }

    // Настройка
    DCB dcb = { 0 };
    dcb.DCBlength = sizeof(DCB);
    GetCommState(g_hComPort, &dcb);

    dcb.BaudRate = baud;
    dcb.ByteSize = bit_data;
    dcb.StopBits = bit_stop;//ONESTOPBIT;
    dcb.Parity = bit_parity;//NOPARITY;
    dcb.fBinary = TRUE; // Бинарный режим
    dcb.fDtrControl =  DTR_CONTROL_ENABLE;
    dcb.fRtsControl =  RTS_CONTROL_ENABLE;

    if (!SetCommState(g_hComPort, &dcb)) {
        CloseHandle(g_hComPort);
        g_hComPort = INVALID_HANDLE_VALUE;
        return false;
    }

    // Таймауты
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    SetCommTimeouts(g_hComPort, &timeouts);

    // Очистка буферов
    PurgeComm(g_hComPort, PURGE_TXCLEAR | PURGE_RXCLEAR);

    g_bPortOpened = true;
    return true;
}

// Закрытие порта
void ClosePort() {
    if (g_hComPort != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hComPort);
        g_hComPort = INVALID_HANDLE_VALUE;
    }
    g_bPortOpened = false;
}

// Отправка данных
bool SendData(const BYTE* data, DWORD length) {
    if (g_hComPort == INVALID_HANDLE_VALUE) return false;

    DWORD bytesWritten;
    return WriteFile(g_hComPort, data, length, &bytesWritten, NULL);
}

// Сохранение текущего лога в файл
void SaveCurrentLog() {
    if (!g_bAutoSave) {
        // Если автосохранение выключено, создаем новый файл
        std::wstring filename = L"ZR_" + GetCurrentDateTimeForFilename() + L".txt";

        if (OpenLogFile(filename)) {
            AddToList(L"Save: " + filename, false);

            // Сохраняем историю из списка
            int count = (int)SendMessage(g_hList, LB_GETCOUNT, 0, 0);
            for (int i = 0; i < count; i++) {
                wchar_t buffer[1024];
                SendMessage(g_hList, LB_GETTEXT, i, (LPARAM)buffer);
                g_logFile << buffer << std::endl;
            }
            g_logFile.flush();
            AddToList(L"Total saved " + std::to_wstring(count) + L" rows", false);
        }
        else {
            AddToList(L"Failed save", false);
        }
    }
    else {
        // При автосохранении просто показываем текущий файл
        if (!g_currentLogFile.empty()) {
            AddToList(L"Save: " + g_currentLogFile, false);
        }
    }
}

// Оконная процедура
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {

        hFont1 = CreateFont(120, 70, 0, 0, 700, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, L"Times New Roman");
        hFont2 = CreateFont(20, 7, 0, 0, 700, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, L"Arial");
        hFont3 = CreateFont(25, 7, 0, 0, 500, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, L"GOST type A");

        SendMessage(hwnd, WM_SETFONT, WPARAM(hFont1), 0);// установить шрифт
        SendMessage(hwnd, WM_SETFONT, WPARAM(hFont2), 0);// установить шрифт
        SendMessage(hwnd, WM_SETFONT, WPARAM(hFont3), 0);// установить шрифт

        // элементы управления
        int x = 10, y = 10;

        CreateWindow(L"STATIC", L"Порт:", WS_CHILD | WS_VISIBLE,
            x, y, 40, 25, hwnd, NULL, NULL, NULL);
        x += 50;

        g_hComboPorts = CreateWindow(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            x, y, 100, 200, hwnd, (HMENU)ID_COMBO_PORTS, NULL, NULL);
        x += 110;

        CreateWindow(L"BUTTON", L"Обновить", WS_CHILD | WS_VISIBLE,
            x, y, 80, 25, hwnd, (HMENU)ID_BUTTON_REFRESH, NULL, NULL);
        x += 90;

        CreateWindow(L"STATIC", L"Имя файла автосохранения:", WS_CHILD | WS_VISIBLE,
            x, y, 200, 25, hwnd, NULL, NULL, NULL);
        x += 200;

        g_hEditFileName = CreateWindow(L"EDIT", L"ZR.txt", WS_CHILD | WS_VISIBLE | WS_BORDER,
            x, y, 150, 25, hwnd, (HMENU)ID_EDIT_FILENAME, NULL, NULL);

        ///////////////////////////// вторая строка - настройки порта
        y += 35;
        x = 10;

        CreateWindow(L"STATIC", L"Скорость:", WS_CHILD | WS_VISIBLE,
            x, y, 80, 25, hwnd, NULL, NULL, NULL);
        x += 70;

        g_hEditBaud = CreateWindow(L"EDIT", L"9600", WS_CHILD | WS_VISIBLE | WS_BORDER,
            x, y, 70, 25, hwnd, (HMENU)ID_EDIT_BAUDRATE, NULL, NULL);
        x += 80;

        CreateWindow(L"STATIC", L"Данных:", WS_CHILD | WS_VISIBLE,
            x, y, 80, 25, hwnd, NULL, NULL, NULL);
        x += 70;

        g_hEditbitdata = CreateWindow(L"EDIT", L"8", WS_CHILD | WS_VISIBLE | WS_BORDER,
            x, y, 20, 25, hwnd, (HMENU)ID_EDIT_BITDATA, NULL, NULL);
        x += 30;

        CreateWindow(L"STATIC", L"Четность:", WS_CHILD | WS_VISIBLE,
            x, y, 80, 25, hwnd, NULL, NULL, NULL);
        x += 70;

        g_hEditbitparity = CreateWindow(L"EDIT", L"0", WS_CHILD | WS_VISIBLE | WS_BORDER,
            x, y, 20, 25, hwnd, (HMENU)ID_EDIT_BITPARITY, NULL, NULL);
        x += 30;

        CreateWindow(L"STATIC", L"Стоп бит:", WS_CHILD | WS_VISIBLE,
            x, y, 80, 25, hwnd, NULL, NULL, NULL);
        x += 70;

        g_hEditbitstop = CreateWindow(L"EDIT", L"0", WS_CHILD | WS_VISIBLE | WS_BORDER,
            x, y, 20, 25, hwnd, (HMENU)ID_EDIT_BITSTOP, NULL, NULL);
        x += 30;

        CreateWindow(L"BUTTON", L"Открыть", WS_CHILD | WS_VISIBLE,
            x, y, 70, 25, hwnd, (HMENU)ID_BUTTON_OPEN, NULL, NULL);
        x += 80;

        CreateWindow(L"BUTTON", L"Закрыть", WS_CHILD | WS_VISIBLE,
            x, y, 70, 25, hwnd, (HMENU)ID_BUTTON_CLOSE, NULL, NULL);

        // третья строка - режим ввода и сохранение
        y += 35;
        x = 10;

        CreateWindow(L"STATIC", L"Режим ввода:", WS_CHILD | WS_VISIBLE,
            x, y, 100, 25, hwnd, (HMENU)ID_STATIC_MODE, NULL, NULL);
        x += 110;

        g_hRadioText = CreateWindow(L"BUTTON", L"ASCII", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            x, y, 60, 25, hwnd, (HMENU)ID_RADIO_TEXT, NULL, NULL);
        x += 70;

        g_hRadioHex = CreateWindow(L"BUTTON", L"HEX", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            x, y, 60, 25, hwnd, (HMENU)ID_RADIO_HEX, NULL, NULL);
        SendMessage(g_hRadioHex, BM_SETCHECK, BST_CHECKED, 0); // HEX по умолчанию
        x += 70;

        g_hCheckAutoSave = CreateWindow(L"BUTTON", L"Auto Save", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            x, y, 90, 25, hwnd, (HMENU)ID_CHECK_AUTOSAVE, NULL, NULL);
        SendMessage(g_hCheckAutoSave, BM_SETCHECK, BST_CHECKED, 0); // Автосохранение по умолчанию
        x += 100;

        CreateWindow(L"BUTTON", L"Сохранить", WS_CHILD | WS_VISIBLE,
            x, y, 80, 25, hwnd, (HMENU)ID_BUTTON_SAVE, NULL, NULL);

        x += 100;

        CreateWindow(L"BUTTON", L"Очистить лог", WS_CHILD | WS_VISIBLE,
            x, y, 100, 25, hwnd, (HMENU)ID_BUTTON_CLEAR, NULL, NULL);
        
        x += 850;

        // Список для отображения данных
        y += 35;
        g_hList = CreateWindow(L"LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL,
            10, y, 1150, 500, hwnd, (HMENU)ID_LISTBOX, NULL, NULL);

        // Нижняя строка - отправка данных
        y += 510;
        x = 10;

        CreateWindow(L"STATIC", L"Для отправления:", WS_CHILD | WS_VISIBLE,
            x, y, 140, 25, hwnd, NULL, NULL, NULL);
        x += 140;

        g_hEditSend = CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
            x, y, 910, 25, hwnd, (HMENU)ID_EDIT_SEND, NULL, NULL);
        x += 920;

        CreateWindow(L"BUTTON", L"Отправить", WS_CHILD | WS_VISIBLE,
            x, y, 80, 25, hwnd, (HMENU)ID_BUTTON_SEND, NULL, NULL);

        /////////////////////////////////////////////////////////////////////////////
            // кнопки для быстрого тестирования
        y += 35;
        x = 150;


        CreateWindow(L"STATIC", L"Пример заполнения:", WS_CHILD | WS_VISIBLE,
            10, y, 150, 25, hwnd, NULL, NULL, NULL);

        x += 20;
        CreateWindow(L"BUTTON", L"для отправки в ASCII", WS_CHILD | WS_VISIBLE,
            x, y, 150, 25, hwnd, (HMENU)ID_TEST_ASCII, NULL, NULL);

        x += 180;

        CreateWindow(L"BUTTON", L"для отправки в HEX", WS_CHILD | WS_VISIBLE,
            x, y, 150, 25, hwnd, (HMENU)ID_TEST_HEX, NULL, NULL);
          
          // Заполняем список портов
        std::vector<std::wstring> ports = GetPorts();
        for (const auto& port : ports) {
            SendMessage(g_hComboPorts, CB_ADDSTRING, 0, (LPARAM)port.c_str());
        }
        if (!ports.empty()) {
            SendMessage(g_hComboPorts, CB_SETCURSEL, 0, 0);
        }

        // Автоматически открываем файл логирования
        g_bAutoSave = (SendMessage(g_hCheckAutoSave, BM_GETCHECK, 0, 0) == BST_CHECKED);
        if (g_bAutoSave) {
            wchar_t filename[256];
            GetWindowText(g_hEditFileName, filename, 256);
            if (OpenLogFile(filename)) {
                AddToList(L"Autosave: " + std::wstring(filename), false);
            }
        }

        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);

        if (id == ID_BUTTON_PREPARATION) { // подготовка
            char CRC[256] = {};
            uint16_t result, result_H, result_L;
            wchar_t cmd[6] = {};
            GetWindowText(Edit_cmd, cmd, 6);
            //long num = std::stoi(cmd);
            uint8_t dat[43] = { 0x55, 0xAA, 0x0A, 0x80, 0x00, 0x01, 0x00, 0x00 };
            dat[6] = ((std::stoi(cmd)) >> 8) & 0xff;
            dat[7] = (std::stoi(cmd)) & 0xFF;
            //if (dat[7] == 13 || dat[7] == 17)
            //{
                ShowWindow(Edit_data, 1);
                ShowWindow(button_writedata, 1);
                SendMessageA(g_hEditSend, WM_SETTEXT, 0, (LPARAM)"");
                //return 0;
            //}
            //else
            //{
            //    ShowWindow(Edit_data, 0);
             //   ShowWindow(button_writedata, 0);
            //}
            result = crc(dat);
            result_H = (result >> 8) & 0xff;
            result_L = result & 0xff;
            dat[dat[2] - 2] = result_H;
            dat[dat[2] - 1] = result_L;
            //wsprintfA(&CRC[0], "%02X %02X", result_H,result_L);//преобразуем число в символ
            //SendMessageA(Editcrc, WM_SETTEXT, 0, (LPARAM)&CRC);          
            std::string HEX_STR = BYTES_TO_HEX_STRING(dat, sizeof(dat));
            SendMessageA(g_hEditSend, WM_SETTEXT, 0, (LPARAM)HEX_STR.c_str());
        }

        else if (id == ID_BUTTON_WRITEDATA) { //записать данные
            char CRC[256] = {};
            uint8_t dat[43] = { 0x55, 0xAA, 0x0A, 0x80, 0x00, 0x01, 0x00, 0x00 };
            uint16_t result, result_H, result_L;
            wchar_t cmd[6] = {};
            GetWindowText(Edit_cmd, cmd, 6);
            dat[6] = ((std::stoi(cmd)) >> 8) & 0xff;
            dat[7] = (std::stoi(cmd)) & 0xFF;
            dat[2] = 0x0C;
            wchar_t data[6] = {};
            GetWindowText(Edit_data, data, 6);
            dat[8] = ((std::stoi(data)) >> 8) & 0xff;
            dat[9] = (std::stoi(data)) & 0xff;
            result = crc(dat);
            result_H = (result >> 8) & 0xff;
            result_L = result & 0xff;
            dat[dat[2] - 2] = result_H;
            dat[dat[2] - 1] = result_L;
            //wsprintfA(&CRC[0], "%02X %02X", result_H, result_L);//преобразуем число в символ
            //SendMessageA(Editcrc, WM_SETTEXT, 0, (LPARAM)&CRC);
            std::string HEX_STR = BYTES_TO_HEX_STRING(dat, sizeof(dat));
            SendMessageA(g_hEditSend, WM_SETTEXT, 0, (LPARAM)HEX_STR.c_str());
            //}
            //else  return 0;
        }

        else if (id == ID_BUTTON_REFRESH) { // обновить
            SendMessage(g_hComboPorts, CB_RESETCONTENT, 0, 0);
            std::vector<std::wstring> ports = GetPorts();
            for (const auto& port : ports) {
                SendMessage(g_hComboPorts, CB_ADDSTRING, 0, (LPARAM)port.c_str());
            }
            if (!ports.empty()) {
                SendMessage(g_hComboPorts, CB_SETCURSEL, 0, 0);
            }
        }
        else if (id == ID_BUTTON_OPEN) {
            int sel = SendMessage(g_hComboPorts, CB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                wchar_t portName[20];
                SendMessage(g_hComboPorts, CB_GETLBTEXT, sel, (LPARAM)portName);

                wchar_t baudStr[20];
                wchar_t dataStr[20];
                wchar_t parityStr[20];
                wchar_t stopStr[20];
                GetWindowText(g_hEditBaud, baudStr, 20);
                GetWindowText(g_hEditbitdata, dataStr, 20);
                GetWindowText(g_hEditbitparity, parityStr, 20);
                GetWindowText(g_hEditbitstop, stopStr, 20);
                int baud = _wtoi(baudStr);
                int data = _wtoi(dataStr);
                int parity = _wtoi(parityStr);
                int stop = _wtoi(stopStr);
                if (baud == 0) baud = 9600;

                if (OpenPort(portName, baud, data, parity, stop)) {
                    AddToList(L"PORT " + std::wstring(portName) + L" open, SPEED - " + std::to_wstring(baud) + L", DATA - " + std::to_wstring(data)
                        + L", PARITY - " + std::to_wstring(parity) + L", STOP BIT - " + std::to_wstring(stop));
                    SetTimer(hwnd, ID_TIMER_READ, 100, NULL);
                }
                else {
                    DWORD err = GetLastError();
                    wchar_t errMsg[100];
                    wsprintf(errMsg, L"Failed to open port (Error: %lu)", err);
                    MessageBox(hwnd, errMsg, L"Error", MB_OK | MB_ICONERROR);
                }
            }
        }
        else if (id == ID_BUTTON_CLOSE) {
            KillTimer(hwnd, ID_TIMER_READ);
            ClosePort();
            AddToList(L"Port closed");
        }
        else if (id == ID_BUTTON_CLEAR) {
            SendMessage(g_hList, LB_RESETCONTENT, 0, 0);
            AddToList(L"Cleared", false);
        }
        else if (id == ID_RADIO_TEXT || id == ID_RADIO_HEX) {
            // Обновляем режим ввода
            g_bHexMode = (SendMessage(g_hRadioHex, BM_GETCHECK, 0, 0) == BST_CHECKED);

            // Очищаем поле ввода при смене режима
            SetWindowText(g_hEditSend, L"");
        }
        else if (id == ID_CHECK_AUTOSAVE) {
            g_bAutoSave = (SendMessage(g_hCheckAutoSave, BM_GETCHECK, 0, 0) == BST_CHECKED);

            if (g_bAutoSave) {
                wchar_t filename[256];
                GetWindowText(g_hEditFileName, filename, 256);
                if (OpenLogFile(filename)) {
                    AddToList(L"Autosave enable: " + std::wstring(filename), false);
                }
            }
            else {
                CloseLogFile();
                AddToList(L"Autosave disable", false);
            }
        }
        else if (id == ID_BUTTON_SAVE) {
            SaveCurrentLog();
        }
        else if (id == ID_BUTTON_SEND) {
            if (!g_bPortOpened) {
                MessageBox(hwnd, L"Port close", L"Error", MB_OK | MB_ICONERROR);
            }
            else {
                wchar_t input[512];
                GetWindowText(g_hEditSend, input, 512);

                if (wcslen(input) > 0) {
                    if (g_bHexMode) {
                        // HEX режим
                        std::vector<BYTE> bytes = HexStringToBytes(input);

                        if (!bytes.empty()) {
                            if (SendData(bytes.data(), (DWORD)bytes.size())) {
                                std::wstring hexStr = BytesToHexString(bytes.data(), bytes.size(), true);
                                //AddToList(L"TX [" + std::to_wstring(bytes.size()) + L" bytes]: " + hexStr, true);
                                AddToList(L"TX [" + std::to_wstring(bytes.size()) + L"]: " + hexStr, true);
                                SetWindowText(g_hEditSend, L"");
                            }
                        }
                        else {
                            MessageBox(hwnd, L"HEX Error", L"Error", MB_OK | MB_ICONERROR);
                        }
                    }
                    else {
                        // Текстовый режим
                        int len = WideCharToMultiByte(CP_ACP, 0, input, -1, NULL, 0, NULL, NULL);
                        std::vector<BYTE> buffer(len);
                        WideCharToMultiByte(CP_ACP, 0, input, -1, (char*)buffer.data(), len, NULL, NULL);

                        // Убираем нулевой терминатор
                        buffer.pop_back();

                        if (!buffer.empty()) {
                            if (SendData(buffer.data(), (DWORD)buffer.size())) {
                                std::wstring hexStr = BytesToHexString(buffer.data(), buffer.size(), true);
                                AddToList(L"TX [" + std::to_wstring(buffer.size()) + L"]: " + hexStr, true);
                                SetWindowText(g_hEditSend, L"");
                            }
                        }
                    }
                }
            }
        }
        else if (id >= 200 && id <= 206) {
            // HEX данные
            const wchar_t* examples[] = {
                L"07 67 A0", L"08 96 4F", L"09 86 6E", L"0A B6 0D", L"0C D6 CB",
                L"0B A6 2C", L"05 47 E2 "
            };

            // Устанавливаем HEX режим и заполняем поле
            SendMessage(g_hRadioHex, BM_SETCHECK, BST_CHECKED, 0);
            g_bHexMode = true;
            SetWindowText(g_hEditSend, examples[id - 200]);
            SendMessageA(hwnd, WM_COMMAND, ID_BUTTON_SEND, (LPARAM)0);
        }
        else if (id == ID_TEST_ASCII) {
            SendMessage(g_hRadioHex, BM_SETCHECK, BST_CHECKED, 0);
            g_bHexMode = false;
            SetWindowText(g_hEditSend, L"Hello, Word!");
        }
        else if (id == ID_TEST_HEX) {
            SendMessage(g_hRadioHex, BM_SETCHECK, BST_CHECKED, 0);
            g_bHexMode = true;
            SetWindowText(g_hEditSend, L"48 65 6C 6C 6F 2C 20 57 6F 72 64 21");
        }
        break;
    }

    case WM_TIMER: {
        if (wParam == ID_TIMER_READ) {
            ReadPort();
        }
        break;
    }
    case WM_CTLCOLORSTATIC:// изменение цветов в статических окнах
        SetTextColor((HDC)wParam, RGB(0, 0, 0));
        SetBkColor((HDC)wParam, fon);// свой цвет фона
        return (LRESULT)CreateSolidBrush(fon);// возвращаем цвет
        break;
    case WM_CTLCOLOREDIT:// изменение цветов в динамических окнах
        SetTextColor((HDC)wParam, RGB(0, 0, 0));
        SetBkColor((HDC)wParam, RGB(220, 220, 255));
        return (LRESULT)CreateSolidBrush(RGB(220, 220, 255));// возвращаем цвет
        break;
    case WM_PAINT:
    {

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        SetTextColor(hdc, RGB(0, 200, 0));// цвет текста
        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, hFont1);
        TextOut(hdc, 950, 10, L"ZR", 2);
        SetTextColor(hdc, RGB(100, 0, 0));// цвет текста
        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, hFont2);
        TextOut(hdc, 630, 10, L"Передача в порт идет только HEX числом не зависимо от режима ввода!", 67);// текст по координатам(дискриптор окна,x,y,текст,размер)
        TextOut(hdc, 630, 30, L"Прием автоматический после открытия порта.", 42);
        TextOut(hdc, 630, 50, L"Четность: 0-4=None,Odd,Even,Mark,Space", 38);
        TextOut(hdc, 630, 70, L"Стоповых битов: 0,1,2 = 1, 1.5, 2", 33);
        EndPaint(hwnd, &ps);
    }
    break;
    case WM_CLOSE: {
        KillTimer(hwnd, ID_TIMER_READ);
        ClosePort();
        CloseLogFile();
        DestroyWindow(hwnd);
        break;
    }

    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

// Точка входа
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Регистрация класса окна
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(fon);
    wc.lpszClassName = L"COMPortLogger";

    RegisterClassEx(&wc);

    // Создание окна
    HWND hwnd = CreateWindow(wc.lpszClassName, L"RS232 sniffer ZR",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        1200, 740, NULL, NULL, hInstance, NULL);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Цикл сообщений
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

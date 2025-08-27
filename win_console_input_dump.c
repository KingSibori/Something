// win_console_input_dump.c
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

static const char* boolstr(BOOL b){ return b ? "TRUE" : "FALSE"; }

int main(void) {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "GetStdHandle(STD_INPUT_HANDLE) failed (%lu)\n", GetLastError());
        return 1;
    }

    // 콘솔 모드 설정: 모든 입력 이벤트 수집
    DWORD oldMode = 0;
    GetConsoleMode(hIn, &oldMode);
    DWORD mode = ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
    // 키보드 이벤트도 받고, 라인버퍼링/에코를 끄려면 다음 두 플래그를 제거하지 말 것:
    // ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT 제거를 위해 RAW에 가까운 모드로 전환
    mode |= ENABLE_PROCESSED_INPUT;
    if (!SetConsoleMode(hIn, mode)) {
        fprintf(stderr, "SetConsoleMode failed (%lu)\n", GetLastError());
        return 1;
    }

    while (1) {
        DWORD count = 0;
        if (!GetNumberOfConsoleInputEvents(hIn, &count)) {
            fprintf(stderr, "GetNumberOfConsoleInputEvents failed (%lu)\n", GetLastError());
            break;
        }
        printf("[PEEK] pending events = %lu\n", count);

        if (count == 0) {
            Sleep(50);
            continue;
        }

        // 버퍼 내용을 그대로 읽어 출력
        DWORD toRead = count;
        INPUT_RECORD *rec = (INPUT_RECORD*)HeapAlloc(GetProcessHeap(), 0, sizeof(INPUT_RECORD)*toRead);
        DWORD read = 0;
        if (!PeekConsoleInput(hIn, rec, toRead, &read)) {
            fprintf(stderr, "PeekConsoleInput failed (%lu)\n", GetLastError());
            HeapFree(GetProcessHeap(), 0, rec);
            break;
        }
        // Peek로 들여다본 뒤 실제 소비(Read)까지 해보고 싶다면 ReadConsoleInput 사용
        if (!ReadConsoleInput(hIn, rec, read, &read)) {
            fprintf(stderr, "ReadConsoleInput failed (%lu)\n", GetLastError());
            HeapFree(GetProcessHeap(), 0, rec);
            break;
        }

        for (DWORD i = 0; i < read; ++i) {
            INPUT_RECORD *ir = &rec[i];
            switch (ir->EventType) {
            case KEY_EVENT: {
                KEY_EVENT_RECORD k = ir->Event.KeyEvent;
                printf("KEY_EVENT: bKeyDown=%s, wRepeat=%u, vk=0x%02X, sc=0x%02X, uChar='%lc' (0x%04X), ctrl=0x%04X\n",
                       boolstr(k.bKeyDown), k.wRepeatCount, k.wVirtualKeyCode, k.wVirtualScanCode,
                       k.uChar.UnicodeChar ? k.uChar.UnicodeChar : L' ',
                       k.uChar.UnicodeChar, k.dwControlKeyState);
                break;
            }
            case MOUSE_EVENT: {
                MOUSE_EVENT_RECORD m = ir->Event.MouseEvent;
                printf("MOUSE_EVENT: pos=(%d,%d) btn=0x%08X ctrl=0x%04X flags=0x%08X\n",
                       m.dwMousePosition.X, m.dwMousePosition.Y, m.dwButtonState,
                       m.dwControlKeyState, m.dwEventFlags);
                break;
            }
            case WINDOW_BUFFER_SIZE_EVENT: {
                COORD sz = ir->Event.WindowBufferSizeEvent.dwSize;
                printf("WINDOW_BUFFER_SIZE_EVENT: size=(%d,%d)\n", sz.X, sz.Y);
                break;
            }
            case FOCUS_EVENT:
                printf("FOCUS_EVENT: %s\n", ir->Event.FocusEvent.bSetFocus ? "SET" : "LOST");
                break;
            case MENU_EVENT:
                printf("MENU_EVENT: dwCommandId=%u\n", ir->Event.MenuEvent.dwCommandId);
                break;
            default:
                printf("UNKNOWN_EVENT: type=%u\n", ir->EventType);
                break;
            }
        }
        HeapFree(GetProcessHeap(), 0, rec);
    }

    // 모드 복원 (옵션)
    SetConsoleMode(hIn, oldMode);
    return 0;
}

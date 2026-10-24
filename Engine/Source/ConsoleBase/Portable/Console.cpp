// Native output/command bridge for the original ConsoleBase/CVar implementation.
#include "Engine/Source/ConsoleBase/ConsoleCommand.h"
#include "Engine/Source/ConsoleBase/IConsole.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Storm/H/storm.h"
#include <cstdarg>
#include <cstdio>
void ConsolePostClose() { EventPostClose(); }
void ConsoleWrite(const char* text, COLOR_T) { if (text) std::fprintf(stdout, "%s\n", text); }
void ConsolePrintf(const char* format, ...) {
    char text[4096]; va_list args; va_start(args, format);
    SStrVPrintf(text, sizeof(text), format, args); va_end(args); ConsoleWrite(text, DEFAULT_COLOR);
}
void ConsoleWriteA(const char* format, COLOR_T color, ...) {
    char text[4096]; va_list args; va_start(args, format);
    SStrVPrintf(text, sizeof(text), format, args); va_end(args); ConsoleWrite(text, color);
}
void ConsoleCommandExecute(const char* line, int32_t history) {
    char arguments[8192]; const char* name = nullptr;
    auto* command = ParseCommand(line, &name, arguments, sizeof(arguments));
    if (history) AddToHistory(line);
    if (command && command->m_handler) command->m_handler(name, arguments);
    else if (g_defaultCommand) g_defaultCommand(name, arguments);
    else ConsolePrintf("Unknown console command: %s", line);
}

// clang-format off
#include <windows.h>
#include <mmsystem.h>
// clang-format on

#include "app/main.h"

#include "app/legoracers.h"
#include "decomp.h"
#include "golerror.h"
#include "types.h"

#include <objbase.h>
#include <stdlib.h>
#include <string.h>

DECOMP_SIZE_ASSERT(CommandLineArgs, 0x84)

// GLOBAL: LEGORACERS 0x004c47e4
HINSTANCE g_hInstance;

// GLOBAL: LEGORACERS 0x004c47e8
HINSTANCE g_hPrevInstance;

// GLOBAL: LEGORACERS 0x004c4a38
LegoRacers g_racers;

// GLOBAL: LEGORACERS 0x004c6ee8
LegoChar g_commandLine[256];

// GLOBAL: LEGORACERS 0x004c6fe8
CommandLineArgs g_commandLineArgs;

#ifdef COMPAT_MODE

void SplitCommand()
{
	LegoS32 offset = 0;
	g_commandLineArgs.m_argc = 0;

	if (!g_commandLine[0]) {
		return;
	}

	LegoS32 i;
	for (; g_commandLineArgs.m_argc < (LegoS32) sizeOfArray(g_commandLineArgs.m_argv);
		 offset += i, g_commandLineArgs.m_argc++) {
		LegoChar* arg = &g_commandLine[offset];

		for (i = 0; arg[i] && arg[i] != ' '; i++) {
		}

		g_commandLineArgs.m_argv[g_commandLineArgs.m_argc] = arg;

		if (!g_commandLine[offset + i]) {
			g_commandLineArgs.m_argc++;
			break;
		}

		arg[i++] = '\0';
	}
}

#else

// FUNCTION: LEGORACERS 0x00449ce0
__declspec(naked) void SplitCommand()
{
	__asm {
		mov al, byte ptr [g_commandLine]
		push esi
		push edi
		xor esi, esi
		xor edi, edi
		test al, al
		mov dword ptr [g_commandLineArgs], edi
		je L_SplitCommand_0
	L_SplitCommand_4:
		mov dl, byte ptr [esi + g_commandLine]
		lea ecx, [esi + g_commandLine]
		xor eax, eax
		test dl, dl
		je L_SplitCommand_1
	L_SplitCommand_2:
		cmp dl, 0x20
		je L_SplitCommand_1
		mov dl, byte ptr [ecx + eax + 1]
		inc eax
		test dl, dl
		jne L_SplitCommand_2
	L_SplitCommand_1:
		mov dl, byte ptr [eax + esi + g_commandLine]
		mov dword ptr [edi*4 + g_commandLineArgs + 4], ecx
		test dl, dl
		je L_SplitCommand_3
		mov byte ptr [ecx + eax], 0
		mov edi, dword ptr [g_commandLineArgs]
		inc edi
		lea esi, [esi + eax + 1]
		cmp edi, 0x20
		mov dword ptr [g_commandLineArgs], edi
		jl L_SplitCommand_4
		pop edi
		pop esi
		ret
	L_SplitCommand_3:
		inc edi
		mov dword ptr [g_commandLineArgs], edi
	L_SplitCommand_0:
		pop edi
		pop esi
		ret
	}
}

#endif

// FUNCTION: LEGORACERS 0x00449d50
int WINAPI WinMain(HINSTANCE p_hInstance, HINSTANCE p_hPrevInstance, LPSTR p_lpCmdLine, int p_nShowCmd)
{
	int result;

	CoInitialize(0);

	g_hInstance = p_hInstance;
	g_hPrevInstance = p_hPrevInstance;
	srand((unsigned int) timeGetTime);
	strncpy(g_commandLine, p_lpCmdLine, sizeof(g_commandLine));
	g_commandLine[sizeof(g_commandLine) - 1] = '\0';
	SplitCommand();

	result = GameMain(g_commandLineArgs.m_argc, g_commandLineArgs.m_argv);

	CoUninitialize();
	return result;
}

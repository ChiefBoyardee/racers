#include "save/savegame.h"

#include "golbyteorder.h"
#include "golfile.h"
#include "save/persistentgamestate.h"

#include <string.h>

DECOMP_SIZE_ASSERT(SaveGame, 0x4a8)

// GLOBAL: LEGORACERS 0x004c6b38
LegoU8 g_saveFileBlock[SaveGame::c_saveFileBlockSize];

// FUNCTION: LEGORACERS 0x00442660
SaveGame::SaveGame()
{
	::memset(m_fileImage, 0, sizeof(m_fileImage));
	m_fileOffset = 0;
}

// FUNCTION: LEGORACERS 0x00442680
SaveGame::~SaveGame()
{
	Destroy();
}

// FUNCTION: LEGORACERS 0x004426d0
void SaveGame::Initialize(undefined4 p_count, undefined4 p_recordSource, undefined4 p_saveIndex)
{
	if (m_records) {
		Destroy();
	}

	AllocateRecords(p_count, p_recordSource, p_saveIndex);
}

// FUNCTION: LEGORACERS 0x00442700
LegoS32 SaveGame::WriteFileImage(GolFile& p_file)
{
	LegoU32 recordCount = m_recordCount;

	m_fileImage[0] = c_saveFileMagic0;
	m_fileImage[1] = c_saveFileMagic1;
	m_fileImage[2] = static_cast<LegoU8>(recordCount);
	m_fileImage[3] = c_saveFileBlockSize;
	m_fileImage[4] = 4;
	m_fileOffset = 0;

	LegoS32 result = WriteBlocks(&p_file, m_fileImage, c_saveFileHeaderSize);
	if (result) {
		return result;
	}

	for (LegoU32 i = 0; i < recordCount; i++) {
		Record* record = GetRecord(i);
		result = WriteBlocks(&p_file, record->m_data, sizeof(record->m_data));
		if (result) {
			return result;
		}
	}

	m_dirty = 0;
	return GolStream::e_ioSuccess;
}

// FUNCTION: LEGORACERS 0x00442770
LegoS32 SaveGame::LoadFromFile(GolFile& p_file)
{
	m_fileOffset = 0;

	LegoS32 result = ReadBlocks(p_file, m_fileImage, c_saveFileHeaderSize);
	if (result) {
		return result;
	}

	if (m_fileImage[0] != c_saveFileMagic0 || m_fileImage[1] != c_saveFileMagic1) {
		return c_saveFileInvalidStatus;
	}

	LegoU32 recordCount = m_fileImage[2];
	if (recordCount > m_recordCapacity) {
		return c_saveFileInvalidStatus;
	}

	m_recordCount = 0;
	RebuildFreeList();
	m_fileOffset = static_cast<LegoU16>(m_fileImage[3] + (m_fileImage[4] << 8));

	for (LegoU32 i = 0; i < recordCount; i++) {
		Record* record = AllocateRecord();
		record->Initialize(m_recordSource, m_saveIndex, i, NULL);
		result = ReadBlocks(p_file, record->m_data, sizeof(record->m_data));
		if (result) {
			return result;
		}
	}

	m_unk0x10 = recordCount;
	m_dirty = 0;
	return GolStream::e_ioSuccess;
}

// FUNCTION: LEGORACERS 0x00442840
LegoS32 SaveGame::WriteBlocks(GolStream* p_file, const LegoU8* p_source, LegoU32 p_size)
{
	SaveGame* saveGame = this;
	const LegoU8* end = p_source + p_size;
	LegoU32 fileOffset;

	while (p_source < end) {
		LegoU32 chunkSize = end - p_source;
		if (chunkSize >= c_saveFileBlockDataSize) {
			chunkSize = c_saveFileBlockDataSize;
		}
		else {
			::memset(&g_saveFileBlock[chunkSize], 0, c_saveFileBlockDataSize - chunkSize);
		}

		::memcpy(g_saveFileBlock, p_source, chunkSize);
		g_saveFileBlock[c_saveFileBlockDataSize] =
			static_cast<LegoU8>(CalculateBlockChecksum(g_saveFileBlock, c_saveFileBlockDataSize));

		LegoS32 result = p_file->BufferedWrite(saveGame->m_fileOffset, g_saveFileBlock, c_saveFileBlockSize);
		if (result) {
			return result;
		}

		fileOffset = saveGame->m_fileOffset;
		fileOffset += c_saveFileBlockSize;
		saveGame->m_fileOffset = fileOffset;
		p_source += chunkSize;
	}

	return GolStream::e_ioSuccess;
}

// FUNCTION: LEGORACERS 0x00442910
LegoS32 SaveGame::ReadBlocks(GolStream& p_file, LegoU8* p_dest, LegoU32 p_size)
{
	SaveGame* saveGame = this;
	LegoU8* end = p_dest + p_size;
	LegoU32 fileOffset;

	while (p_dest < end) {
		LegoS32 bytesRead;
		LegoS32 result = p_file.BufferedRead(saveGame->m_fileOffset, g_saveFileBlock, c_saveFileBlockSize, &bytesRead);
		if (result) {
			return result;
		}

		if (bytesRead != c_saveFileBlockSize ||
			g_saveFileBlock[c_saveFileBlockDataSize] !=
				static_cast<LegoU8>(CalculateBlockChecksum(g_saveFileBlock, c_saveFileBlockDataSize))) {
			return c_saveFileInvalidStatus;
		}

		LegoU32 chunkSize = end - p_dest;
		if (chunkSize >= c_saveFileBlockDataSize) {
			chunkSize = c_saveFileBlockDataSize;
		}

		::memcpy(p_dest, g_saveFileBlock, chunkSize);
		fileOffset = saveGame->m_fileOffset;
		fileOffset += c_saveFileBlockSize;
		saveGame->m_fileOffset = fileOffset;
		p_dest += chunkSize;
	}

	return GolStream::e_ioSuccess;
}

// FUNCTION: LEGORACERS 0x004429d0
LegoU32 SaveGame::CalculateBlockChecksum(const LegoU8* p_source, LegoU32 p_size)
{
	LegoU32 result = 0;

	for (LegoU32 i = 0; i < p_size; i++) {
		result += p_source[i];
	}

	return result;
}

#ifdef COMPAT_MODE

void SaveGame::ReadPersistentGameState(PersistentGameState* p_state)
{
	LegoU8* dest;
	const LegoU8* source;
	LegoU32 i;
	LegoU32 j;

	dest = &p_state->m_racerCount;
	dest[0] = m_fileImage[c_persistentHeaderOffset];
	source = &m_fileImage[c_persistentHeaderOffset + 1];
	for (i = 0; i < sizeof(p_state->m_displayDriverGuid.m_bytes); i++) {
		dest[i + 1] = source[i];
	}

	dest[0x11] = m_fileImage[c_persistentHeaderOffset + 0x11];
	dest[0x12] = m_fileImage[c_persistentHeaderOffset + 0x12];
	dest[0x13] = m_fileImage[c_persistentHeaderOffset + 0x13];
	dest[0x14] = m_fileImage[c_persistentHeaderOffset + 0x14];
	dest[0x15] = m_fileImage[c_persistentHeaderOffset + 0x15];
	dest[0x16] = m_fileImage[c_persistentHeaderOffset + 0x16];
	dest[0x17] = m_fileImage[c_persistentHeaderOffset + 0x17];

	source = &m_fileImage[c_inputBindingHeaderOffset + 1];
	dest = &p_state->m_inputBindings.m_players[0].m_selectedRecordSource;
	for (i = 0; i < sizeOfArray(p_state->m_inputBindings.m_players); i++) {
		dest[-1] = source[-1];
		dest[0] = source[0];
		dest[1] = source[1];
		dest[2] = source[2];

		source += sizeof(InputBindingState::PlayerState);
		dest += sizeof(InputBindingState::PlayerState);
	}

	source = &m_fileImage[c_inputBindingEntryOffset + 2];
	for (i = 0; i < sizeOfArray(p_state->m_inputBindings.m_entries); i++) {
		InputBindingState::Entry* entry = &p_state->m_inputBindings.m_entries[i];
		entry->m_deviceType = source[-2];
		entry->m_deviceSubType = source[-1];
		entry->m_deviceId = source[0];

		for (j = 0; j < sizeOfArray(entry->m_events); j++) {
			entry->m_events[j] = ReadLittleEndianU32(&source[1 + (j * sizeof(LegoU32))]);
		}

		source += c_inputBindingEntrySize;
	}

	p_state->m_partUnlockFlags = m_fileImage[c_unlockStateOffset];
	p_state->m_unlockedCircuits = m_fileImage[c_unlockStateOffset + 1];
	p_state->m_unlockedRaces = ReadLittleEndianU16(&m_fileImage[c_unlockStateOffset + 2]);

	source = &m_fileImage[c_scoreRecordsOffset + c_scoreRecordCursorOffset];
	for (i = 0; i < c_scoreRecordCount; i++) {
		LegoU32 time = source[c_scoreRecordLapTimeOffset + 3 - c_scoreRecordCursorOffset];
		time <<= 8;
		time += source[c_scoreRecordLapTimeOffset + 2 - c_scoreRecordCursorOffset];
		time <<= 8;
		time += source[c_scoreRecordLapTimeOffset + 1 - c_scoreRecordCursorOffset];
		time <<= 8;
		time += source[c_scoreRecordLapTimeOffset - c_scoreRecordCursorOffset];
		p_state->m_bestLapTimes[i] = time;

		for (j = 0; j < c_scoreHolderNameSize; j++) {
			p_state->m_bestLapHolderNames[i][j] =
				source[c_scoreRecordLapHolderNameOffset - c_scoreRecordCursorOffset + j];
		}

		time = source[c_scoreRecordRaceTimeOffset + 3 - c_scoreRecordCursorOffset];
		time <<= 8;
		time += source[c_scoreRecordRaceTimeOffset + 2 - c_scoreRecordCursorOffset];
		time <<= 8;
		time += source[c_scoreRecordRaceTimeOffset + 1 - c_scoreRecordCursorOffset];
		time <<= 8;
		time += source[c_scoreRecordRaceTimeOffset - c_scoreRecordCursorOffset];
		p_state->m_bestRaceTimes[i] = time;

		for (j = 0; j < c_scoreHolderNameSize; j++) {
			p_state->m_bestRaceHolderNames[i][j] =
				source[c_scoreRecordRaceHolderNameOffset - c_scoreRecordCursorOffset + j];
		}

		source += c_scoreRecordSize;
	}
}

#else

// FUNCTION: LEGORACERS 0x00442a00
__declspec(naked) void SaveGame::ReadPersistentGameState(PersistentGameState* p_state)
{
	__asm {
		mov edx, dword ptr [esp + 4]
		sub esp, 0x1c
		push ebp
		push esi
		mov esi, ecx
		push edi
		mov al, byte ptr [esi + 0x36d]
		mov byte ptr [edx], al
		xor eax, eax
	L_ReadPersistentGameState_0:
		mov cl, byte ptr [esi + eax + 0x36e]
		mov byte ptr [edx + eax + 1], cl
		inc eax
		cmp eax, 0x10
		jb L_ReadPersistentGameState_0
		mov al, byte ptr [esi + 0x37e]
		lea edi, [edx + 0x35d]
		mov byte ptr [edx + 0x11], al
		mov cl, byte ptr [esi + 0x37f]
		mov byte ptr [edx + 0x12], cl
		mov al, byte ptr [esi + 0x380]
		mov byte ptr [edx + 0x13], al
		mov cl, byte ptr [esi + 0x381]
		mov byte ptr [edx + 0x14], cl
		mov al, byte ptr [esi + 0x382]
		mov byte ptr [edx + 0x15], al
		mov cl, byte ptr [esi + 0x383]
		mov byte ptr [edx + 0x16], cl
		mov al, byte ptr [esi + 0x384]
		mov byte ptr [edx + 0x17], al
		lea eax, [esi + 0x386]
		mov ebp, 2
	L_ReadPersistentGameState_1:
		mov cl, byte ptr [eax - 1]
		add eax, 4
		mov byte ptr [edi - 1], cl
		mov cl, byte ptr [eax - 4]
		mov byte ptr [edi], cl
		mov cl, byte ptr [eax - 3]
		mov byte ptr [edi + 1], cl
		mov cl, byte ptr [eax - 2]
		mov byte ptr [edi + 2], cl
		add edi, 4
		dec ebp
		jne L_ReadPersistentGameState_1
		lea eax, [esi + 0x25]
		lea ebp, [esi + 0x24]
		lea ecx, [esi + 0x26]
		add edx, 0x365
		sub ebp, eax
		sub eax, ecx
		push ebx
		lea edi, [ecx + 0x369]
		mov dword ptr [esp + 0x20], ebp
		mov dword ptr [esp + 0x10], eax
		mov dword ptr [esp + 0x1c], 5
		jmp L_ReadPersistentGameState_2
	L_ReadPersistentGameState_5:
		mov ebp, dword ptr [esp + 0x20]
		mov eax, dword ptr [esp + 0x10]
	L_ReadPersistentGameState_2:
		add eax, edi
		lea ebx, [esi + 0x24]
		mov dword ptr [esp + 0x18], 9
		mov cl, byte ptr [eax + ebp]
		lea ebp, [esi + 0x27]
		mov byte ptr [edx - 1], cl
		mov al, byte ptr [eax]
		mov byte ptr [edx], al
		mov cl, byte ptr [edi]
		mov byte ptr [edx + 1], cl
		lea ecx, [esi + 0x26]
		lea eax, [edx + 3]
		sub ebp, ecx
		sub ebx, ecx
		mov dword ptr [esp + 0x14], eax
		lea eax, [edi + 3]
		mov dword ptr [esp + 0x24], ebp
		mov dword ptr [esp + 0x28], ebx
		jmp L_ReadPersistentGameState_3
	L_ReadPersistentGameState_4:
		mov ebp, dword ptr [esp + 0x24]
	L_ReadPersistentGameState_3:
		xor ecx, ecx
		mov cl, byte ptr [eax + ebp]
		mov ebp, dword ptr [esp + 0x10]
		mov ebx, ecx
		xor ecx, ecx
		mov cl, byte ptr [eax]
		add eax, 4
		shl ebx, 8
		add ebx, ecx
		xor ecx, ecx
		mov cl, byte ptr [eax + ebp - 4]
		mov ebp, dword ptr [esp + 0x28]
		shl ebx, 8
		add ebx, ecx
		xor ecx, ecx
		mov cl, byte ptr [eax + ebp - 4]
		shl ebx, 8
		add ebx, ecx
		mov ecx, dword ptr [esp + 0x14]
		mov dword ptr [ecx], ebx
		add ecx, 4
		mov dword ptr [esp + 0x14], ecx
		mov ecx, dword ptr [esp + 0x18]
		dec ecx
		mov dword ptr [esp + 0x18], ecx
		jne L_ReadPersistentGameState_4
		mov eax, dword ptr [esp + 0x1c]
		add edi, 0x27
		add edx, 0x28
		dec eax
		mov dword ptr [esp + 0x1c], eax
		jne L_ReadPersistentGameState_5
		mov eax, dword ptr [esp + 0x30]
		mov dl, byte ptr [esi + 0x29]
		pop ebx
		mov byte ptr [eax + 0x18], dl
		mov cl, byte ptr [esi + 0x2a]
		mov byte ptr [eax + 0x19], cl
		lea edi, [eax + 0x1f0]
		movzx dx, byte ptr [esi + 0x2c]
		movzx cx, byte ptr [esi + 0x2b]
		shl edx, 8
		add edx, ecx
		lea ebp, [eax + 0x50]
		mov word ptr [eax + 0x1a], dx
		lea eax, [esi + 0x2f]
		mov esi, 0xd
	L_ReadPersistentGameState_8:
		xor edx, edx
		xor ecx, ecx
		mov dl, byte ptr [eax + 1]
		mov cl, byte ptr [eax]
		shl edx, 8
		add edx, ecx
		xor ecx, ecx
		mov cl, byte ptr [eax - 1]
		shl edx, 8
		add edx, ecx
		xor ecx, ecx
		mov cl, byte ptr [eax - 2]
		shl edx, 8
		add edx, ecx
		xor ecx, ecx
		mov dword ptr [ebp - 0x34], edx
	L_ReadPersistentGameState_6:
		mov dl, byte ptr [eax + ecx + 2]
		mov byte ptr [edi + ecx - 0x16c], dl
		inc ecx
		cmp ecx, 0x1c
		jb L_ReadPersistentGameState_6
		xor ecx, ecx
		xor edx, edx
		mov cl, byte ptr [eax + 0x21]
		mov dl, byte ptr [eax + 0x20]
		shl ecx, 8
		add ecx, edx
		xor edx, edx
		mov dl, byte ptr [eax + 0x1f]
		shl ecx, 8
		add ecx, edx
		xor edx, edx
		mov dl, byte ptr [eax + 0x1e]
		shl ecx, 8
		add ecx, edx
		mov dword ptr [ebp], ecx
		xor ecx, ecx
	L_ReadPersistentGameState_7:
		mov dl, byte ptr [eax + ecx + 0x22]
		mov byte ptr [edi + ecx], dl
		inc ecx
		cmp ecx, 0x1c
		jb L_ReadPersistentGameState_7
		add ebp, 4
		add eax, 0x40
		add edi, 0x1c
		dec esi
		jne L_ReadPersistentGameState_8
		pop edi
		pop esi
		pop ebp
		add esp, 0x1c
		ret 4
	}
}

#endif

#ifdef COMPAT_MODE

void SaveGame::WritePersistentGameState(PersistentGameState* p_state)
{
	LegoU8* dest;
	LegoU32 i;
	LegoU32 j;

	m_fileImage[c_persistentHeaderOffset] = p_state->m_racerCount;
	for (i = 0; i < sizeof(p_state->m_displayDriverGuid.m_bytes); i++) {
		m_fileImage[c_persistentHeaderOffset + 1 + i] = p_state->m_displayDriverGuid.m_bytes[i];
	}
	m_fileImage[c_persistentHeaderOffset + 0x11] = p_state->m_unk0x1d;
	m_fileImage[c_persistentHeaderOffset + 0x12] = p_state->m_unk0x1e;
	m_fileImage[c_persistentHeaderOffset + 0x13] = p_state->m_musicVolume;
	m_fileImage[c_persistentHeaderOffset + 0x14] = p_state->m_soundVolume;
	m_fileImage[c_persistentHeaderOffset + 0x15] = p_state->m_stereo;
	m_fileImage[c_persistentHeaderOffset + 0x16] = p_state->m_languageIndex;
	m_fileImage[c_persistentHeaderOffset + 0x17] = p_state->m_lapCount;

	dest = &m_fileImage[c_inputBindingHeaderOffset + 1];
	for (i = 0; i < sizeOfArray(p_state->m_inputBindings.m_players); i++) {
		InputBindingState::PlayerState* player = &p_state->m_inputBindings.m_players[i];
		dest[-1] = player->m_selectedRecordId;
		dest[0] = player->m_selectedRecordSource;
		dest[1] = player->m_selectedSaveIndex;
		dest[2] = player->m_selectedEntryIndex;
		dest += sizeof(InputBindingState::PlayerState);
	}

	dest = &m_fileImage[c_inputBindingEntryOffset];
	for (i = 0; i < sizeOfArray(p_state->m_inputBindings.m_entries); i++) {
		InputBindingState::Entry* entry = &p_state->m_inputBindings.m_entries[i];
		dest[0] = entry->m_deviceType;
		dest[1] = entry->m_deviceSubType;
		dest[2] = entry->m_deviceId;

		for (j = 0; j < sizeOfArray(entry->m_events); j++) {
			WriteLittleEndianU32(&dest[3 + (j * sizeof(LegoU32))], entry->m_events[j]);
		}

		dest += c_inputBindingEntrySize;
	}

	m_fileImage[c_unlockStateOffset] = p_state->m_partUnlockFlags;
	m_fileImage[c_unlockStateOffset + 1] = p_state->m_unlockedCircuits;
	m_fileImage[c_unlockStateOffset + 2] = static_cast<LegoU8>(p_state->m_unlockedRaces);
	m_fileImage[c_unlockStateOffset + 3] = static_cast<LegoU8>(p_state->m_unlockedRaces >> 8);

	dest = &m_fileImage[c_scoreRecordsOffset + c_scoreRecordCursorOffset];
	for (i = 0; i < c_scoreRecordCount; i++) {
		dest[c_scoreRecordLapTimeOffset - c_scoreRecordCursorOffset] = static_cast<LegoU8>(p_state->m_bestLapTimes[i]);
		dest[c_scoreRecordLapTimeOffset + 1 - c_scoreRecordCursorOffset] =
			static_cast<LegoU8>(p_state->m_bestLapTimes[i] >> 8);
		dest[c_scoreRecordLapTimeOffset + 2 - c_scoreRecordCursorOffset] =
			static_cast<LegoU8>(p_state->m_bestLapTimes[i] >> 16);
		dest[c_scoreRecordLapTimeOffset + 3 - c_scoreRecordCursorOffset] =
			static_cast<LegoU8>(p_state->m_bestLapTimes[i] >> 24);

		for (j = 0; j < c_scoreHolderNameSize; j++) {
			dest[c_scoreRecordLapHolderNameOffset - c_scoreRecordCursorOffset + j] =
				p_state->m_bestLapHolderNames[i][j];
		}

		dest[c_scoreRecordRaceTimeOffset - c_scoreRecordCursorOffset] =
			static_cast<LegoU8>(p_state->m_bestRaceTimes[i]);
		dest[c_scoreRecordRaceTimeOffset + 1 - c_scoreRecordCursorOffset] =
			static_cast<LegoU8>(p_state->m_bestRaceTimes[i] >> 8);
		dest[c_scoreRecordRaceTimeOffset + 2 - c_scoreRecordCursorOffset] =
			static_cast<LegoU8>(p_state->m_bestRaceTimes[i] >> 16);
		dest[c_scoreRecordRaceTimeOffset + 3 - c_scoreRecordCursorOffset] =
			static_cast<LegoU8>(p_state->m_bestRaceTimes[i] >> 24);

		for (j = 0; j < c_scoreHolderNameSize; j++) {
			dest[c_scoreRecordRaceHolderNameOffset - c_scoreRecordCursorOffset + j] =
				p_state->m_bestRaceHolderNames[i][j];
		}

		dest += c_scoreRecordSize;
	}
}

#else

// FUNCTION: LEGORACERS 0x00442c20
__declspec(naked) void SaveGame::WritePersistentGameState(PersistentGameState* p_state)
{
	__asm {
		sub esp, 0x18
		push ebp
		push esi
		mov esi, dword ptr [esp + 0x24]
		push edi
		mov edi, ecx
		mov al, byte ptr [esi]
		mov byte ptr [edi + 0x36d], al
		xor eax, eax
	L_WritePersistentGameState_0:
		mov cl, byte ptr [esi + eax + 1]
		mov byte ptr [edi + eax + 0x36e], cl
		inc eax
		cmp eax, 0x10
		jb L_WritePersistentGameState_0
		mov dl, byte ptr [esi + 0x11]
		mov ebp, 2
		mov byte ptr [edi + 0x37e], dl
		mov al, byte ptr [esi + 0x12]
		mov byte ptr [edi + 0x37f], al
		mov cl, byte ptr [esi + 0x13]
		mov byte ptr [edi + 0x380], cl
		mov dl, byte ptr [esi + 0x14]
		mov byte ptr [edi + 0x381], dl
		mov al, byte ptr [esi + 0x15]
		mov byte ptr [edi + 0x382], al
		mov cl, byte ptr [esi + 0x16]
		mov byte ptr [edi + 0x383], cl
		mov dl, byte ptr [esi + 0x17]
		mov byte ptr [edi + 0x384], dl
		lea eax, [edi + 0x386]
		lea ecx, [esi + 0x35d]
	L_WritePersistentGameState_1:
		mov dl, byte ptr [ecx - 1]
		add ecx, 4
		mov byte ptr [eax - 1], dl
		mov dl, byte ptr [ecx - 4]
		mov byte ptr [eax], dl
		mov dl, byte ptr [ecx - 3]
		mov byte ptr [eax + 1], dl
		mov dl, byte ptr [ecx - 2]
		mov byte ptr [eax + 2], dl
		add eax, 4
		dec ebp
		jne L_WritePersistentGameState_1
		lea ecx, [edi + 0x24]
		lea eax, [edi + 0x26]
		lea edx, [edi + 0x25]
		add esi, 0x365
		sub ecx, eax
		sub eax, edx
		push ebx
		lea ebp, [edx + 0x369]
		mov dword ptr [esp + 0x1c], ecx
		mov dword ptr [esp + 0x10], eax
		mov dword ptr [esp + 0x18], 5
		jmp L_WritePersistentGameState_2
	L_WritePersistentGameState_4:
		mov eax, dword ptr [esp + 0x10]
		mov ecx, dword ptr [esp + 0x1c]
	L_WritePersistentGameState_2:
		mov dl, byte ptr [esi - 1]
		add eax, ebp
		lea ebx, [edi + 0x24]
		mov dword ptr [esp + 0x14], 9
		mov byte ptr [eax + ecx], dl
		mov cl, byte ptr [esi]
		mov byte ptr [ebp], cl
		mov dl, byte ptr [esi + 1]
		lea ecx, [edi + 0x25]
		mov byte ptr [eax], dl
		sub ebx, ecx
		lea edx, [esi + 3]
		mov dword ptr [esp + 0x20], ebx
		lea ebx, [edi + 0x27]
		sub ebx, ecx
		lea eax, [ebp + 3]
		mov dword ptr [esp + 0x24], ebx
	L_WritePersistentGameState_3:
		mov ebx, dword ptr [esp + 0x20]
		mov cl, byte ptr [edx]
		add edx, 4
		mov byte ptr [ebx + eax], cl
		mov ecx, dword ptr [edx - 4]
		mov ebx, dword ptr [esp + 0x10]
		add eax, 4
		shr ecx, 8
		mov byte ptr [eax - 4], cl
		mov ecx, dword ptr [edx - 4]
		shr ecx, 0x10
		mov byte ptr [ebx + eax - 4], cl
		mov ecx, dword ptr [edx - 4]
		mov ebx, dword ptr [esp + 0x24]
		shr ecx, 0x18
		mov byte ptr [ebx + eax - 4], cl
		mov ecx, dword ptr [esp + 0x14]
		dec ecx
		mov dword ptr [esp + 0x14], ecx
		jne L_WritePersistentGameState_3
		mov eax, dword ptr [esp + 0x18]
		add ebp, 0x27
		add esi, 0x28
		dec eax
		mov dword ptr [esp + 0x18], eax
		jne L_WritePersistentGameState_4
		mov eax, dword ptr [esp + 0x2c]
		pop ebx
		mov dl, byte ptr [eax + 0x18]
		lea ebp, [eax + 0x1f0]
		mov byte ptr [edi + 0x29], dl
		mov cl, byte ptr [eax + 0x19]
		mov byte ptr [edi + 0x2a], cl
		mov dl, byte ptr [eax + 0x1a]
		xor ecx, ecx
		mov byte ptr [edi + 0x2b], dl
		mov cl, byte ptr [eax + 0x1b]
		lea esi, [eax + 0x50]
		mov byte ptr [edi + 0x2c], cl
		lea eax, [edi + 0x2f]
		mov edi, 0xd
	L_WritePersistentGameState_7:
		mov dl, byte ptr [esi - 0x34]
		mov byte ptr [eax - 2], dl
		mov ecx, dword ptr [esi - 0x34]
		shr ecx, 8
		mov byte ptr [eax - 1], cl
		mov edx, dword ptr [esi - 0x34]
		shr edx, 0x10
		mov byte ptr [eax], dl
		mov ecx, dword ptr [esi - 0x34]
		shr ecx, 0x18
		mov byte ptr [eax + 1], cl
		xor ecx, ecx
	L_WritePersistentGameState_5:
		mov dl, byte ptr [ecx + ebp - 0x16c]
		mov byte ptr [eax + ecx + 2], dl
		inc ecx
		cmp ecx, 0x1c
		jb L_WritePersistentGameState_5
		mov cl, byte ptr [esi]
		mov byte ptr [eax + 0x1e], cl
		mov edx, dword ptr [esi]
		shr edx, 8
		mov byte ptr [eax + 0x1f], dl
		mov ecx, dword ptr [esi]
		shr ecx, 0x10
		mov byte ptr [eax + 0x20], cl
		mov edx, dword ptr [esi]
		shr edx, 0x18
		mov byte ptr [eax + 0x21], dl
		xor ecx, ecx
	L_WritePersistentGameState_6:
		mov dl, byte ptr [ecx + ebp]
		mov byte ptr [eax + ecx + 0x22], dl
		inc ecx
		cmp ecx, 0x1c
		jb L_WritePersistentGameState_6
		add esi, 4
		add eax, 0x40
		add ebp, 0x1c
		dec edi
		jne L_WritePersistentGameState_7
		pop edi
		pop esi
		pop ebp
		add esp, 0x18
		ret 4
	}
}

#endif

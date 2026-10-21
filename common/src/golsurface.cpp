#include "golsurface.h"

#include "device/goldirectdrawpalette.h"

DECOMP_SIZE_ASSERT(GolSurface, 0x30)

// FUNCTION: GOLDP 0x1001cf00
// FUNCTION: LEGORACERS 0x00408490
GolSurface::GolSurface()
{
	m_pixels = NULL;
	m_auxPixels = NULL;
	m_pitch = 0;
	m_pixelFlags = 0;
	m_auxFlags = 0;
	m_width = 0;
	m_height = 0;
}

// FUNCTION: GOLDP 0x1001cf60
// FUNCTION: LEGORACERS 0x004084f0
GolSurface::~GolSurface()
{
}

// FUNCTION: GOLDP 0x1001cf70
// FUNCTION: LEGORACERS 0x00408500
void GolSurface::LockPixels(LegoU8** p_pixels, LegoU32* p_pitch, LegoU32 p_flags)
{
	*p_pitch = m_pitch;
	*p_pixels = m_pixels;

	m_pixelFlags |= c_lockFlagLocked;
	if (p_flags & c_lockRequestRead) {
		m_pixelFlags |= c_lockFlagRead;
	}
	if (p_flags & c_lockRequestWrite) {
		m_pixelFlags |= c_lockFlagWrite;
	}
}

// FUNCTION: GOLDP 0x1001cfb0
// FUNCTION: LEGORACERS 0x00408540
void GolSurface::UnlockPixels()
{
	LegoU16 flags = m_pixelFlags;
	if (((LegoU8) flags) & c_lockFlagLocked) {
		flags &= ~c_lockStateMask;
		m_pixelFlags = flags;
	}
}

// FUNCTION: GOLDP 0x1001cfd0
// FUNCTION: LEGORACERS 0x00408560
void GolSurface::LockAuxPixels(LegoU8** p_pixels, LegoU32* p_pitch, LegoU32 p_flags)
{
	*p_pitch = m_pitch;
	*p_pixels = m_auxPixels;

	m_auxFlags |= c_lockFlagLocked;
	if (p_flags & c_lockRequestRead) {
		m_auxFlags |= c_lockFlagRead;
	}
	if (p_flags & c_lockRequestWrite) {
		m_auxFlags |= c_lockFlagWrite;
	}
}

// FUNCTION: GOLDP 0x1001d010
// FUNCTION: LEGORACERS 0x004085a0
void GolSurface::UnlockAuxPixels()
{
	if (m_auxFlags & c_lockFlagLocked) {
		m_auxFlags &= ~c_lockStateMask;
	}
}

// FUNCTION: GOLDP 0x1002c020 FOLDED
// FUNCTION: LEGORACERS 0x004513d0 FOLDED
void GolSurface::Present(undefined4)
{
	// empty
}

// FUNCTION: GOLDP 0x10029920 FOLDED
// FUNCTION: LEGORACERS 0x004164c0 FOLDED
void GolSurface::FinishPendingFlip()
{
	// empty
}

// FUNCTION: GOLDP 0x1001d030
// FUNCTION: LEGORACERS 0x004085c0
void GolSurface::Fill(LegoU32 p_color)
{
	LegoU8* pixels;
	LegoU32 pitch;
	LegoBool32 unlockPixels;

	if (m_pixelFlags & c_lockFlagLocked) {
		pixels = m_pixels;
		pitch = m_pitch;
		unlockPixels = FALSE;
	}
	else {
		LockPixels(&pixels, &pitch, c_lockRequestWrite);
		unlockPixels = TRUE;
	}

	LegoU32 height = m_height;
	LegoU32 width = m_width;

	switch (m_textureFormat.m_bitsPerPixel) {
	case 4:
		p_color &= 0x0f;
		width >>= 1;
		p_color |= p_color << 4;
		// fall through
	case 8:
		if (height > 0) {
			LegoU32 rows = height;
			do {
				LegoU8* rowEnd = pixels + width;
				LegoU8* row = pixels;
				while (row < rowEnd) {
					*row++ = (LegoU8) p_color;
				}
				pixels += pitch;
			} while (--rows);
		}
		break;
	case 16:
		if (height > 0) {
			LegoU32 rows = height;
			do {
				LegoU16* row = (LegoU16*) pixels;
				LegoU16* rowEnd = row + width;
				while (row < rowEnd) {
					*row++ = (LegoU16) p_color;
				}
				pixels += pitch;
			} while (--rows);
		}
		break;
	case 24: {
		LegoU8 color0 = p_color;
		LegoU8 color1 = p_color >> 8;
		LegoU8 color2 = p_color >> 16;
		width *= 3;
		if (height > 0) {
			LegoU32 rows = height;
			do {
				LegoU8* rowEnd = pixels + width;
				LegoU8* row = pixels;
				while (row < rowEnd) {
					*row++ = color0;
					*row++ = color1;
					*row++ = color2;
				}
				pixels += pitch;
			} while (--rows);
		}
		break;
	}
	case 32:
		if (height > 0) {
			LegoU32 rows = height;
			do {
				LegoU32* row = (LegoU32*) pixels;
				LegoU32* rowEnd = row + width;
				while (row < rowEnd) {
					*row++ = p_color;
				}
				pixels += pitch;
			} while (--rows);
		}
		break;
	}

	if (unlockPixels) {
		UnlockPixels();
	}
}

#ifdef COMPAT_MODE

void GolSurface::Blit(LegoU32 p_x, LegoU32 p_y, GolSurface* p_source, Rect* p_sourceRect)
{
	LegoU8* destPixels;
	LegoU32 destPitch;
	LegoU32 copyWidth;
	LegoU32 sourcePitch;
	GolSurface* destSurface = this;
	LegoU32 copyHeight;
	LegoBool32 unlockSource;
	LegoBool32 unlockDest;
	LegoU8* sourcePixels;

	if (destSurface->m_textureFormat.m_bitsPerPixel != p_source->m_textureFormat.m_bitsPerPixel) {
		return;
	}

	if (p_sourceRect->m_right > p_source->m_width) {
		p_sourceRect->m_right = p_source->m_width;
	}
	if (p_sourceRect->m_bottom > p_source->m_height) {
		p_sourceRect->m_bottom = p_source->m_height;
	}
	if (p_sourceRect->m_left >= p_sourceRect->m_right) {
		return;
	}
	if (p_sourceRect->m_top >= p_sourceRect->m_bottom) {
		return;
	}

	copyHeight = p_sourceRect->m_bottom - p_sourceRect->m_top;
	LegoU32 destWidth = destSurface->m_width;
	copyWidth = p_sourceRect->m_right - p_sourceRect->m_left;

	if (p_x >= destWidth) {
		return;
	}

	LegoU32 destHeight = destSurface->m_height;
	if (p_y >= destHeight) {
		return;
	}
	if (p_x + copyWidth > destWidth) {
		copyWidth = destWidth - p_x;
	}
	if (p_y + copyHeight > destHeight) {
		copyHeight = destHeight - p_y;
	}

	if (destSurface->m_textureFormat.m_paletteMask) {
		GolPaletteBase* palette = destSurface->GetPalette();
		palette->CopyEntriesFrom(p_source->GetPalette());
	}

	if (destSurface->m_pixelFlags & c_lockFlagLocked) {
		destPixels = destSurface->m_pixels;
		unlockDest = FALSE;
		destPitch = destSurface->m_pitch;
	}
	else {
		destSurface->LockPixels(&destPixels, &destPitch, c_lockRequestWrite);
		unlockDest = TRUE;
	}

	if (p_source->m_pixelFlags & c_lockFlagLocked) {
		sourcePitch = p_source->m_pitch;
		sourcePixels = p_source->m_pixels;
		unlockSource = FALSE;
	}
	else {
		p_source->LockPixels(&sourcePixels, &sourcePitch, c_lockRequestRead);
		unlockSource = TRUE;
	}

	if (destSurface->m_textureFormat.m_bitsPerPixel == 4) {
		sourcePixels += sourcePitch * p_sourceRect->m_top;
		destPixels += p_y * destPitch;

		LegoU32 destMask;
		LegoU32 destShift;
		if (p_x & 1) {
			destMask = 0x0f;
			destShift = 4;
		}
		else {
			destMask = 0xf0;
			destShift = 0;
		}

		LegoU32 sourceMask;
		LegoU32 sourceShift;
		if (p_sourceRect->m_left & 1) {
			sourceMask = 0xf0;
			sourceShift = 4;
		}
		else {
			sourceMask = 0x0f;
			sourceShift = 0;
		}

		if (copyHeight > 0) {
			LegoU32 rows = copyHeight;
			do {
				LegoU32 rowDestMask = destMask;
				LegoU32 rowSourceMask = sourceMask;
				LegoU32 rowSourceShift = sourceShift;
				LegoU32 x = 0;
				LegoU32 rowDestShift = destShift;
				while (x < copyWidth) {
					LegoU8* dest = &destPixels[(x + p_x) >> 1];
					LegoU32 color = (sourcePixels[(x + p_sourceRect->m_left) >> 1] & rowSourceMask) >> rowSourceShift;
					color <<= rowDestShift;
					rowDestShift ^= 4;
					rowSourceMask = ~rowSourceMask;
					*dest = (*dest & rowDestMask) | (LegoU8) color;
					rowDestMask = ~rowDestMask;
					++x;
					rowSourceShift ^= 4;
				}
				destPixels += destPitch;
				sourcePixels += sourcePitch;
			} while (--rows);
		}
	}
	else {
		LegoU32 bytesPerPixel = destSurface->m_textureFormat.m_bitsPerPixel >> 3;
		sourcePixels += (sourcePitch * p_sourceRect->m_top) + (bytesPerPixel * p_sourceRect->m_left);
		destPixels += (p_y * destPitch) + (p_x * bytesPerPixel);

		LegoU32 rowBytes = copyWidth * bytesPerPixel;
		for (LegoU32 rows = copyHeight; rows; --rows) {
			LegoU8* source = sourcePixels;
			LegoU8* destEnd = destPixels + rowBytes;
			LegoU8* dest = destPixels;
			while (dest < destEnd) {
				*dest++ = *source++;
			}
			destPixels += destPitch;
			sourcePixels += sourcePitch;
		}
	}

	if (unlockSource) {
		p_source->UnlockPixels();
	}
	if (unlockDest) {
		destSurface->UnlockPixels();
	}
}

#else

// FUNCTION: GOLDP 0x1001d240
// FUNCTION: LEGORACERS 0x004087d0
__declspec(naked) void GolSurface::Blit(LegoU32 p_x, LegoU32 p_y, GolSurface* p_source, Rect* p_sourceRect)
{
	__asm {
		sub esp, 0x38
		push ebx
		push ebp
		push esi
		push edi
		mov edi, ecx
		mov ecx, dword ptr [esp + 0x54]
		mov dword ptr [esp + 0x20], edi
		mov ax, word ptr [edi + 0x1a]
		cmp ax, word ptr [ecx + 0x1a]
		jne L_Blit_0
		mov ebp, dword ptr [esp + 0x58]
		xor eax, eax
		mov ax, word ptr [ecx + 0x2a]
		cmp dword ptr [ebp + 8], eax
		jle L_Blit_1
		mov dword ptr [ebp + 8], eax
	L_Blit_1:
		xor eax, eax
		mov ax, word ptr [ecx + 0x2c]
		mov ecx, dword ptr [ebp + 0xc]
		cmp ecx, eax
		jle L_Blit_2
		mov dword ptr [ebp + 0xc], eax
	L_Blit_2:
		mov esi, dword ptr [ebp]
		mov ecx, dword ptr [ebp + 8]
		cmp esi, ecx
		jge L_Blit_0
		mov edx, dword ptr [ebp + 4]
		mov eax, dword ptr [ebp + 0xc]
		cmp edx, eax
		jge L_Blit_0
		mov ebx, dword ptr [esp + 0x4c]
		sub eax, edx
		mov edx, eax
		xor eax, eax
		mov ax, word ptr [edi + 0x2a]
		sub ecx, esi
		mov esi, ecx
		cmp ebx, eax
		mov dword ptr [esp + 0x18], esi
		mov dword ptr [esp + 0x24], edx
		jae L_Blit_0
		xor ecx, ecx
		mov cx, word ptr [edi + 0x2c]
		mov edi, dword ptr [esp + 0x50]
		cmp edi, ecx
		jae L_Blit_0
		add esi, ebx
		cmp esi, eax
		jbe L_Blit_3
		sub eax, ebx
		mov dword ptr [esp + 0x18], eax
	L_Blit_3:
		add edx, edi
		cmp edx, ecx
		jbe L_Blit_4
		sub ecx, edi
		mov dword ptr [esp + 0x24], ecx
	L_Blit_4:
		mov ecx, dword ptr [esp + 0x20]
		cmp word ptr [ecx + 0x18], 0
		je L_Blit_5
		mov edx, dword ptr [ecx]
		call dword ptr [edx + 0x1c]
		mov ecx, dword ptr [esp + 0x54]
		mov esi, dword ptr [eax]
		mov dword ptr [esp + 0x44], eax
		mov eax, dword ptr [ecx]
		call dword ptr [eax + 0x1c]
		mov ecx, dword ptr [esp + 0x44]
		push eax
		call dword ptr [esi + 4]
		mov ecx, dword ptr [esp + 0x20]
	L_Blit_5:
		test byte ptr [ecx + 0x26], 2
		je L_Blit_6
		mov edx, dword ptr [ecx + 0x1c]
		mov dword ptr [esp + 0x44], 0
		mov dword ptr [esp + 0x10], edx
		xor edx, edx
		mov dx, word ptr [ecx + 0x24]
		mov dword ptr [esp + 0x14], edx
		jmp L_Blit_7
	L_Blit_6:
		mov eax, dword ptr [ecx]
		lea edx, [esp + 0x14]
		push 2
		push edx
		lea edx, [esp + 0x18]
		push edx
		call dword ptr [eax + 4]
		mov edx, dword ptr [esp + 0x14]
		mov dword ptr [esp + 0x44], 1
	L_Blit_7:
		mov ecx, dword ptr [esp + 0x54]
		test byte ptr [ecx + 0x26], 2
		je L_Blit_8
		mov eax, dword ptr [ecx + 0x1c]
		xor esi, esi
		mov si, word ptr [ecx + 0x24]
		mov dword ptr [esp + 0x50], eax
		mov dword ptr [esp + 0x1c], esi
		mov dword ptr [esp + 0x40], 0
		jmp L_Blit_9
	L_Blit_8:
		mov edx, dword ptr [ecx]
		lea eax, [esp + 0x1c]
		push 1
		push eax
		lea eax, [esp + 0x58]
		push eax
		call dword ptr [edx + 4]
		mov esi, dword ptr [esp + 0x1c]
		mov edx, dword ptr [esp + 0x14]
		mov ecx, dword ptr [esp + 0x54]
		mov dword ptr [esp + 0x40], 1
	L_Blit_9:
		mov eax, dword ptr [esp + 0x20]
		mov ax, word ptr [eax + 0x1a]
		cmp ax, 4
		jne L_Blit_10
		mov eax, dword ptr [ebp + 4]
		imul edx, edi
		imul eax, esi
		mov esi, dword ptr [esp + 0x50]
		mov edi, dword ptr [esp + 0x10]
		add esi, eax
		add edi, edx
		test bl, 1
		mov dword ptr [esp + 0x50], esi
		mov dword ptr [esp + 0x10], edi
		mov eax, 0xf
		mov edx, 0xf0
		jne L_Blit_11
		mov dword ptr [esp + 0x28], edx
		mov dword ptr [esp + 0x2c], 0
		jmp L_Blit_12
	L_Blit_11:
		mov dword ptr [esp + 0x28], eax
		mov dword ptr [esp + 0x2c], 4
	L_Blit_12:
		test byte ptr [ebp], 1
		jne L_Blit_13
		mov dword ptr [esp + 0x30], eax
		mov dword ptr [esp + 0x34], 0
		jmp L_Blit_14
	L_Blit_13:
		mov dword ptr [esp + 0x30], edx
		mov dword ptr [esp + 0x34], 4
	L_Blit_14:
		mov eax, dword ptr [esp + 0x24]
		test eax, eax
		jbe L_Blit_15
		mov dword ptr [esp + 0x3c], eax
	L_Blit_18:
		mov eax, dword ptr [esp + 0x34]
		mov ecx, dword ptr [esp + 0x2c]
		mov edx, dword ptr [esp + 0x28]
		mov edi, dword ptr [esp + 0x30]
		mov dword ptr [esp + 0x24], eax
		mov eax, dword ptr [esp + 0x18]
		xor esi, esi
		mov dword ptr [esp + 0x38], ecx
		test eax, eax
		jbe L_Blit_16
	L_Blit_17:
		mov ecx, dword ptr [esp + 0x10]
		lea eax, [esi + ebx]
		shr eax, 1
		add eax, ecx
		mov ecx, dword ptr [ebp]
		mov ebp, dword ptr [esp + 0x50]
		add ecx, esi
		shr ecx, 1
		xor ebx, ebx
		mov bl, byte ptr [ecx + ebp]
		mov ecx, dword ptr [esp + 0x24]
		mov ebp, dword ptr [esp + 0x38]
		and ebx, edi
		shr ebx, cl
		mov ecx, ebp
		xor ebp, 4
		mov dword ptr [esp + 0x38], ebp
		mov ebp, dword ptr [esp + 0x58]
		not edi
		shl bl, cl
		mov cl, dl
		and cl, byte ptr [eax]
		not edx
		or bl, cl
		mov byte ptr [eax], bl
		mov ebx, dword ptr [esp + 0x24]
		mov eax, dword ptr [esp + 0x18]
		xor ebx, 4
		inc esi
		mov dword ptr [esp + 0x24], ebx
		mov ebx, dword ptr [esp + 0x4c]
		cmp esi, eax
		jb L_Blit_17
	L_Blit_16:
		mov eax, dword ptr [esp + 0x1c]
		mov ecx, dword ptr [esp + 0x50]
		mov edx, dword ptr [esp + 0x14]
		mov esi, dword ptr [esp + 0x10]
		add ecx, eax
		mov eax, dword ptr [esp + 0x3c]
		add esi, edx
		dec eax
		mov dword ptr [esp + 0x10], esi
		mov dword ptr [esp + 0x50], ecx
		mov dword ptr [esp + 0x3c], eax
		jne L_Blit_18
		jmp L_Blit_19
	L_Blit_10:
		mov ecx, dword ptr [ebp + 4]
		mov ebp, dword ptr [ebp]
		and eax, 0xffff
		imul ecx, esi
		shr eax, 3
		imul ebp, eax
		add ebp, dword ptr [esp + 0x50]
		add ebp, ecx
		mov ecx, eax
		imul ecx, ebx
		imul eax, dword ptr [esp + 0x18]
		mov ebx, edx
		mov dword ptr [esp + 0x50], ebp
		imul ebx, edi
		mov ebp, dword ptr [esp + 0x10]
		mov edi, dword ptr [esp + 0x24]
		add ebx, ebp
		add ecx, ebx
		test edi, edi
		mov dword ptr [esp + 0x10], ecx
		jbe L_Blit_19
	L_Blit_22:
		mov ebx, dword ptr [esp + 0x50]
		lea ebp, [eax + ecx]
		cmp ecx, ebp
		jae L_Blit_20
	L_Blit_21:
		mov dl, byte ptr [ebx]
		mov byte ptr [ecx], dl
		inc ecx
		inc ebx
		cmp ecx, ebp
		jb L_Blit_21
		mov ecx, dword ptr [esp + 0x10]
		mov esi, dword ptr [esp + 0x1c]
		mov edx, dword ptr [esp + 0x14]
	L_Blit_20:
		mov ebp, dword ptr [esp + 0x50]
		add ecx, edx
		add ebp, esi
		dec edi
		mov dword ptr [esp + 0x10], ecx
		mov dword ptr [esp + 0x50], ebp
		jne L_Blit_22
	L_Blit_19:
		mov ecx, dword ptr [esp + 0x54]
	L_Blit_15:
		mov eax, dword ptr [esp + 0x40]
		test eax, eax
		je L_Blit_23
		mov eax, dword ptr [ecx]
		call dword ptr [eax + 8]
	L_Blit_23:
		mov eax, dword ptr [esp + 0x44]
		test eax, eax
		je L_Blit_0
		mov ecx, dword ptr [esp + 0x20]
		mov edx, dword ptr [ecx]
		call dword ptr [edx + 8]
	L_Blit_0:
		pop edi
		pop esi
		pop ebp
		pop ebx
		add esp, 0x38
		ret 0x10
	}
}

#endif

// FUNCTION: GOLDP 0x100294f0 FOLDED
// FUNCTION: LEGORACERS 0x00416030 FOLDED
void GolSurface::BlitStretched(Rect*, GolSurface*, Rect*)
{
	// empty
}

// FUNCTION: GOLDP 0x10029920 FOLDED
// FUNCTION: LEGORACERS 0x004164c0 FOLDED
void GolSurface::CopyFromDisplay()
{
	// empty
}

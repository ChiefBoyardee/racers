#include "material/goltexturelist.h"

#include "golbmpfile.h"
#include "golstream.h"
#include "goltgafile.h"
#include "render/gold3drenderdevice.h"
#include "surface/gold3dtexture.h"

#include <string.h>

// GLOBAL: LEGORACERS 0x004c2930
GolTgaFile g_textureTgaFile;

// GLOBAL: LEGORACERS 0x004c2f10
GolBmpFile g_textureBmpFile;

#ifdef COMPAT_MODE

void GolTextureList::LoadTextures()
{
	GolTextureSourceItem sourceItem;
	GolSurfaceFormat textureFormat;
	LegoChar textureName[sizeof(GolName) + 1];

	if (m_textureSource != NULL) {
		for (LegoU32 i = 0; i < m_itemCount; i++) {
			GolTexture* texture = GetItem(i);
			if (texture->GetPixelFlags() & GolSurface::c_lockRequestRead) {
				continue;
			}

			m_textureSource->GetTextureDefinition(i, &sourceItem);

			LegoU16 flags = sourceItem.m_flags;
			if (m_renderer->VTable0x110()) {
				flags |= GolTexture::c_textureFlagBit6;
			}
			if ((flags & GolTexture::c_textureFlagColorKeyed) &&
				(m_renderer->GetFlags() & GolD3DRenderDevice::c_flagBlackColorKey)) {
				flags |= GolTexture::c_textureFlagBlackColorKey;
			}

			LegoU16 textureFlags = flags;
			texture->m_textureFlags = flags;
			LegoU16 mipmapCount = sourceItem.m_mipmapCount;
			texture->m_mipmapCount = mipmapCount;
			textureFlags |= GolTexture::c_textureFlagColorKeyDirty;
			texture->m_textureFlags = textureFlags;
			texture->m_colorKey = sourceItem.m_colorKey;
			texture->m_colorKey.m_alp = 0;

			m_renderer->SelectTextureFormat(
				sourceItem.m_textureFormat,
				&textureFormat,
				textureFlags & GolTexture::c_textureFlagColorKeyed
			);
			AllocateTexture(i, textureFormat, sourceItem.m_width, sourceItem.m_height);
			m_textureSource->OnTextureLoaded(i, 0, texture);
		}
		return;
	}

	if (m_nameTableEntry != NULL && g_hashTable != NULL) {
		g_hashTable->SetCurrentEntry(m_nameTableEntry);
	}

	for (LegoU32 i = 0; i < m_itemCount; i++) {
		GolD3DTexture* texture = GetItem(i);
		if (texture->GetPixelFlags() & GolSurface::c_lockRequestRead) {
			continue;
		}

		LegoChar firstChar = texture->m_name[0];
		::memcpy(textureName, texture->m_name, sizeof(GolName));
		if (firstChar == '\0') {
			continue;
		}
		textureName[sizeof(GolName)] = '\0';

		LegoU8 textureFlags = static_cast<LegoU8>(texture->m_textureFlags);
		GolImgFile* imageFile = &g_textureBmpFile;
		if (!(textureFlags & GolTexture::c_textureFlagBmpSource)) {
			imageFile = &g_textureTgaFile;
		}

		imageFile->Open(textureName);
		texture->LoadFromImgFile(*m_renderer, imageFile);
		imageFile->Destroy();
	}
}

#else

// FUNCTION: LEGORACERS 0x004146a0
__declspec(naked) void GolTextureList::LoadTextures()
{
	__asm {
		sub esp, 0x50
		push ebx
		push ebp
		push esi
		mov esi, ecx
		xor ebp, ebp
		push edi
		mov eax, dword ptr [esi + 0x14]
		mov dword ptr [esp + 0x3c], ebp
		cmp eax, ebp
		mov dword ptr [esp + 0x40], ebp
		mov dword ptr [esp + 0x44], ebp
		mov dword ptr [esp + 0x48], ebp
		mov dword ptr [esp + 0x4c], ebp
		mov word ptr [esp + 0x50], bp
		mov word ptr [esp + 0x52], bp
		mov dword ptr [esp + 0x1c], ebp
		mov dword ptr [esp + 0x20], ebp
		mov dword ptr [esp + 0x24], ebp
		mov dword ptr [esp + 0x28], ebp
		mov dword ptr [esp + 0x2c], ebp
		mov word ptr [esp + 0x30], bp
		mov word ptr [esp + 0x32], bp
		je L_LoadTextures_0
		cmp dword ptr [esi + 0x1c], ebp
		jbe L_LoadTextures_1
	L_LoadTextures_5:
		mov eax, dword ptr [esi]
		push ebp
		mov ecx, esi
		call dword ptr [eax + 0x28]
		mov edi, eax
		test byte ptr [edi + 0x26], 1
		jne L_LoadTextures_2
		mov ecx, dword ptr [esi + 0x14]
		lea eax, [esp + 0x34]
		push eax
		push ebp
		mov edx, dword ptr [ecx]
		call dword ptr [edx]
		mov ecx, dword ptr [esi + 0xc]
		mov ebx, dword ptr [esp + 0x58]
		mov edx, dword ptr [ecx]
		call dword ptr [edx + 0x110]
		test eax, eax
		je L_LoadTextures_3
		or ebx, 0x40
	L_LoadTextures_3:
		test bl, 0x20
		je L_LoadTextures_4
		mov eax, dword ptr [esi + 0xc]
		mov ecx, dword ptr [eax + 4]
		test ch, 2
		je L_LoadTextures_4
		or bl, 0x80
	L_LoadTextures_4:
		mov word ptr [edi + 0x36], bx
		mov cx, word ptr [esp + 0x54]
		mov eax, ebx
		mov word ptr [edi + 0x34], cx
		mov edx, dword ptr [esp + 0x5a]
		or ah, 8
		mov word ptr [edi + 0x36], ax
		and eax, 0x20
		mov dword ptr [edi + 0x30], edx
		push eax
		mov byte ptr [edi + 0x33], 0
		mov ecx, dword ptr [esi + 0xc]
		lea eax, [esp + 0x20]
		mov edx, dword ptr [ecx]
		push eax
		lea eax, [esp + 0x44]
		push eax
		call dword ptr [edx + 0xc]
		mov eax, dword ptr [esp + 0x38]
		mov ecx, dword ptr [esp + 0x34]
		mov edx, dword ptr [esi]
		push eax
		lea eax, [esp + 0x20]
		push ecx
		push eax
		push ebp
		mov ecx, esi
		call dword ptr [edx + 0x18]
		mov ecx, dword ptr [esi + 0x14]
		push edi
		push 0
		push ebp
		mov edx, dword ptr [ecx]
		call dword ptr [edx + 4]
	L_LoadTextures_2:
		mov eax, dword ptr [esi + 0x1c]
		inc ebp
		cmp ebp, eax
		jb L_LoadTextures_5
		pop edi
		pop esi
		pop ebp
		pop ebx
		add esp, 0x50
		ret
	L_LoadTextures_0:
		mov eax, dword ptr [esi + 0x18]
		cmp eax, ebp
		je L_LoadTextures_6
		mov ecx, dword ptr [g_hashTable]
		cmp ecx, ebp
		je L_LoadTextures_6
		mov dword ptr [ecx + 0x1c], eax
	L_LoadTextures_6:
		mov eax, dword ptr [esi + 0x1c]
		xor ebx, ebx
		cmp eax, ebp
		jbe L_LoadTextures_1
	L_LoadTextures_9:
		mov eax, dword ptr [esi]
		push ebx
		mov ecx, esi
		call dword ptr [eax + 0x28]
		mov edi, eax
		test byte ptr [edi + 0x26], 1
		jne L_LoadTextures_7
		mov eax, dword ptr [edi + 0x38]
		mov ecx, dword ptr [edi + 0x3c]
		test al, al
		je L_LoadTextures_7
		mov dword ptr [esp + 0x10], eax
		mov dword ptr [esp + 0x14], ecx
		mov byte ptr [esp + 0x18], 0
		mov al, byte ptr [edi + 0x36]
		test al, 8
		mov ebp, offset g_textureBmpFile
		jne L_LoadTextures_8
		mov ebp, offset g_textureTgaFile
	L_LoadTextures_8:
		mov edx, dword ptr [ebp]
		lea eax, [esp + 0x10]
		push eax
		mov ecx, ebp
		call dword ptr [edx + 8]
		mov eax, dword ptr [esi + 0xc]
		mov edx, dword ptr [edi]
		push ebp
		push eax
		mov ecx, edi
		call dword ptr [edx + 0x30]
		mov edx, dword ptr [ebp]
		mov ecx, ebp
		call dword ptr [edx + 0xc]
	L_LoadTextures_7:
		mov eax, dword ptr [esi + 0x1c]
		inc ebx
		cmp ebx, eax
		jb L_LoadTextures_9
	L_LoadTextures_1:
		pop edi
		pop esi
		pop ebp
		pop ebx
		add esp, 0x50
		ret
	}
}

#endif

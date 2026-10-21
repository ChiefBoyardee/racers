#include "golimgfile.h"

#include "device/golpalettebase.h"
#include "golerror.h"

#include <limits.h>
#include <string.h>

DECOMP_SIZE_ASSERT(GolImgFile, 0x5b0)

// GLOBAL: GOLDP 0x1005cd90
// GLOBAL: LEGORACERS 0x004be258
ColorRGBA g_nearBlackColor = {0x08, 0x08, 0x08, 0xff};

// FUNCTION: GOLDP 0x1001fe90
// FUNCTION: LEGORACERS 0x0040a8b0
GolImgFile::GolImgFile()
{
	m_width = 0;
	m_height = 0;
	m_rowByteStride = 0;
	m_hasColorKey = 0;
	m_paletteSize = 0;
	m_colorKeyRed = 0;
	m_colorKeyGrn = 0;
	m_colorKeyBlu = 0;
	m_unk0x09f = 0;
	m_constPixelBits = 0;
	m_unk0x068 = 0;
	m_colorKeyReplacement.m_red = 0;
	m_colorKeyReplacement.m_grn = 0;
	m_colorKeyReplacement.m_blu = 0;
	m_colorKeyReplacement.m_alp = 0;
	m_colorKeyPixel = 0;
	m_keepNibbleOrder = 0;
	m_remapPureBlack = 0;
	m_paletteCount = 0;
	m_paletteReservedEnd = 0;
	m_paletteCapacity = 0;
	m_paletteEntries = NULL;
}

// FUNCTION: GOLDP 0x1001ff50
// FUNCTION: LEGORACERS 0x0040a970
GolImgFile::~GolImgFile()
{
	Destroy();
}

// FUNCTION: GOLDP 0x1001ffc0
// FUNCTION: LEGORACERS 0x0040a9c0
void GolImgFile::Open(const LegoChar* p_fileName)
{
	LegoChar buffer[256];
	LegoS32 result;

	result = m_file.BufferedOpen(p_fileName, GolStream::c_modeRead, 8192);
	if (result != GolStream::e_ioSuccess) {
		::strcpy(buffer, GolStream::ErrorCodeToString(result));
		::strcat(buffer, "\nFile: ");
		::strcat(buffer, p_fileName);
		GOL_FATALERROR_MESSAGE(buffer);
	}

	m_format.ClearMasks();
	m_keepNibbleOrder = 0;
	m_remapPureBlack = 0;

	ReadHeader();
}

// FUNCTION: GOLDP 0x100200c0
// FUNCTION: LEGORACERS 0x0040aac0
void GolImgFile::Destroy()
{
	m_file.Dispose();
	if (m_paletteEntries != NULL) {
		delete[] m_paletteEntries;
		m_paletteEntries = NULL;
	}
}

// FUNCTION: GOLDP 0x100200f0
// FUNCTION: LEGORACERS 0x0040aaf0
void GolImgFile::BuildPaletteRemap(GolPaletteBase* p_palette, ColorRGBA* p_colorKey)
{
	LegoS32 colorKeyIndex = -1;
	LegoU32 sourcePaletteSize = 1 << m_format.m_bitsPerPixel;

	if (p_colorKey != NULL) {
		for (LegoU32 i = 0; i < sourcePaletteSize; i++) {
			if (p_colorKey->m_red == m_palette[i].m_red && p_colorKey->m_grn == m_palette[i].m_grn &&
				p_colorKey->m_blu == m_palette[i].m_blu) {
				colorKeyIndex = i;

				if (m_hasColorKey) {
					m_palette[i].m_red = m_colorKeyReplacement.m_red;
					m_palette[i].m_grn = m_colorKeyReplacement.m_grn;
					m_palette[i].m_blu = m_colorKeyReplacement.m_blu;
					m_palette[i].m_alp = 0;
				}
				break;
			}
		}
	}

	LegoU32 entryCount = p_palette->GetEntryCount();
	m_paletteCapacity = p_palette->GetPaletteSize();
	if (sourcePaletteSize > m_paletteCapacity) {
		sourcePaletteSize = m_paletteCapacity;
	}

	if (m_paletteCapacity == entryCount && p_colorKey == NULL) {
		for (LegoU32 i = 0; i < sourcePaletteSize; i++) {
			m_paletteRemap[i] = static_cast<LegoU8>(i);
		}

		p_palette->SetEntries(m_palette, 0, sourcePaletteSize);
		return;
	}

	if (m_paletteEntries == NULL) {
		m_paletteEntries = new ColorRGBA[m_paletteCapacity];
		if (m_paletteEntries == NULL) {
			GOL_FATALERROR(c_golErrorOutOfMemory);
		}
	}

	LegoU32 firstEntry = p_palette->GetFirstEntry();
	m_paletteCount = firstEntry;
	if (firstEntry != 0) {
		p_palette->GetEntries(m_paletteEntries, 0, firstEntry);
	}

	m_paletteReservedEnd = m_paletteCapacity - firstEntry - entryCount;
	if (m_paletteReservedEnd != 0 && m_paletteCapacity > entryCount) {
		LegoU32 reservedStart = firstEntry + entryCount;
		p_palette->GetEntries(m_paletteEntries + reservedStart, reservedStart, m_paletteReservedEnd);
	}

	if (colorKeyIndex >= 0) {
		m_paletteRemap[colorKeyIndex] = FindOrAddPaletteEntry(m_palette[colorKeyIndex]);
		for (LegoU32 i = 0; i < sourcePaletteSize; i++) {
			if (i != static_cast<LegoU32>(colorKeyIndex)) {
				m_paletteRemap[i] = FindOrAddPaletteEntry(m_palette[i]);
			}
		}
	}
	else if (m_remapPureBlack) {
		for (LegoU32 i = 0; i < sourcePaletteSize; i++) {
			ColorRGBA* color = &m_palette[i];
			if (color->m_red == 0 && color->m_grn == 0 && color->m_blu == 0) {
				g_nearBlackColor.m_alp = color->m_alp;
				m_paletteRemap[i] = FindOrAddPaletteEntry(g_nearBlackColor);
			}
			else {
				m_paletteRemap[i] = FindOrAddPaletteEntry(*color);
			}
		}
	}
	else {
		for (LegoU32 i = 0; i < sourcePaletteSize; i++) {
			m_paletteRemap[i] = FindOrAddPaletteEntry(m_palette[i]);
		}
	}

	p_palette->SetEntries(m_paletteEntries + firstEntry, firstEntry, m_paletteCount - firstEntry);
	delete[] m_paletteEntries;
	m_paletteEntries = NULL;
}

// FUNCTION: GOLDP 0x10020370
// FUNCTION: LEGORACERS 0x0040ad70
LegoU32 GolImgFile::FindOrAddPaletteEntry(const ColorRGBA& p_rgba)
{
	LegoU32 i;

	for (i = 0; i < m_paletteCount; i++) {
		if (p_rgba.m_red == m_paletteEntries[i].m_red && p_rgba.m_grn == m_paletteEntries[i].m_grn &&
			p_rgba.m_blu == m_paletteEntries[i].m_blu) {
			return i;
		}
	}

	for (i = m_paletteCapacity - m_paletteReservedEnd; i < m_paletteCapacity; i++) {
		if (p_rgba.m_red == m_paletteEntries[i].m_red && p_rgba.m_grn == m_paletteEntries[i].m_grn &&
			p_rgba.m_blu == m_paletteEntries[i].m_blu) {
			return i;
		}
	}

	if (m_paletteReservedEnd + m_paletteCount >= m_paletteCapacity) {
		LegoU32 bestMatch = 0;
		LegoS32 bestMatchDiff = INT_MAX;
		for (i = 0; i < m_paletteCapacity; i++) {
			LegoS32 dR = static_cast<LegoU32>(m_paletteEntries[i].m_red) - static_cast<LegoU32>(p_rgba.m_red);
			LegoS32 dG = static_cast<LegoU32>(m_paletteEntries[i].m_grn) - static_cast<LegoU32>(p_rgba.m_grn);
			LegoS32 dB = static_cast<LegoU32>(m_paletteEntries[i].m_blu) - static_cast<LegoU32>(p_rgba.m_blu);
			LegoS32 d = dR * dR + dG * dG + dB * dB;
			if (d < bestMatchDiff) {
				bestMatch = i;
				bestMatchDiff = d;
			}
		}
		return bestMatch;
	}
	else {
		LegoU32 idx = m_paletteCount;
		m_paletteEntries[idx].m_red = p_rgba.m_red;
		m_paletteEntries[idx].m_grn = p_rgba.m_grn;
		m_paletteEntries[idx].m_blu = p_rgba.m_blu;
		m_paletteEntries[idx].m_alp = p_rgba.m_alp;
		m_paletteCount++;
		return idx;
	}
}

// FUNCTION: GOLDP 0x100204d0
// FUNCTION: LEGORACERS 0x0040aed0
void GolImgFile::SetupPixelConversion(const GolSurfaceFormat& p_format, ColorRGBA* p_colorKey)
{
	const GolSurfaceFormat& destFormat = p_format;
	m_hasColorKey = FALSE;

	if (destFormat.m_paletteMask != 0) {
		if (p_colorKey != NULL) {
			m_hasColorKey = TRUE;
		}
		return;
	}

	switch (m_format.m_bitsPerPixel) {
	case 4:
		m_srcStrideMask = 0x80;
		break;
	case 8:
		m_srcStrideMask = 0x88;
		break;
	case 16:
		m_srcStrideMask = 0x99;
		break;
	case 24:
		m_srcStrideMask = 0xbb;
		break;
	case 32:
		m_srcStrideMask = 0xff;
		break;
	default:
		GOL_FATALERROR_MESSAGE("Unsupported color depth.");
		break;
	}

	LegoU32 srcRedBitCount;
	LegoU32 srcGreenBitCount;
	LegoU32 srcBlueBitCount;
	LegoU32 srcAlphaBitCount;

	if (m_format.m_paletteMask > 0) {
		srcRedBitCount = 8;
		m_redSrcShift = 0;
		srcGreenBitCount = srcRedBitCount;
		m_grnSrcShift = 0;
		m_bluSrcShift = 0;
		srcBlueBitCount = srcRedBitCount;
		m_alpSrcShift = 0;
		srcAlphaBitCount = 0;
	}
	else {
		m_redSrcShift = m_format.GetRedBitShift();
		srcRedBitCount = m_format.GetRedBitCount();
		m_grnSrcShift = m_format.GetGreenBitShift();
		srcGreenBitCount = m_format.GetGreenBitCount();
		m_bluSrcShift = m_format.GetBlueBitShift();
		srcBlueBitCount = m_format.GetBlueBitCount();
		m_alpSrcShift = m_format.GetAlphaBitShift();
		srcAlphaBitCount = m_format.GetAlphaBitCount();
	}

	LegoU32 destRedBitCount = destFormat.GetRedBitCount();
	if (srcRedBitCount > destRedBitCount) {
		m_redSrcShift += srcRedBitCount - destRedBitCount;
	}
	m_redDstShift = destFormat.GetRedBitShift();
	if (destRedBitCount > srcRedBitCount) {
		m_redDstShift += destRedBitCount - srcRedBitCount;
	}

	LegoU32 destGreenBitCount = destFormat.GetGreenBitCount();
	if (srcGreenBitCount > destGreenBitCount) {
		m_grnSrcShift += srcGreenBitCount - destGreenBitCount;
	}
	m_grnDstShift = destFormat.GetGreenBitShift();
	if (destGreenBitCount > srcGreenBitCount) {
		m_grnDstShift += destGreenBitCount - srcGreenBitCount;
	}

	LegoU32 destBlueBitCount = destFormat.GetBlueBitCount();
	if (srcBlueBitCount > destBlueBitCount) {
		m_bluSrcShift += srcBlueBitCount - destBlueBitCount;
	}
	m_bluDstShift = destFormat.GetBlueBitShift();
	if (destBlueBitCount > srcBlueBitCount) {
		m_bluDstShift += destBlueBitCount - srcBlueBitCount;
	}

	if (p_colorKey != NULL) {
		m_hasColorKey = TRUE;
		m_colorKeyRed = p_colorKey->m_red >> (8 - destRedBitCount);
		m_colorKeyGrn = p_colorKey->m_grn >> (8 - destGreenBitCount);
		m_colorKeyBlu = p_colorKey->m_blu >> (8 - destBlueBitCount);
		m_alpSrcShift = 0;
		m_alpDstShift = 0;

		LegoU32 red = (m_colorKeyReplacement.m_red >> (8 - destRedBitCount)) << m_redDstShift;
		LegoU32 grn = (m_colorKeyReplacement.m_grn >> (8 - destGreenBitCount)) << m_grnDstShift;
		LegoU32 blu = (m_colorKeyReplacement.m_blu >> (8 - destBlueBitCount)) << m_bluDstShift;
		m_colorKeyPixel = red | grn | blu;
		m_constPixelBits = destFormat.m_alpBitMask;
		return;
	}

	LegoU32 destAlphaBitCount = destFormat.GetAlphaBitCount();
	if (srcAlphaBitCount > destAlphaBitCount) {
		m_alpSrcShift += srcAlphaBitCount - destAlphaBitCount;
	}
	m_alpDstShift = destFormat.GetAlphaBitShift();
	if (destAlphaBitCount > srcAlphaBitCount) {
		m_alpDstShift += destAlphaBitCount - srcAlphaBitCount;
	}
}

// FUNCTION: GOLDP 0x100207e0
// FUNCTION: LEGORACERS 0x0040b1e0
void GolImgFile::ConvertRow(const void* p_src, void* p_dst, const GolSurfaceFormat& p_format)
{
	const LegoU8* src = static_cast<const LegoU8*>(p_src);
	LegoU8* dst = static_cast<LegoU8*>(p_dst);

	if (m_format.m_paletteMask > 0) {
		if (p_format.m_paletteMask != 0) {
			if (m_format.m_bitsPerPixel == 4) {
				if (p_format.m_bitsPerPixel == 4) {
					if (m_keepNibbleOrder) {
						RemapRow4To4(src, dst);
					}
					else {
						RemapRow4To4Swapped(src, dst);
					}
					return;
				}

				if (p_format.m_bitsPerPixel == 8) {
					RemapRow4To8(src, dst);
					return;
				}
			}
			else if (m_format.m_bitsPerPixel == 8 && p_format.m_bitsPerPixel == 8) {
				RemapRow8To8(src, dst);
				return;
			}
		}
		else if (m_format.m_bitsPerPixel == 4) {
			switch (p_format.m_bitsPerPixel) {
			case 8:
				ConvertRowIndexed4To8(src, dst);
				return;
			case 16:
				ConvertRowIndexed4To16(src, reinterpret_cast<LegoU16*>(dst));
				return;
			case 24:
				ConvertRowIndexed4To24(src, dst);
				return;
			case 32:
				ConvertRowIndexed4To32(src, reinterpret_cast<LegoU32*>(dst));
				return;
			}
		}
		else {
			switch (p_format.m_bitsPerPixel) {
			case 8:
				ConvertRowIndexed8To8(src, dst);
				return;
			case 16:
				ConvertRowIndexed8To16(src, reinterpret_cast<LegoU16*>(dst));
				return;
			case 24:
				ConvertRowIndexed8To24(src, dst);
				return;
			case 32:
				ConvertRowIndexed8To32(src, reinterpret_cast<LegoU32*>(dst));
				return;
			}
		}
	}
	else {
		if (m_format.m_intensityMask > 0) {
			GOL_FATALERROR_MESSAGE("Intensity formats not supported");
			return;
		}

		if (p_format.m_paletteMask != 0) {
			GOL_FATALERROR_MESSAGE("RGB to CI conversion not supported");
			return;
		}

		switch (p_format.m_bitsPerPixel) {
		case 8:
			ConvertRowRgbTo8(src, dst);
			return;
		case 16:
			ConvertRowRgbTo16(src, reinterpret_cast<LegoU16*>(dst));
			return;
		case 24:
			ConvertRowRgbTo24(src, dst);
			return;
		case 32:
			ConvertRowRgbTo32(src, reinterpret_cast<LegoU32*>(dst));
			return;
		}
	}
}

// FUNCTION: GOLDP 0x10020b90
// FUNCTION: LEGORACERS 0x0040b480
void GolImgFile::ConvertRowRgbTo8(const LegoU8* p_src, LegoU8* p_dst)
{
	LegoU8* endDst = p_dst + m_width;
	LegoU32 u32;
	if (m_hasColorKey) {
		for (; p_dst < endDst;) {
			u32 = *p_src;
			p_src += (m_srcStrideMask >> 0) & 0x1;
			u32 |= *p_src << 8;
			p_src += (m_srcStrideMask >> 1) & 0x1;
			u32 |= *p_src << 16;
			p_src += (m_srcStrideMask >> 2) & 0x1;
			u32 |= *p_src << 24;
			p_src += (m_srcStrideMask >> 3) & 0x1;
			LegoU32 r = (m_format.m_redBitMask & u32) >> m_redSrcShift;
			LegoU32 g = (m_format.m_grnBitMask & u32) >> m_grnSrcShift;
			LegoU32 b = (m_format.m_bluBitMask & u32) >> m_bluSrcShift;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				*p_dst++ = m_colorKeyPixel;
			}
			else {
				LegoU32 pixel = r << m_redDstShift;
				pixel |= g << m_grnDstShift;
				pixel |= b << m_bluDstShift;
				pixel |= m_constPixelBits;
				*p_dst++ = pixel;
			}
		}
	}
	else {
		for (; p_dst < endDst; p_dst++) {
			u32 = *p_src;
			p_src += (m_srcStrideMask >> 0) & 0x1;
			u32 |= *p_src << 8;
			p_src += (m_srcStrideMask >> 1) & 0x1;
			u32 |= *p_src << 16;
			p_src += (m_srcStrideMask >> 2) & 0x1;
			u32 |= *p_src << 24;
			p_src += (m_srcStrideMask >> 3) & 0x1;
			LegoU8 pixel = ((m_format.m_alpBitMask & u32) >> m_alpSrcShift) << m_alpDstShift;
			pixel |= ((m_format.m_bluBitMask & u32) >> m_bluSrcShift) << m_bluDstShift;
			pixel |= ((m_format.m_grnBitMask & u32) >> m_grnSrcShift) << m_grnDstShift;
			pixel |= ((m_format.m_redBitMask & u32) >> m_redSrcShift) << m_redDstShift;
			*p_dst = pixel;
		}
	}
}

// FUNCTION: GOLDP 0x10020d60
// FUNCTION: LEGORACERS 0x0040b650
void GolImgFile::ConvertRowRgbTo16(const LegoU8* p_src, LegoU16* p_dst)
{
	LegoU16* endDst = p_dst + m_width;
	LegoU32 u32;
	if (m_hasColorKey) {
		for (; p_dst < endDst;) {
			u32 = *p_src;
			p_src += (m_srcStrideMask >> 0) & 0x1;
			u32 |= *p_src << 8;
			p_src += (m_srcStrideMask >> 1) & 0x1;
			u32 |= *p_src << 16;
			p_src += (m_srcStrideMask >> 2) & 0x1;
			u32 |= *p_src << 24;
			p_src += (m_srcStrideMask >> 3) & 0x1;
			LegoU32 r = (m_format.m_redBitMask & u32) >> m_redSrcShift;
			LegoU32 g = (m_format.m_grnBitMask & u32) >> m_grnSrcShift;
			LegoU32 b = (m_format.m_bluBitMask & u32) >> m_bluSrcShift;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				*p_dst++ = m_colorKeyPixel;
			}
			else {
				LegoU32 pixel = r << m_redDstShift;
				pixel |= g << m_grnDstShift;
				pixel |= b << m_bluDstShift;
				pixel |= m_constPixelBits;
				*p_dst++ = pixel;
			}
		}
	}
	else {
		for (; p_dst < endDst; p_dst++) {
			u32 = *p_src;
			p_src += (m_srcStrideMask >> 0) & 0x1;
			u32 |= *p_src << 8;
			p_src += (m_srcStrideMask >> 1) & 0x1;
			u32 |= *p_src << 16;
			p_src += (m_srcStrideMask >> 2) & 0x1;
			u32 |= *p_src << 24;
			p_src += (m_srcStrideMask >> 3) & 0x1;
			LegoU32 pixel = ((m_format.m_bluBitMask & u32) >> m_bluSrcShift) << m_bluDstShift;
			pixel |= ((m_format.m_grnBitMask & u32) >> m_grnSrcShift) << m_grnDstShift;
			pixel |= ((m_format.m_redBitMask & u32) >> m_redSrcShift) << m_redDstShift;
			pixel |= ((m_format.m_alpBitMask & u32) >> m_alpSrcShift) << m_alpDstShift;
			*p_dst = pixel;
		}
	}
}

// FUNCTION: GOLDP 0x10020f20
// FUNCTION: LEGORACERS 0x0040b810
void GolImgFile::ConvertRowRgbTo24(const LegoU8* p_src, LegoU8* p_dst)
{
	LegoU32 i;
	LegoU32 u32;
	if (m_hasColorKey) {
		for (i = 0; i < m_width; i++) {
			u32 = *p_src;
			p_src += (m_srcStrideMask >> 0) & 0x1;
			u32 |= *p_src << 8;
			p_src += (m_srcStrideMask >> 1) & 0x1;
			u32 |= *p_src << 16;
			p_src += (m_srcStrideMask >> 2) & 0x1;
			u32 |= *p_src << 24;
			p_src += (m_srcStrideMask >> 3) & 0x1;
			LegoU32 r = (m_format.m_redBitMask & u32) >> m_redSrcShift;
			LegoU32 g = (m_format.m_grnBitMask & u32) >> m_grnSrcShift;
			LegoU32 b = (m_format.m_bluBitMask & u32) >> m_bluSrcShift;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				*p_dst++ = m_colorKeyPixel;
				*p_dst++ = m_colorKeyPixel >> 8;
				*p_dst++ = m_colorKeyPixel >> 16;
			}
			else {
				LegoU32 rgb = (r << m_redDstShift) | (g << m_grnDstShift) | (b << m_bluDstShift) | m_constPixelBits;
				*p_dst++ = rgb;
				*p_dst++ = rgb >> 8;
				*p_dst++ = rgb >> 16;
			}
		}
	}
	else {
		for (i = 0; i < m_width; i++) {
			u32 = *p_src;
			p_src += (m_srcStrideMask >> 0) & 0x1;
			u32 |= *p_src << 8;
			p_src += (m_srcStrideMask >> 1) & 0x1;
			u32 |= *p_src << 16;
			p_src += (m_srcStrideMask >> 2) & 0x1;
			u32 |= *p_src << 24;
			p_src += (m_srcStrideMask >> 3) & 0x1;
			LegoU32 blu = ((m_format.m_bluBitMask & u32) >> m_bluSrcShift) << m_bluDstShift;
			LegoU32 grn = ((m_format.m_grnBitMask & u32) >> m_grnSrcShift) << m_grnDstShift;
			LegoU32 alp = ((m_format.m_alpBitMask & u32) >> m_alpSrcShift) << m_alpDstShift;
			LegoU32 red = ((m_format.m_redBitMask & u32) >> m_redSrcShift) << m_redDstShift;
			LegoU32 rgb = blu | grn | alp | red;
			*p_dst++ = rgb;
			*p_dst++ = rgb >> 8;
			*p_dst++ = rgb >> 16;
		}
	}
}

// FUNCTION: GOLDP 0x10021130
// FUNCTION: LEGORACERS 0x0040ba20
void GolImgFile::ConvertRowRgbTo32(const LegoU8* p_src, LegoU32* p_dst)
{
	LegoU32* endDst = p_dst + m_width;
	LegoU32 u32;
	if (m_hasColorKey) {
		for (; p_dst < endDst;) {
			u32 = *p_src << 0;
			p_src += (m_srcStrideMask >> 0) & 0x1;
			u32 |= *p_src << 8;
			p_src += (m_srcStrideMask >> 1) & 0x1;
			u32 |= *p_src << 16;
			p_src += (m_srcStrideMask >> 2) & 0x1;
			u32 |= *p_src << 24;
			p_src += (m_srcStrideMask >> 3) & 0x1;
			LegoU32 r = (m_format.m_redBitMask & u32) >> m_redSrcShift;
			LegoU32 g = (m_format.m_grnBitMask & u32) >> m_grnSrcShift;
			LegoU32 b = (m_format.m_bluBitMask & u32) >> m_bluSrcShift;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				*p_dst++ = m_colorKeyPixel;
			}
			else {
				LegoU32 pixel = r << m_redDstShift;
				pixel |= g << m_grnDstShift;
				pixel |= b << m_bluDstShift;
				pixel |= m_constPixelBits;
				*p_dst++ = pixel;
			}
		}
	}
	else {
		for (; p_dst < endDst; p_dst++) {
			u32 = *p_src << 0;
			p_src += (m_srcStrideMask >> 0) & 0x1;
			u32 |= *p_src << 8;
			p_src += (m_srcStrideMask >> 1) & 0x1;
			u32 |= *p_src << 16;
			p_src += (m_srcStrideMask >> 2) & 0x1;
			u32 |= *p_src << 24;
			p_src += (m_srcStrideMask >> 3) & 0x1;
			LegoU32 pixel = ((m_format.m_redBitMask & u32) >> m_redSrcShift) << m_redDstShift;
			pixel |= ((m_format.m_bluBitMask & u32) >> m_bluSrcShift) << m_bluDstShift;
			pixel |= ((m_format.m_grnBitMask & u32) >> m_grnSrcShift) << m_grnDstShift;
			pixel |= ((m_format.m_alpBitMask & u32) >> m_alpSrcShift) << m_alpDstShift;
			*p_dst = pixel;
		}
	}
}

// FUNCTION: LEGORACERS 0x0040bbe0
void GolImgFile::RemapRow4To4(const LegoU8* p_src, LegoU8* p_dst)
{
	LegoU8* end = p_dst + ((m_width + 1) >> 1);
	for (; p_dst < end; p_dst++, p_src++) {
		LegoU8 low = m_paletteRemap[*p_src & 0x0f];
		*p_dst = low;
		*p_dst = (m_paletteRemap[*p_src >> 4] << 4) | low;
	}
}

// FUNCTION: LEGORACERS 0x0040bc30
void GolImgFile::RemapRow4To4Swapped(const LegoU8* p_src, LegoU8* p_dst)
{
	LegoU8* end = p_dst + ((m_width + 1) >> 1);
	for (; p_dst < end; p_dst++, p_src++) {
		LegoU8 low = m_paletteRemap[*p_src >> 4];
		*p_dst = low;
		*p_dst = (m_paletteRemap[*p_src & 0x0f] << 4) | low;
	}
}

// FUNCTION: LEGORACERS 0x0040bc80
void GolImgFile::RemapRow4To8(const LegoU8* p_src, LegoU8* p_dst)
{
	LegoU8* end = p_dst + m_width;
	for (; p_dst + 1 < end; p_dst += 2, p_src++) {
		*p_dst = m_paletteRemap[*p_src >> 4];
		p_dst[1] = m_paletteRemap[*p_src & 0x0f];
	}
	if (p_dst < end) {
		*p_dst = m_paletteRemap[*p_src & 0x0f];
	}
}

// FUNCTION: LEGORACERS 0x0040bce0
void GolImgFile::RemapRow8To8(const LegoU8* p_src, LegoU8* p_dst)
{
	LegoU8* end = p_dst + m_width;
	while (p_dst < end) {
		*p_dst++ = m_paletteRemap[*p_src++];
	}
}

// FUNCTION: GOLDP 0x100212f0
// FUNCTION: LEGORACERS 0x0040bd10
void GolImgFile::ConvertRowIndexed4To8(const LegoU8* p_src, LegoU8* p_dst)
{
	LegoU8* endDst = p_dst + m_width;
	LegoU8 index;

	if (m_hasColorKey) {
		LegoU32 r;
		LegoU32 g;
		LegoU32 b;

		while (p_dst + 1 < endDst) {
			const ColorRGBA* c = &m_palette[*p_src >> 4];
			r = c->m_red >> m_redSrcShift;
			g = c->m_grn >> m_grnSrcShift;
			b = c->m_blu >> m_bluSrcShift;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				*p_dst++ = m_colorKeyPixel;
			}
			else {
				b <<= m_bluDstShift;
				g <<= m_grnDstShift;
				r <<= m_redDstShift;
				LegoU32 pixel = b | g;
				pixel |= r;
				pixel |= m_constPixelBits;
				*p_dst++ = pixel;
			}

			index = *p_src & 0x0f;
			p_src++;
			c = &m_palette[index];
			r = c->m_red >> m_redSrcShift;
			g = c->m_grn >> m_grnSrcShift;
			b = c->m_blu >> m_bluSrcShift;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				*p_dst++ = m_colorKeyPixel;
			}
			else {
				b <<= m_bluDstShift;
				g <<= m_grnDstShift;
				r <<= m_redDstShift;
				LegoU32 pixel = b | g;
				pixel |= r;
				pixel |= m_constPixelBits;
				*p_dst++ = pixel;
			}
		}
		if (p_dst < endDst) {
			const ColorRGBA* c = &m_palette[*p_src >> 4];
			r = c->m_red >> m_redSrcShift;
			g = c->m_grn >> m_grnSrcShift;
			b = c->m_blu >> m_bluSrcShift;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				p_dst[0] = m_colorKeyPixel;
			}
			else {
				b <<= m_bluDstShift;
				g <<= m_grnDstShift;
				r <<= m_redDstShift;
				LegoU32 pixel = b | g;
				pixel |= r;
				pixel |= m_constPixelBits;
				p_dst[0] = pixel;
			}
		}
	}
	else {
		while (p_dst + 1 < endDst) {
			const ColorRGBA* c = &m_palette[*p_src >> 4];
			LegoU32 b = c->m_blu >> m_bluSrcShift;
			b <<= m_bluDstShift;
			LegoU32 g = c->m_grn >> m_grnSrcShift;
			g <<= m_grnDstShift;
			LegoU32 r = c->m_red >> m_redSrcShift;
			r <<= m_redDstShift;
			LegoU32 pixel = b | g;
			pixel |= r;
			*p_dst++ = pixel;

			index = *p_src & 0x0f;
			p_src++;
			c = &m_palette[index];
			b = c->m_blu >> m_bluSrcShift;
			b <<= m_bluDstShift;
			g = c->m_grn >> m_grnSrcShift;
			g <<= m_grnDstShift;
			r = c->m_red >> m_redSrcShift;
			r <<= m_redDstShift;
			pixel = b | g;
			pixel |= r;
			*p_dst++ = pixel;
		}
		if (p_dst < endDst) {
			const ColorRGBA* c = &m_palette[*p_src >> 4];
			LegoU32 b = c->m_blu >> m_bluSrcShift;
			b <<= m_bluDstShift;
			LegoU32 g = c->m_grn >> m_grnSrcShift;
			g <<= m_grnDstShift;
			LegoU32 r = c->m_red >> m_redSrcShift;
			r <<= m_redDstShift;
			LegoU32 pixel = b | g;
			pixel |= r;
			p_dst[0] = pixel;
		}
	}
}

// FUNCTION: GOLDP 0x10021660
// FUNCTION: LEGORACERS 0x0040c080
void GolImgFile::ConvertRowIndexed4To16(const LegoU8* p_src, LegoU16* p_dst)
{
	LegoU16* endDst = p_dst + m_width;
	LegoU8 index;

	if (m_hasColorKey) {
		LegoU32 r;
		LegoU32 g;
		LegoU32 b;

		while (p_dst + 1 < endDst) {
			const ColorRGBA* c = &m_palette[*p_src >> 4];
			r = c->m_red >> m_redSrcShift;
			g = c->m_grn >> m_grnSrcShift;
			b = c->m_blu >> m_bluSrcShift;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				*p_dst++ = m_colorKeyPixel;
			}
			else {
				b <<= m_bluDstShift;
				g <<= m_grnDstShift;
				r <<= m_redDstShift;
				LegoU32 pixel = b | g;
				pixel |= r;
				pixel |= m_constPixelBits;
				*p_dst++ = pixel;
			}

			index = *p_src & 0x0f;
			p_src++;
			c = &m_palette[index];
			r = c->m_red >> m_redSrcShift;
			g = c->m_grn >> m_grnSrcShift;
			b = c->m_blu >> m_bluSrcShift;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				*p_dst++ = m_colorKeyPixel;
			}
			else {
				b <<= m_bluDstShift;
				g <<= m_grnDstShift;
				r <<= m_redDstShift;
				LegoU32 pixel = b | g;
				pixel |= r;
				pixel |= m_constPixelBits;
				*p_dst++ = pixel;
			}
		}
		if (p_dst < endDst) {
			const ColorRGBA* c = &m_palette[*p_src >> 4];
			r = c->m_red >> m_redSrcShift;
			g = c->m_grn >> m_grnSrcShift;
			b = c->m_blu >> m_bluSrcShift;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				p_dst[0] = m_colorKeyPixel;
			}
			else {
				b <<= m_bluDstShift;
				g <<= m_grnDstShift;
				r <<= m_redDstShift;
				LegoU32 pixel = b | g;
				pixel |= r;
				pixel |= m_constPixelBits;
				p_dst[0] = pixel;
			}
		}
	}
	else {
		while (p_dst + 1 < endDst) {
			const ColorRGBA* c = &m_palette[*p_src >> 4];
			LegoU32 b = c->m_blu >> m_bluSrcShift;
			b <<= m_bluDstShift;
			LegoU32 g = c->m_grn >> m_grnSrcShift;
			g <<= m_grnDstShift;
			LegoU32 r = c->m_red >> m_redSrcShift;
			r <<= m_redDstShift;
			LegoU32 pixel = b | g;
			pixel |= r;
			*p_dst++ = pixel;

			index = *p_src & 0x0f;
			p_src++;
			c = &m_palette[index];
			b = c->m_blu >> m_bluSrcShift;
			b <<= m_bluDstShift;
			g = c->m_grn >> m_grnSrcShift;
			g <<= m_grnDstShift;
			r = c->m_red >> m_redSrcShift;
			r <<= m_redDstShift;
			pixel = b | g;
			pixel |= r;
			*p_dst++ = pixel;
		}
		if (p_dst < endDst) {
			const ColorRGBA* c = &m_palette[*p_src >> 4];
			LegoU32 b = c->m_blu >> m_bluSrcShift;
			b <<= m_bluDstShift;
			LegoU32 g = c->m_grn >> m_grnSrcShift;
			g <<= m_grnDstShift;
			LegoU32 r = c->m_red >> m_redSrcShift;
			r <<= m_redDstShift;
			LegoU32 pixel = b | g;
			pixel |= r;
			p_dst[0] = pixel;
		}
	}
}

#ifdef COMPAT_MODE

void GolImgFile::ConvertRowIndexed4To24(const LegoU8* p_src, LegoU8* p_dst)
{
	LegoU32 i = 1;
	LegoU32 pixelIndex = 0;
	LegoU8 index;

	if (m_hasColorKey) {
		while (i < m_width) {
			const ColorRGBA* c = &m_palette[*p_src >> 4];
			LegoU32 r = c->m_red >> m_redSrcShift;
			LegoU32 g = c->m_grn >> m_grnSrcShift;
			LegoU32 b = c->m_blu >> m_bluSrcShift;
			LegoU32 pixel;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				*p_dst++ = m_colorKeyPixel;
				*p_dst++ = m_colorKeyPixel >> 8;
				*p_dst++ = m_colorKeyPixel >> 16;
			}
			else {
				pixel = (r << m_redDstShift) | (g << m_grnDstShift) | (b << m_bluDstShift) | m_constPixelBits;
				*p_dst++ = pixel;
				*p_dst++ = pixel >> 8;
				*p_dst++ = pixel >> 16;
			}

			index = *p_src & 0x0f;
			p_src++;
			c = &m_palette[index];
			r = c->m_red >> m_redSrcShift;
			g = c->m_grn >> m_grnSrcShift;
			b = c->m_blu >> m_bluSrcShift;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				*p_dst++ = m_colorKeyPixel;
				*p_dst++ = m_colorKeyPixel >> 8;
				*p_dst++ = m_colorKeyPixel >> 16;
			}
			else {
				pixel = (r << m_redDstShift) | (g << m_grnDstShift) | (b << m_bluDstShift) | m_constPixelBits;
				*p_dst++ = pixel;
				*p_dst++ = pixel >> 8;
				*p_dst++ = pixel >> 16;
			}

			i += 2;
			pixelIndex += 2;
		}
		if (pixelIndex < m_width) {
			const ColorRGBA* c = &m_palette[*p_src >> 4];
			LegoU32 r = c->m_red >> m_redSrcShift;
			LegoU32 g = c->m_grn >> m_grnSrcShift;
			LegoU32 b = c->m_blu >> m_bluSrcShift;
			LegoU32 pixel;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				*p_dst++ = m_colorKeyPixel;
				*p_dst++ = m_colorKeyPixel >> 8;
				*p_dst = m_colorKeyPixel >> 16;
			}
			else {
				pixel = (r << m_redDstShift) | (g << m_grnDstShift) | (b << m_bluDstShift) | m_constPixelBits;
				*p_dst++ = pixel;
				*p_dst++ = pixel >> 8;
				*p_dst = pixel >> 16;
			}
		}
	}
	else {
		while (i < m_width) {
			const ColorRGBA* c = &m_palette[*p_src >> 4];
			LegoU32 pixel = ((c->m_red >> m_redSrcShift) << m_redDstShift) |
							((c->m_grn >> m_grnSrcShift) << m_grnDstShift) |
							((c->m_blu >> m_bluSrcShift) << m_bluDstShift);
			*p_dst++ = pixel;
			*p_dst++ = pixel >> 8;
			*p_dst++ = pixel >> 16;

			index = *p_src & 0x0f;
			p_src++;
			c = &m_palette[index];
			pixel = ((c->m_red >> m_redSrcShift) << m_redDstShift) | ((c->m_grn >> m_grnSrcShift) << m_grnDstShift) |
					((c->m_blu >> m_bluSrcShift) << m_bluDstShift);
			*p_dst++ = pixel;
			*p_dst++ = pixel >> 8;
			*p_dst++ = pixel >> 16;

			i += 2;
			pixelIndex += 2;
		}
		if (pixelIndex < m_width) {
			const ColorRGBA* c = &m_palette[*p_src >> 4];
			LegoU32 pixel = ((c->m_red >> m_redSrcShift) << m_redDstShift) |
							((c->m_grn >> m_grnSrcShift) << m_grnDstShift) |
							((c->m_blu >> m_bluSrcShift) << m_bluDstShift);
			*p_dst++ = pixel;
			*p_dst++ = pixel >> 8;
			*p_dst = pixel >> 16;
		}
	}
}

#else

// FUNCTION: GOLDP 0x100219e0
// FUNCTION: LEGORACERS 0x0040c400
__declspec(naked) void GolImgFile::ConvertRowIndexed4To24(const LegoU8* p_src, LegoU8* p_dst)
{
	__asm {
		sub esp, 8
		mov eax, ecx
		push ebx
		xor ecx, ecx
		push ebp
		mov edx, dword ptr [eax + 0x98]
		push esi
		cmp edx, ecx
		mov edx, dword ptr [eax + 0x44]
		mov dword ptr [esp + 0xc], ecx
		push edi
		mov ecx, 1
		je L_ConvertRowIndexed4To24_0
		cmp edx, ecx
		jbe L_ConvertRowIndexed4To24_1
		mov edx, dword ptr [esp + 0x20]
		mov dword ptr [esp + 0x14], ecx
	L_ConvertRowIndexed4To24_6:
		mov esi, dword ptr [esp + 0x1c]
		xor ecx, ecx
		mov cl, byte ptr [esi]
		shr ecx, 4
		lea esi, [eax + ecx*4 + 0xa8]
		xor ecx, ecx
		mov cl, byte ptr [esi]
		mov edi, ecx
		mov cl, byte ptr [eax + 0x78]
		shr edi, cl
		xor ecx, ecx
		mov cl, byte ptr [esi + 1]
		mov ebp, ecx
		mov cl, byte ptr [eax + 0x7c]
		shr ebp, cl
		xor ecx, ecx
		mov cl, byte ptr [esi + 2]
		mov esi, ecx
		mov cl, byte ptr [eax + 0x80]
		shr esi, cl
		xor ecx, ecx
		mov cl, byte ptr [eax + 0x9c]
		cmp edi, ecx
		jne L_ConvertRowIndexed4To24_2
		xor ecx, ecx
		mov cl, byte ptr [eax + 0x9d]
		cmp ebp, ecx
		jne L_ConvertRowIndexed4To24_2
		xor ecx, ecx
		mov cl, byte ptr [eax + 0x9e]
		cmp esi, ecx
		jne L_ConvertRowIndexed4To24_2
		mov cl, byte ptr [eax + 0xa4]
		mov byte ptr [edx], cl
		mov ecx, dword ptr [eax + 0xa4]
		inc edx
		shr ecx, 8
		mov byte ptr [edx], cl
		mov ecx, dword ptr [eax + 0xa4]
		inc edx
		jmp L_ConvertRowIndexed4To24_3
	L_ConvertRowIndexed4To24_2:
		mov ecx, dword ptr [eax + 0x90]
		shl esi, cl
		mov ecx, dword ptr [eax + 0x8c]
		shl ebp, cl
		mov ecx, dword ptr [eax + 0x88]
		shl edi, cl
		or esi, ebp
		or esi, edi
		mov edi, dword ptr [eax + 0x74]
		or esi, edi
		inc edx
		mov ecx, esi
		mov byte ptr [edx - 1], cl
		mov ebx, ecx
		shr ebx, 8
		mov byte ptr [edx], bl
		inc edx
	L_ConvertRowIndexed4To24_3:
		mov esi, dword ptr [esp + 0x1c]
		shr ecx, 0x10
		mov byte ptr [edx], cl
		mov cl, byte ptr [esi]
		and cl, 0xf
		inc edx
		mov byte ptr [esp + 0x20], cl
		inc esi
		mov ecx, dword ptr [esp + 0x20]
		mov dword ptr [esp + 0x1c], esi
		and ecx, 0xff
		lea esi, [eax + ecx*4 + 0xa8]
		xor ecx, ecx
		mov cl, byte ptr [esi]
		mov edi, ecx
		mov cl, byte ptr [eax + 0x78]
		shr edi, cl
		xor ecx, ecx
		mov cl, byte ptr [esi + 1]
		mov ebp, ecx
		mov cl, byte ptr [eax + 0x7c]
		shr ebp, cl
		xor ecx, ecx
		mov cl, byte ptr [esi + 2]
		mov esi, ecx
		mov cl, byte ptr [eax + 0x80]
		shr esi, cl
		xor ecx, ecx
		mov cl, byte ptr [eax + 0x9c]
		cmp edi, ecx
		jne L_ConvertRowIndexed4To24_4
		xor ecx, ecx
		mov cl, byte ptr [eax + 0x9d]
		cmp ebp, ecx
		jne L_ConvertRowIndexed4To24_4
		xor ecx, ecx
		mov cl, byte ptr [eax + 0x9e]
		cmp esi, ecx
		jne L_ConvertRowIndexed4To24_4
		mov cl, byte ptr [eax + 0xa4]
		mov byte ptr [edx], cl
		mov ecx, dword ptr [eax + 0xa4]
		inc edx
		shr ecx, 8
		mov byte ptr [edx], cl
		mov ecx, dword ptr [eax + 0xa4]
		inc edx
		jmp L_ConvertRowIndexed4To24_5
	L_ConvertRowIndexed4To24_4:
		mov ecx, dword ptr [eax + 0x90]
		shl esi, cl
		mov ecx, dword ptr [eax + 0x8c]
		shl ebp, cl
		mov ecx, dword ptr [eax + 0x88]
		shl edi, cl
		or esi, ebp
		or esi, edi
		mov edi, dword ptr [eax + 0x74]
		or esi, edi
		inc edx
		mov ecx, esi
		mov byte ptr [edx - 1], cl
		mov ebx, ecx
		shr ebx, 8
		mov byte ptr [edx], bl
		inc edx
	L_ConvertRowIndexed4To24_5:
		mov ebx, dword ptr [esp + 0x10]
		shr ecx, 0x10
		mov byte ptr [edx], cl
		mov ecx, dword ptr [esp + 0x14]
		mov esi, dword ptr [eax + 0x44]
		add ecx, 2
		inc edx
		add ebx, 2
		cmp ecx, esi
		mov dword ptr [esp + 0x10], ebx
		mov dword ptr [esp + 0x14], ecx
		jb L_ConvertRowIndexed4To24_6
		jmp L_ConvertRowIndexed4To24_7
	L_ConvertRowIndexed4To24_1:
		mov edx, dword ptr [esp + 0x20]
	L_ConvertRowIndexed4To24_7:
		mov ecx, dword ptr [esp + 0x10]
		mov esi, dword ptr [eax + 0x44]
		cmp ecx, esi
		jae L_ConvertRowIndexed4To24_8
		mov esi, dword ptr [esp + 0x1c]
		xor ecx, ecx
		mov cl, byte ptr [esi]
		shr ecx, 4
		lea esi, [eax + ecx*4 + 0xa8]
		xor ecx, ecx
		mov cl, byte ptr [esi]
		mov edi, ecx
		mov cl, byte ptr [eax + 0x78]
		shr edi, cl
		xor ecx, ecx
		mov cl, byte ptr [esi + 1]
		mov ebp, ecx
		mov cl, byte ptr [eax + 0x7c]
		shr ebp, cl
		xor ecx, ecx
		mov cl, byte ptr [esi + 2]
		mov esi, ecx
		mov cl, byte ptr [eax + 0x80]
		shr esi, cl
		xor ecx, ecx
		mov cl, byte ptr [eax + 0x9c]
		cmp edi, ecx
		jne L_ConvertRowIndexed4To24_9
		xor ecx, ecx
		mov cl, byte ptr [eax + 0x9d]
		cmp ebp, ecx
		jne L_ConvertRowIndexed4To24_9
		xor ecx, ecx
		mov cl, byte ptr [eax + 0x9e]
		cmp esi, ecx
		jne L_ConvertRowIndexed4To24_9
		mov cl, byte ptr [eax + 0xa4]
		pop edi
		mov byte ptr [edx], cl
		mov ecx, dword ptr [eax + 0xa4]
		inc edx
		pop esi
		shr ecx, 8
		mov byte ptr [edx], cl
		mov eax, dword ptr [eax + 0xa4]
		shr eax, 0x10
		pop ebp
		mov byte ptr [edx + 1], al
		pop ebx
		add esp, 8
		ret 8
	L_ConvertRowIndexed4To24_9:
		mov ecx, dword ptr [eax + 0x90]
		mov ebx, dword ptr [eax + 0x74]
		shl esi, cl
		mov ecx, dword ptr [eax + 0x8c]
		shl ebp, cl
		mov ecx, dword ptr [eax + 0x88]
		shl edi, cl
		or esi, ebp
		or esi, edi
		pop edi
		or esi, ebx
		inc edx
		mov eax, esi
		pop esi
		mov byte ptr [edx - 1], al
		mov ecx, eax
		shr ecx, 8
		shr eax, 0x10
		mov byte ptr [edx], cl
		pop ebp
		mov byte ptr [edx + 1], al
		pop ebx
		add esp, 8
		ret 8
	L_ConvertRowIndexed4To24_0:
		cmp edx, ecx
		jbe L_ConvertRowIndexed4To24_10
		mov esi, dword ptr [esp + 0x20]
		mov ebp, dword ptr [esp + 0x1c]
		mov dword ptr [esp + 0x14], ecx
	L_ConvertRowIndexed4To24_11:
		xor edx, edx
		mov cl, byte ptr [eax + 0x80]
		mov dl, byte ptr [ebp]
		shr edx, 4
		lea edi, [eax + edx*4 + 0xa8]
		xor edx, edx
		mov dl, byte ptr [edi + 2]
		shr edx, cl
		mov ecx, dword ptr [eax + 0x90]
		shl edx, cl
		xor ecx, ecx
		mov cl, byte ptr [edi + 1]
		mov ebx, ecx
		mov cl, byte ptr [eax + 0x7c]
		shr ebx, cl
		mov ecx, dword ptr [eax + 0x8c]
		shl ebx, cl
		xor ecx, ecx
		mov cl, byte ptr [edi]
		mov edi, ecx
		mov cl, byte ptr [eax + 0x78]
		shr edi, cl
		mov ecx, dword ptr [eax + 0x88]
		or edx, ebx
		shl edi, cl
		or edx, edi
		mov byte ptr [esi], dl
		mov ecx, edx
		inc esi
		shr ecx, 8
		mov byte ptr [esi], cl
		inc esi
		shr edx, 0x10
		mov byte ptr [esi], dl
		mov dl, byte ptr [ebp]
		and dl, 0xf
		inc esi
		mov byte ptr [esp + 0x20], dl
		xor edx, edx
		mov ecx, dword ptr [esp + 0x20]
		inc ebp
		and ecx, 0xff
		mov dl, byte ptr [eax + ecx*4 + 0xaa]
		lea edi, [eax + ecx*4 + 0xa8]
		mov cl, byte ptr [eax + 0x80]
		shr edx, cl
		mov ecx, dword ptr [eax + 0x90]
		shl edx, cl
		xor ecx, ecx
		mov cl, byte ptr [edi + 1]
		mov ebx, ecx
		mov cl, byte ptr [eax + 0x7c]
		shr ebx, cl
		mov ecx, dword ptr [eax + 0x8c]
		shl ebx, cl
		xor ecx, ecx
		mov cl, byte ptr [edi]
		mov edi, ecx
		mov cl, byte ptr [eax + 0x78]
		shr edi, cl
		mov ecx, dword ptr [eax + 0x88]
		or edx, ebx
		mov ebx, dword ptr [esp + 0x10]
		add ebx, 2
		shl edi, cl
		mov dword ptr [esp + 0x10], ebx
		or edx, edi
		mov byte ptr [esi], dl
		mov ecx, edx
		inc esi
		shr ecx, 8
		mov byte ptr [esi], cl
		mov ecx, dword ptr [esp + 0x14]
		inc esi
		add ecx, 2
		shr edx, 0x10
		mov byte ptr [esi], dl
		mov edx, dword ptr [eax + 0x44]
		inc esi
		mov dword ptr [esp + 0x14], ecx
		cmp ecx, edx
		jb L_ConvertRowIndexed4To24_11
		jmp L_ConvertRowIndexed4To24_12
	L_ConvertRowIndexed4To24_10:
		mov esi, dword ptr [esp + 0x20]
		mov ebp, dword ptr [esp + 0x1c]
	L_ConvertRowIndexed4To24_12:
		mov edx, dword ptr [esp + 0x10]
		mov ecx, dword ptr [eax + 0x44]
		cmp edx, ecx
		jae L_ConvertRowIndexed4To24_8
		xor ecx, ecx
		xor edx, edx
		mov cl, byte ptr [ebp]
		shr ecx, 4
		mov dl, byte ptr [eax + ecx*4 + 0xaa]
		lea edi, [eax + ecx*4 + 0xa8]
		mov cl, byte ptr [eax + 0x80]
		shr edx, cl
		mov ecx, dword ptr [eax + 0x90]
		shl edx, cl
		xor ecx, ecx
		mov cl, byte ptr [edi + 1]
		mov ebx, ecx
		mov cl, byte ptr [eax + 0x7c]
		shr ebx, cl
		mov ecx, dword ptr [eax + 0x8c]
		shl ebx, cl
		xor ecx, ecx
		mov cl, byte ptr [edi]
		mov edi, ecx
		mov cl, byte ptr [eax + 0x78]
		shr edi, cl
		mov ecx, dword ptr [eax + 0x88]
		or edx, ebx
		shl edi, cl
		or edx, edi
		mov byte ptr [esi], dl
		mov eax, edx
		inc esi
		shr eax, 8
		shr edx, 0x10
		mov byte ptr [esi], al
		mov byte ptr [esi + 1], dl
	L_ConvertRowIndexed4To24_8:
		pop edi
		pop esi
		pop ebp
		pop ebx
		add esp, 8
		ret 8
	}
}

#endif

// FUNCTION: GOLDP 0x10021e10
// FUNCTION: LEGORACERS 0x0040c830
void GolImgFile::ConvertRowIndexed4To32(const LegoU8* p_src, LegoU32* p_dst)
{
	LegoU32* endDst = p_dst + m_width;
	LegoU8 index;

	if (m_hasColorKey) {
		while (p_dst + 1 < endDst) {
			const ColorRGBA* c = &m_palette[*p_src >> 4];
			LegoU32 r = c->m_red >> m_redSrcShift;
			LegoU32 g = c->m_grn >> m_grnSrcShift;
			LegoU32 b = c->m_blu >> m_bluSrcShift;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				*p_dst++ = m_colorKeyPixel;
			}
			else {
				*p_dst++ = (r << m_redDstShift) | (g << m_grnDstShift) | (b << m_bluDstShift) | m_constPixelBits;
			}

			index = *p_src & 0x0f;
			p_src++;
			c = &m_palette[index];
			r = c->m_red >> m_redSrcShift;
			g = c->m_grn >> m_grnSrcShift;
			b = c->m_blu >> m_bluSrcShift;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				*p_dst++ = m_colorKeyPixel;
			}
			else {
				*p_dst++ = (r << m_redDstShift) | (g << m_grnDstShift) | (b << m_bluDstShift) | m_constPixelBits;
			}
		}
		if (p_dst < endDst) {
			const ColorRGBA* c = &m_palette[*p_src >> 4];
			LegoU32 r = c->m_red >> m_redSrcShift;
			LegoU32 g = c->m_grn >> m_grnSrcShift;
			LegoU32 b = c->m_blu >> m_bluSrcShift;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				p_dst[0] = m_colorKeyPixel;
			}
			else {
				p_dst[0] = (r << m_redDstShift) | (g << m_grnDstShift) | (b << m_bluDstShift) | m_constPixelBits;
			}
		}
	}
	else {
		while (p_dst + 1 < endDst) {
			const ColorRGBA* c = &m_palette[*p_src >> 4];
			*p_dst++ = ((c->m_red >> m_redSrcShift) << m_redDstShift) | ((c->m_grn >> m_grnSrcShift) << m_grnDstShift) |
					   ((c->m_blu >> m_bluSrcShift) << m_bluDstShift);

			index = *p_src & 0x0f;
			p_src++;
			c = &m_palette[index];
			*p_dst++ = ((c->m_red >> m_redSrcShift) << m_redDstShift) | ((c->m_grn >> m_grnSrcShift) << m_grnDstShift) |
					   ((c->m_blu >> m_bluSrcShift) << m_bluDstShift);
		}
		if (p_dst < endDst) {
			const ColorRGBA* c = &m_palette[*p_src >> 4];
			p_dst[0] = ((c->m_red >> m_redSrcShift) << m_redDstShift) | ((c->m_grn >> m_grnSrcShift) << m_grnDstShift) |
					   ((c->m_blu >> m_bluSrcShift) << m_bluDstShift);
		}
	}
}

// FUNCTION: GOLDP 0x10022180
// FUNCTION: LEGORACERS 0x0040cba0
void GolImgFile::ConvertRowIndexed8To8(const LegoU8* p_src, LegoU8* p_dst)
{
	LegoU8* endDst = p_dst + m_width;
	if (m_hasColorKey) {
		for (; p_dst < endDst; p_dst++) {
			const ColorRGBA* c = &m_palette[*p_src++];
			LegoU32 r = c->m_red >> m_redSrcShift;
			LegoU32 g = c->m_grn >> m_grnSrcShift;
			LegoU32 b = c->m_blu >> m_bluSrcShift;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				*p_dst = m_colorKeyPixel;
			}
			else {
				b <<= m_bluDstShift;
				g <<= m_grnDstShift;
				r <<= m_redDstShift;
				LegoU32 pixel = b | g;
				pixel |= r;
				pixel |= m_constPixelBits;
				*p_dst = pixel;
			}
		}
	}
	else {
		for (; p_dst < endDst; p_dst++) {
			const ColorRGBA* c = &m_palette[*p_src++];
			*p_dst = ((c->m_red >> m_redSrcShift) << m_redDstShift) | ((c->m_blu >> m_bluSrcShift) << m_bluDstShift) |
					 ((c->m_grn >> m_grnSrcShift) << m_grnDstShift);
		}
	}
}

// FUNCTION: GOLDP 0x100222c0
// FUNCTION: LEGORACERS 0x0040cce0
void GolImgFile::ConvertRowIndexed8To16(const LegoU8* p_src, LegoU16* p_dst)
{
	LegoU16* endDst = p_dst + m_width;
	if (m_hasColorKey) {
		for (; p_dst < endDst; p_dst++) {
			const ColorRGBA* c = &m_palette[*p_src++];
			LegoU32 r = c->m_red >> m_redSrcShift;
			LegoU32 g = c->m_grn >> m_grnSrcShift;
			LegoU32 b = c->m_blu >> m_bluSrcShift;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				*p_dst = m_colorKeyPixel;
			}
			else {
				b <<= m_bluDstShift;
				g <<= m_grnDstShift;
				r <<= m_redDstShift;
				LegoU32 pixel = b | g;
				pixel |= r;
				pixel |= m_constPixelBits;
				*p_dst = pixel;
			}
		}
	}
	else {
		for (; p_dst < endDst; p_dst++) {
			const ColorRGBA* c = &m_palette[*p_src++];
			LegoU32 b = c->m_blu >> m_bluSrcShift;
			b <<= m_bluDstShift;
			LegoU32 g = c->m_grn >> m_grnSrcShift;
			g <<= m_grnDstShift;
			LegoU32 r = c->m_red >> m_redSrcShift;
			r <<= m_redDstShift;
			LegoU32 pixel = b | g;
			pixel |= r;
			*p_dst = pixel;
		}
	}
}

// FUNCTION: GOLDP 0x10022400
// FUNCTION: LEGORACERS 0x0040ce20
void GolImgFile::ConvertRowIndexed8To24(const LegoU8* p_src, LegoU8* p_dst)
{
	const LegoU8* endSrc = p_src + m_width;

	if (m_hasColorKey) {
		for (; p_src < endSrc;) {
			const ColorRGBA* c = &m_palette[*p_src++];
			LegoU32 r = c->m_red >> m_redSrcShift;
			LegoU32 g = c->m_grn >> m_grnSrcShift;
			LegoU32 b = c->m_blu >> m_bluSrcShift;
			LegoU32 pixel;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				*p_dst++ = m_colorKeyPixel;
				*p_dst++ = m_colorKeyPixel >> 8;
				*p_dst++ = m_colorKeyPixel >> 16;
			}
			else {
				pixel = (r << m_redDstShift) | (g << m_grnDstShift) | (b << m_bluDstShift) | m_constPixelBits;
				*p_dst++ = pixel;
				*p_dst++ = pixel >> 8;
				*p_dst++ = pixel >> 16;
			}
		}
	}
	else {
		for (; p_src < endSrc;) {
			const ColorRGBA* c = &m_palette[*p_src++];
			LegoU32 pixel = ((c->m_red >> m_redSrcShift) << m_redDstShift) |
							((c->m_grn >> m_grnSrcShift) << m_grnDstShift) |
							((c->m_blu >> m_bluSrcShift) << m_bluDstShift);
			*p_dst++ = pixel;
			*p_dst++ = pixel >> 8;
			*p_dst++ = pixel >> 16;
		}
	}
}

#ifdef COMPAT_MODE

void GolImgFile::ConvertRowIndexed8To32(const LegoU8* p_src, LegoU32* p_dst)
{
	LegoU32* endDst = p_dst + m_width;
	if (m_hasColorKey) {
		for (; p_dst < endDst; p_dst++) {
			const ColorRGBA* c = &m_palette[*p_src++];
			LegoU32 r = c->m_red >> m_redSrcShift;
			LegoU32 g = c->m_grn >> m_grnSrcShift;
			LegoU32 b = c->m_blu >> m_bluSrcShift;
			if (r == m_colorKeyRed && g == m_colorKeyGrn && b == m_colorKeyBlu) {
				*p_dst = m_colorKeyPixel;
			}
			else {
				b <<= m_bluDstShift;
				g <<= m_grnDstShift;
				r <<= m_redDstShift;
				LegoU32 pixel = b | g;
				pixel |= r;
				pixel |= m_constPixelBits;
				*p_dst = pixel;
			}
		}
	}
	else {
		for (; p_dst < endDst; p_dst++) {
			const ColorRGBA* c = &m_palette[*p_src++];
			LegoU32 b = c->m_blu >> m_bluSrcShift;
			b <<= m_bluDstShift;
			LegoU32 g = c->m_grn >> m_grnSrcShift;
			g <<= m_grnDstShift;
			LegoU32 r = c->m_red >> m_redSrcShift;
			r <<= m_redDstShift;
			LegoU32 pixel = b | g;
			pixel |= r;
			*p_dst = pixel;
		}
	}
}

#else

// FUNCTION: GOLDP 0x10022560
// FUNCTION: LEGORACERS 0x0040cf80
__declspec(naked) void GolImgFile::ConvertRowIndexed8To32(const LegoU8* p_src, LegoU32* p_dst)
{
	__asm {
		mov eax, ecx
		push ebx
		push ebp
		mov ebp, dword ptr [esp + 0x10]
		mov ecx, dword ptr [eax + 0x44]
		mov edx, dword ptr [eax + 0x98]
		push esi
		push edi
		lea ecx, [ebp + ecx*4]
		test edx, edx
		mov dword ptr [esp + 0x18], ecx
		je L_ConvertRowIndexed8To32_0
		cmp ebp, ecx
		jae L_ConvertRowIndexed8To32_1
		mov ebx, dword ptr [esp + 0x14]
	L_ConvertRowIndexed8To32_4:
		xor edx, edx
		xor ecx, ecx
		mov dl, byte ptr [ebx]
		inc ebx
		mov cl, byte ptr [eax + edx*4 + 0xa8]
		lea edx, [eax + edx*4 + 0xa8]
		mov esi, ecx
		mov cl, byte ptr [eax + 0x78]
		shr esi, cl
		xor ecx, ecx
		mov cl, byte ptr [edx + 1]
		mov edi, ecx
		mov cl, byte ptr [eax + 0x7c]
		shr edi, cl
		xor ecx, ecx
		mov cl, byte ptr [edx + 2]
		mov edx, ecx
		mov cl, byte ptr [eax + 0x80]
		shr edx, cl
		xor ecx, ecx
		mov cl, byte ptr [eax + 0x9c]
		cmp esi, ecx
		jne L_ConvertRowIndexed8To32_2
		xor ecx, ecx
		mov cl, byte ptr [eax + 0x9d]
		cmp edi, ecx
		jne L_ConvertRowIndexed8To32_2
		xor ecx, ecx
		mov cl, byte ptr [eax + 0x9e]
		cmp edx, ecx
		jne L_ConvertRowIndexed8To32_2
		mov edx, dword ptr [eax + 0xa4]
		jmp L_ConvertRowIndexed8To32_3
	L_ConvertRowIndexed8To32_2:
		mov ecx, dword ptr [eax + 0x90]
		shl edx, cl
		mov ecx, dword ptr [eax + 0x8c]
		shl edi, cl
		mov ecx, dword ptr [eax + 0x88]
		shl esi, cl
		or edx, edi
		or edx, esi
		mov esi, dword ptr [eax + 0x74]
		or edx, esi
	L_ConvertRowIndexed8To32_3:
		mov ecx, dword ptr [esp + 0x18]
		mov dword ptr [ebp], edx
		add ebp, 4
		cmp ebp, ecx
		jb L_ConvertRowIndexed8To32_4
		pop edi
		pop esi
		pop ebp
		pop ebx
		ret 8
	L_ConvertRowIndexed8To32_0:
		cmp ebp, ecx
		jae L_ConvertRowIndexed8To32_1
		mov esi, dword ptr [esp + 0x14]
	L_ConvertRowIndexed8To32_5:
		xor ecx, ecx
		add ebp, 4
		mov cl, byte ptr [esi]
		inc esi
		lea edx, [eax + ecx*4 + 0xa8]
		xor ecx, ecx
		mov cl, byte ptr [edx + 2]
		mov edi, ecx
		mov cl, byte ptr [eax + 0x80]
		shr edi, cl
		mov ecx, dword ptr [eax + 0x90]
		shl edi, cl
		xor ecx, ecx
		mov cl, byte ptr [edx + 1]
		mov ebx, ecx
		mov cl, byte ptr [eax + 0x7c]
		shr ebx, cl
		mov ecx, dword ptr [eax + 0x8c]
		shl ebx, cl
		xor ecx, ecx
		mov cl, byte ptr [edx]
		mov edx, ecx
		mov cl, byte ptr [eax + 0x78]
		shr edx, cl
		mov ecx, dword ptr [eax + 0x88]
		or edi, ebx
		shl edx, cl
		mov ecx, dword ptr [esp + 0x18]
		or edi, edx
		cmp ebp, ecx
		mov dword ptr [ebp - 4], edi
		jb L_ConvertRowIndexed8To32_5
	L_ConvertRowIndexed8To32_1:
		pop edi
		pop esi
		pop ebp
		pop ebx
		ret 8
	}
}

#endif

// FUNCTION: GOLDP 0x100226a0
// FUNCTION: LEGORACERS 0x0040d0c0
const LegoChar* GolImgFile::GetSuffix()
{
	return ".img";
}

void GolImgFile::LoadSurfaceFromBuffer(LegoU8*, GolSurface*, LegoU32, ColorRGBA*)
{
	// empty
}

#ifdef COMPAT_MODE

void GolImgFile::SetImageInfo(
	const GolSurfaceFormat& p_format,
	LegoU32 p_width,
	LegoU32 p_height,
	LegoU32 p_rowByteStride,
	ColorRGBA* p_paletteColors,
	LegoU32 p_paletteSize
)
{
	if (m_file.IsOpen()) {
		Destroy();
	}

	m_format = p_format;
	m_width = p_width;
	m_height = p_height;
	m_rowByteStride = p_rowByteStride;
	m_paletteSize = 1 << m_format.m_bitsPerPixel;
	m_keepNibbleOrder = 0;
	m_remapPureBlack = 0;

	if (p_paletteColors != NULL && p_paletteSize != 0) {
		::memcpy(m_palette, p_paletteColors, p_paletteSize * sizeof(*p_paletteColors));
	}
}

#else

// FUNCTION: GOLDP 0x100226c0
__declspec(naked) void GolImgFile::SetImageInfo(
	const GolSurfaceFormat& p_format,
	LegoU32 p_width,
	LegoU32 p_height,
	LegoU32 p_rowByteStride,
	ColorRGBA* p_paletteColors,
	LegoU32 p_paletteSize
)
{
	__asm {
		push ebx
		mov ebx, ecx
		push esi
		push edi
		test byte ptr [ebx + 0x1c], 1
		je L_SetImageInfo_0
		mov eax, dword ptr [ebx]
		call dword ptr [eax + 0xc]
	L_SetImageInfo_0:
		mov esi, dword ptr [esp + 0x10]
		mov edx, dword ptr [esp + 0x18]
		mov eax, dword ptr [esp + 0x1c]
		lea edi, [ebx + 0x50]
		mov ecx, 6
		rep movsd
		mov ecx, dword ptr [esp + 0x14]
		mov esi, dword ptr [esp + 0x20]
		mov dword ptr [ebx + 0x44], ecx
		mov dword ptr [ebx + 0x48], edx
		mov dword ptr [ebx + 0x4c], eax
		mov cl, byte ptr [ebx + 0x66]
		mov edx, 1
		xor eax, eax
		shl edx, cl
		cmp esi, eax
		mov dword ptr [ebx + 0x5a8], eax
		mov dword ptr [ebx + 0x5ac], eax
		mov dword ptr [ebx + 0x6c], edx
		je L_SetImageInfo_1
		mov ecx, dword ptr [esp + 0x24]
		cmp ecx, eax
		je L_SetImageInfo_1
		lea edi, [ebx + 0xa8]
		rep movsd
	L_SetImageInfo_1:
		pop edi
		pop esi
		pop ebx
		ret 0x18
	}
}

#endif

// FUNCTION: GOLDP 0x10022730
void GolImgFile::ConvertImage(
	LegoU8* p_src,
	LegoU8* p_dst,
	LegoU32 p_width,
	LegoU32 p_height,
	LegoU32 p_pitch,
	const GolSurfaceFormat& p_format,
	GolPaletteBase* p_palette,
	undefined4 p_flipVertical,
	ColorRGBA* p_colorKey
)
{
	LegoU32 xScale = 1;
	LegoU32 yScale = 1;
	LegoU32 height = p_height;
	LegoU32 width = p_width;

	if (m_height != height || m_width != width) {
		xScale = width / m_width;
		yScale = height / m_height;
	}

	if (m_height > height || m_width > width) {
		GOL_FATALERROR_MESSAGE("Invalid image size for given storage");
	}

	SetupPixelConversion(p_format, p_colorKey);
	if (p_format.m_paletteMask != 0 && p_palette != NULL) {
		BuildPaletteRemap(p_palette, p_colorKey);
	}

	LegoS32 pitch;
	if (p_flipVertical != 0) {
		pitch = p_pitch;
		p_dst += (height - 1) * pitch;
		pitch = -pitch;
	}
	else {
		pitch = p_pitch;
	}

	LegoU8* src = p_src;
	for (LegoU32 y = 0; y < m_height; y++) {
		ConvertRow(src, p_dst, p_format);

		if (xScale > 1) {
			UpscaleRow(p_dst, xScale, width, p_format.m_bitsPerPixel);
		}

		for (LegoU32 repeat = 1; repeat < yScale; repeat++) {
			::memcpy(p_dst + pitch, p_dst, pitch);
			p_dst += pitch;
		}

		src += m_rowByteStride;
		p_dst += pitch;
	}
}

// FUNCTION: GOLDP 0x10022880
void GolImgFile::ConvertImageHalfSize(
	LegoU8* p_src,
	LegoU8* p_dst,
	LegoU32 p_width,
	LegoU32 p_height,
	LegoU32 p_pitch,
	const GolSurfaceFormat& p_format,
	undefined4,
	ColorRGBA* p_colorKey
)
{
	if ((m_height >> 1) != p_height || (m_width >> 1) != p_width) {
		GOL_FATALERROR_MESSAGE("Invalid image size for given storage");
	}

	SetupPixelConversion(p_format, p_colorKey);

	LegoU32 rows = m_height >> 1;
	LegoU32 srcPitch = m_rowByteStride << 1;

	if (p_format.m_paletteMask != 0) {
		for (; rows != 0; rows--) {
			LegoU8* end = p_dst + (m_width >> 1);
			if (p_dst < end) {
				LegoU8* dst = p_dst - 1;
				LegoU8* bottom = p_src + m_rowByteStride - 1;
				LegoU8* top = p_src - 1;

				for (; dst + 1 < end;) {
					top += 2;
					LegoU8 topLeft = top[-1];
					LegoU8 topRight = top[0];
					bottom += 2;
					dst++;

					if (topLeft == topRight) {
						*dst = topRight;
					}
					else {
						LegoU8 bottomLeft = bottom[-1];
						if (bottomLeft != topLeft && bottomLeft != topRight) {
							*dst = bottom[0];
						}
						else {
							*dst = bottomLeft;
						}
					}
				}
			}

			p_src += srcPitch;
			p_dst += p_pitch;
		}
	}
	else {
		for (; rows != 0; rows--) {
			DownsampleRowPair(p_src, p_src + m_rowByteStride, p_dst);
			p_src += srcPitch;
			p_dst += p_pitch;
		}
	}
}

// FUNCTION: GOLDP 0x100229b0
// FUNCTION: LEGORACERS 0x0040d0d0
void GolImgFile::UpscaleRow(LegoU8* p_row, LegoS32 p_xScale, LegoU32 p_scaledWidth, LegoU32 p_bitsPerPixel)
{
	LegoS32 x;
	LegoS32 i;

	switch (p_bitsPerPixel) {
	case 4: {
		LegoU32 width = m_width;
		LegoU8* dst = p_row + (p_scaledWidth >> 1) - 1;
		LegoU8* src = p_row + (width >> 1) - 1;
		LegoU32 dstNibble = 1;
		for (x = width - 1; x >= 0; x--) {
			LegoU32 pixel;
			if (x & 1) {
				pixel = *src & 0x0f;
			}
			else {
				pixel = *src >> 4;
				src--;
			}

			for (i = p_xScale; i > 0; i--) {
				if (dstNibble & 1) {
					*dst |= pixel << 4;
				}
				else {
					*dst = pixel;
					dst--;
				}
				dstNibble++;
			}
		}
		break;
	}
	case 8: {
		LegoU8* dst = p_row + p_scaledWidth - 1;
		LegoU8* src = p_row + m_width - 1;
		for (x = m_width; x > 0; x--) {
			for (i = p_xScale; i > 0; i--) {
				*dst-- = *src;
			}
			src--;
		}
		break;
	}
	case 15:
	case 16: {
		LegoU16* dst = reinterpret_cast<LegoU16*>(p_row) + p_scaledWidth - 1;
		LegoU16* src = reinterpret_cast<LegoU16*>(p_row) + m_width - 1;
		for (x = m_width; x > 0; x--) {
			for (i = p_xScale; i > 0; i--) {
				*dst-- = *src;
			}
			src--;
		}
		break;
	}
	case 24: {
		LegoU8* dst = p_row + p_scaledWidth * 3 - 3;
		LegoU8* src = p_row + m_width * 3 - 3;
		for (x = m_width; x > 0; x--) {
			for (i = p_xScale; i > 0; i--) {
				dst[0] = src[0];
				dst[1] = src[1];
				dst[2] = src[2];
				dst -= 3;
			}
			src -= 3;
		}
		break;
	}
	case 32: {
		LegoU32* dst = reinterpret_cast<LegoU32*>(p_row) + p_scaledWidth - 1;
		LegoU32* src = reinterpret_cast<LegoU32*>(p_row) + m_width - 1;
		for (x = m_width; x > 0; x--) {
			for (i = p_xScale; i > 0; i--) {
				*dst-- = *src;
			}
			src--;
		}
		break;
	}
	}
}

#ifdef COMPAT_MODE

void GolImgFile::DownsampleRowPair(LegoU8* p_top, LegoU8* p_bottom, LegoU8* p_dst)
{
	LegoU8* end = p_dst + (m_width >> 1) * sizeof(LegoU16);
	for (; p_dst < end; p_dst += sizeof(LegoU16)) {
		LegoU32 samples[4];
		LegoU8* src = p_top;

		samples[0] = *src;
		src += (m_srcStrideMask >> 0) & 1;
		samples[0] |= *src << 8;
		src += (m_srcStrideMask >> 1) & 1;
		samples[0] |= *src << 16;
		src += (m_srcStrideMask >> 2) & 1;
		samples[0] |= *src << 24;
		src += (m_srcStrideMask >> 3) & 1;

		samples[1] = *src;
		src += (m_srcStrideMask >> 0) & 1;
		samples[1] |= *src << 8;
		src += (m_srcStrideMask >> 1) & 1;
		samples[1] |= *src << 16;
		src += (m_srcStrideMask >> 2) & 1;
		samples[1] |= *src << 24;
		src += (m_srcStrideMask >> 3) & 1;

		p_top = src;
		src = p_bottom;

		samples[2] = *src;
		src += (m_srcStrideMask >> 0) & 1;
		samples[2] |= *src << 8;
		src += (m_srcStrideMask >> 1) & 1;
		samples[2] |= *src << 16;
		src += (m_srcStrideMask >> 2) & 1;
		samples[2] |= *src << 24;
		src += (m_srcStrideMask >> 3) & 1;

		samples[3] = *src;
		src += (m_srcStrideMask >> 0) & 1;
		samples[3] |= *src << 8;
		src += (m_srcStrideMask >> 1) & 1;
		samples[3] |= *src << 16;
		src += (m_srcStrideMask >> 2) & 1;
		samples[3] |= *src << 24;
		src += (m_srcStrideMask >> 3) & 1;

		p_bottom = src;

		LegoU32 red = 0;
		LegoU32 grn = 0;
		LegoU32 blu = 0;

		if (m_hasColorKey) {
			LegoBool32 foundColorKey = FALSE;
			for (LegoU32 i = 0; i < 4; i++) {
				LegoU32 sampleRed = (samples[i] & m_format.m_redBitMask) >> m_redSrcShift;
				LegoU32 sampleGrn = (samples[i] & m_format.m_grnBitMask) >> m_grnSrcShift;
				LegoU32 sampleBlu = (samples[i] & m_format.m_bluBitMask) >> m_bluSrcShift;

				if (sampleRed == m_colorKeyRed && sampleGrn == m_colorKeyGrn && sampleBlu == m_colorKeyBlu) {
					*reinterpret_cast<LegoU16*>(p_dst) = static_cast<LegoU16>(m_colorKeyPixel);
					foundColorKey = TRUE;
					break;
				}

				red += sampleRed;
				grn += sampleGrn;
				blu += sampleBlu;
			}

			if (!foundColorKey) {
				LegoU32 pixel = ((red >> 2) << m_redDstShift) | ((grn >> 2) << m_grnDstShift) |
								((blu >> 2) << m_bluDstShift) | m_constPixelBits;
				*reinterpret_cast<LegoU16*>(p_dst) = static_cast<LegoU16>(pixel);
			}
		}
		else {
			LegoU32 alp = 0;
			for (LegoU32 i = 0; i < 4; i++) {
				red += (samples[i] & m_format.m_redBitMask) >> m_redSrcShift;
				grn += (samples[i] & m_format.m_grnBitMask) >> m_grnSrcShift;
				blu += (samples[i] & m_format.m_bluBitMask) >> m_bluSrcShift;
				alp += (samples[i] & m_format.m_alpBitMask) >> m_alpSrcShift;
			}

			LegoU32 pixel = ((red >> 2) << m_redDstShift) | ((grn >> 2) << m_grnDstShift) |
							((blu >> 2) << m_bluDstShift) | ((alp >> 2) << m_alpDstShift);
			*reinterpret_cast<LegoU16*>(p_dst) = static_cast<LegoU16>(pixel);
		}
	}
}

#else

// FUNCTION: GOLDP 0x10022b80
__declspec(naked) void GolImgFile::DownsampleRowPair(LegoU8* p_top, LegoU8* p_bottom, LegoU8* p_dst)
{
	__asm {
		sub esp, 0x24
		mov eax, ecx
		push ebx
		mov ecx, dword ptr [esp + 0x34]
		push ebp
		mov edx, dword ptr [eax + 0x44]
		push esi
		mov esi, dword ptr [eax + 0x98]
		push edi
		shr edx, 1
		test esi, esi
		lea edx, [ecx + edx*2]
		mov dword ptr [esp + 0x1c], edx
		je L_DownsampleRowPair_0
		cmp ecx, edx
		jae L_DownsampleRowPair_1
		mov esi, dword ptr [esp + 0x3c]
		mov edx, dword ptr [esp + 0x38]
	L_DownsampleRowPair_8:
		mov ebp, dword ptr [eax + 0x70]
		xor ecx, ecx
		mov cl, byte ptr [edx]
		mov edi, ebp
		and edi, 1
		xor ebx, ebx
		mov dword ptr [esp + 0x3c], edi
		add edx, edi
		mov edi, ebp
		mov dword ptr [esp + 0x38], ebp
		mov bh, byte ptr [edx]
		shr edi, 1
		and edi, 1
		or ecx, ebx
		add edx, edi
		xor ebx, ebx
		shr ebp, 2
		mov bl, byte ptr [edx]
		and ebp, 1
		shl ebx, 0x10
		or ecx, ebx
		add edx, ebp
		xor ebx, ebx
		mov bl, byte ptr [edx]
		shl ebx, 0x18
		or ecx, ebx
		mov ebx, dword ptr [esp + 0x3c]
		mov dword ptr [esp + 0x24], ecx
		mov ecx, dword ptr [esp + 0x38]
		shr ecx, 3
		and ecx, 1
		mov dword ptr [esp + 0x38], ecx
		add edx, ecx
		xor ecx, ecx
		mov cl, byte ptr [edx]
		add edx, ebx
		xor ebx, ebx
		mov bh, byte ptr [edx]
		add edx, edi
		or ecx, ebx
		xor ebx, ebx
		mov bl, byte ptr [edx]
		add edx, ebp
		shl ebx, 0x10
		or ecx, ebx
		xor ebx, ebx
		mov bl, byte ptr [edx]
		shl ebx, 0x18
		or ecx, ebx
		mov ebx, dword ptr [esp + 0x3c]
		mov dword ptr [esp + 0x28], ecx
		mov ecx, dword ptr [esp + 0x38]
		add edx, ecx
		xor ecx, ecx
		mov cl, byte ptr [esi]
		add esi, ebx
		xor ebx, ebx
		mov bh, byte ptr [esi]
		add esi, edi
		or ecx, ebx
		xor ebx, ebx
		mov bl, byte ptr [esi]
		add esi, ebp
		shl ebx, 0x10
		or ecx, ebx
		xor ebx, ebx
		mov bl, byte ptr [esi]
		shl ebx, 0x18
		or ecx, ebx
		mov ebx, dword ptr [esp + 0x3c]
		mov dword ptr [esp + 0x2c], ecx
		mov ecx, dword ptr [esp + 0x38]
		add esi, ecx
		xor ecx, ecx
		mov cl, byte ptr [esi]
		add esi, ebx
		xor ebx, ebx
		mov bh, byte ptr [esi]
		add esi, edi
		or ecx, ebx
		xor ebx, ebx
		mov bl, byte ptr [esi]
		add esi, ebp
		shl ebx, 0x10
		or ecx, ebx
		xor ebx, ebx
		mov bl, byte ptr [esi]
		shl ebx, 0x18
		or ecx, ebx
		mov dword ptr [esp + 0x30], ecx
		mov ecx, dword ptr [esp + 0x38]
		add esi, ecx
		xor ecx, ecx
		mov dword ptr [esp + 0x38], ecx
		mov dword ptr [esp + 0x3c], ecx
		mov dword ptr [esp + 0x10], ecx
		mov dword ptr [esp + 0x18], ecx
		mov cl, byte ptr [eax + 0x9c]
		mov dword ptr [esp + 0x20], ecx
		lea ecx, [esp + 0x24]
		mov dword ptr [esp + 0x14], ecx
	L_DownsampleRowPair_4:
		mov ecx, dword ptr [esp + 0x14]
		mov edi, dword ptr [ecx]
		mov ecx, dword ptr [eax + 0x50]
		mov ebp, edi
		mov ebx, edi
		and ebp, ecx
		mov ecx, dword ptr [eax + 0x78]
		shr ebp, cl
		mov ecx, dword ptr [eax + 0x54]
		and ebx, ecx
		mov ecx, dword ptr [eax + 0x7c]
		shr ebx, cl
		mov ecx, dword ptr [eax + 0x58]
		and edi, ecx
		mov ecx, dword ptr [eax + 0x80]
		shr edi, cl
		cmp ebp, dword ptr [esp + 0x20]
		jne L_DownsampleRowPair_2
		xor ecx, ecx
		mov cl, byte ptr [eax + 0x9d]
		cmp ebx, ecx
		jne L_DownsampleRowPair_2
		xor ecx, ecx
		mov cl, byte ptr [eax + 0x9e]
		cmp edi, ecx
		je L_DownsampleRowPair_3
	L_DownsampleRowPair_2:
		mov ecx, dword ptr [esp + 0x38]
		add ecx, ebp
		mov ebp, dword ptr [esp + 0x3c]
		add ebp, ebx
		mov ebx, dword ptr [esp + 0x10]
		mov dword ptr [esp + 0x38], ecx
		mov ecx, dword ptr [esp + 0x18]
		add ebx, edi
		mov edi, dword ptr [esp + 0x14]
		inc ecx
		add edi, 4
		cmp ecx, 4
		mov dword ptr [esp + 0x3c], ebp
		mov dword ptr [esp + 0x10], ebx
		mov dword ptr [esp + 0x18], ecx
		mov dword ptr [esp + 0x14], edi
		jb L_DownsampleRowPair_4
		jmp L_DownsampleRowPair_5
	L_DownsampleRowPair_3:
		cmp dword ptr [esp + 0x18], 4
		jae L_DownsampleRowPair_6
		mov ecx, dword ptr [esp + 0x40]
		mov di, word ptr [eax + 0xa4]
		mov word ptr [ecx], di
		jmp L_DownsampleRowPair_7
	L_DownsampleRowPair_6:
		mov ebx, dword ptr [esp + 0x10]
		mov ebp, dword ptr [esp + 0x3c]
	L_DownsampleRowPair_5:
		mov ecx, dword ptr [eax + 0x90]
		mov edi, dword ptr [esp + 0x38]
		shr ebx, 2
		shl ebx, cl
		mov ecx, dword ptr [eax + 0x8c]
		shr ebp, 2
		shl ebp, cl
		mov ecx, dword ptr [eax + 0x88]
		shr edi, 2
		shl edi, cl
		mov ecx, dword ptr [esp + 0x40]
		or ebx, ebp
		mov ebp, dword ptr [eax + 0x74]
		or ebx, edi
		or ebx, ebp
		mov word ptr [ecx], bx
	L_DownsampleRowPair_7:
		mov edi, dword ptr [esp + 0x1c]
		add ecx, 2
		cmp ecx, edi
		mov dword ptr [esp + 0x40], ecx
		jb L_DownsampleRowPair_8
		pop edi
		pop esi
		pop ebp
		pop ebx
		add esp, 0x24
		ret 0xc
	L_DownsampleRowPair_0:
		cmp ecx, edx
		jae L_DownsampleRowPair_1
		mov esi, dword ptr [esp + 0x3c]
		mov edx, dword ptr [esp + 0x38]
	L_DownsampleRowPair_10:
		mov ebp, dword ptr [eax + 0x70]
		xor ecx, ecx
		mov cl, byte ptr [edx]
		mov edi, ebp
		and edi, 1
		xor ebx, ebx
		mov dword ptr [esp + 0x3c], edi
		add edx, edi
		mov edi, ebp
		mov dword ptr [esp + 0x38], ebp
		mov bh, byte ptr [edx]
		shr edi, 1
		and edi, 1
		or ecx, ebx
		add edx, edi
		xor ebx, ebx
		shr ebp, 2
		mov bl, byte ptr [edx]
		and ebp, 1
		shl ebx, 0x10
		or ecx, ebx
		add edx, ebp
		xor ebx, ebx
		mov bl, byte ptr [edx]
		shl ebx, 0x18
		or ecx, ebx
		mov ebx, dword ptr [esp + 0x3c]
		mov dword ptr [esp + 0x24], ecx
		mov ecx, dword ptr [esp + 0x38]
		shr ecx, 3
		and ecx, 1
		mov dword ptr [esp + 0x38], ecx
		add edx, ecx
		xor ecx, ecx
		mov cl, byte ptr [edx]
		add edx, ebx
		xor ebx, ebx
		mov bh, byte ptr [edx]
		add edx, edi
		or ecx, ebx
		xor ebx, ebx
		mov bl, byte ptr [edx]
		add edx, ebp
		shl ebx, 0x10
		or ecx, ebx
		xor ebx, ebx
		mov bl, byte ptr [edx]
		shl ebx, 0x18
		or ecx, ebx
		mov ebx, dword ptr [esp + 0x3c]
		mov dword ptr [esp + 0x28], ecx
		mov ecx, dword ptr [esp + 0x38]
		add edx, ecx
		xor ecx, ecx
		mov cl, byte ptr [esi]
		add esi, ebx
		xor ebx, ebx
		mov bh, byte ptr [esi]
		add esi, edi
		or ecx, ebx
		xor ebx, ebx
		mov bl, byte ptr [esi]
		add esi, ebp
		shl ebx, 0x10
		or ecx, ebx
		xor ebx, ebx
		mov bl, byte ptr [esi]
		shl ebx, 0x18
		or ecx, ebx
		mov ebx, dword ptr [esp + 0x3c]
		mov dword ptr [esp + 0x2c], ecx
		mov ecx, dword ptr [esp + 0x38]
		add esi, ecx
		xor ecx, ecx
		mov cl, byte ptr [esi]
		add esi, ebx
		xor ebx, ebx
		mov bh, byte ptr [esi]
		add esi, edi
		or ecx, ebx
		xor ebx, ebx
		mov bl, byte ptr [esi]
		add esi, ebp
		shl ebx, 0x10
		or ecx, ebx
		xor ebx, ebx
		mov bl, byte ptr [esi]
		mov dword ptr [esp + 0x14], 4
		shl ebx, 0x18
		or ecx, ebx
		mov dword ptr [esp + 0x30], ecx
		mov ecx, dword ptr [esp + 0x38]
		add esi, ecx
		xor ebx, ebx
		lea ecx, [esp + 0x24]
		mov dword ptr [esp + 0x38], ebx
		mov dword ptr [esp + 0x3c], ebx
		mov dword ptr [esp + 0x10], ebx
		mov dword ptr [esp + 0x18], ecx
	L_DownsampleRowPair_9:
		mov ecx, dword ptr [esp + 0x18]
		mov edi, dword ptr [ecx]
		mov ecx, dword ptr [eax + 0x50]
		mov ebp, edi
		and ebp, ecx
		mov ecx, dword ptr [eax + 0x78]
		shr ebp, cl
		mov ecx, dword ptr [esp + 0x38]
		add ecx, ebp
		mov ebp, edi
		mov dword ptr [esp + 0x38], ecx
		mov ecx, dword ptr [eax + 0x54]
		and ebp, ecx
		mov ecx, dword ptr [eax + 0x7c]
		shr ebp, cl
		mov ecx, dword ptr [esp + 0x3c]
		add ecx, ebp
		mov ebp, edi
		mov dword ptr [esp + 0x3c], ecx
		mov ecx, dword ptr [eax + 0x58]
		and ebp, ecx
		mov ecx, dword ptr [eax + 0x80]
		shr ebp, cl
		mov ecx, ebp
		mov ebp, dword ptr [esp + 0x10]
		add ebp, ecx
		mov ecx, dword ptr [eax + 0x5c]
		and edi, ecx
		mov ecx, dword ptr [eax + 0x84]
		shr edi, cl
		mov ecx, dword ptr [esp + 0x14]
		mov dword ptr [esp + 0x10], ebp
		add ebx, edi
		mov edi, dword ptr [esp + 0x18]
		add edi, 4
		dec ecx
		mov dword ptr [esp + 0x18], edi
		mov dword ptr [esp + 0x14], ecx
		jne L_DownsampleRowPair_9
		mov ecx, dword ptr [eax + 0x94]
		mov edi, dword ptr [esp + 0x3c]
		shr ebx, 2
		shl ebx, cl
		mov ecx, dword ptr [eax + 0x90]
		shr ebp, 2
		shl ebp, cl
		mov ecx, dword ptr [eax + 0x8c]
		shr edi, 2
		shl edi, cl
		mov ecx, dword ptr [eax + 0x88]
		or ebx, ebp
		or ebx, edi
		mov edi, dword ptr [esp + 0x38]
		shr edi, 2
		shl edi, cl
		mov ecx, dword ptr [esp + 0x40]
		add ecx, 2
		mov dword ptr [esp + 0x40], ecx
		or ebx, edi
		mov edi, dword ptr [esp + 0x1c]
		mov word ptr [ecx - 2], bx
		cmp ecx, edi
		jb L_DownsampleRowPair_10
	L_DownsampleRowPair_1:
		pop edi
		pop esi
		pop ebp
		pop ebx
		add esp, 0x24
		ret 0xc
	}
}

#endif

// FUNCTION: GOLDP 0x100294f0 FOLDED
// FUNCTION: LEGORACERS 0x00416030 FOLDED
void GolImgFile::LoadTiledTexture(GolTiledTexture*, LegoU32, ColorRGBA*)
{
	// empty
}

// FUNCTION: GOLDP 0x100294f0 FOLDED
// FUNCTION: LEGORACERS 0x00416030 FOLDED
void GolImgFile::LoadSurface(GolSurface* p_texture, LegoU32 p_flags, ColorRGBA* p_colorKey)
{
	// empty
}

// FUNCTION: GOLDP 0x10029920 FOLDED
// FUNCTION: LEGORACERS 0x004164c0 FOLDED
void GolImgFile::ReadHeader()
{
	// empty
}

// FUNCTION: GOLDP 0x1002c020 FOLDED
// FUNCTION: LEGORACERS 0x004513d0 FOLDED
void GolImgFile::ReadPixels(LegoU8* p_buffer)
{
	// empty
}

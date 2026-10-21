#include "race/racetrailmanager.h"

#include "core/gol.h"
#include "decomp.h"
#include "gdbmodelindexarray.h"
#include "golerror.h"
#include "mesh/golmodel.h"
#include "render/gold3drenderdevice.h"

#include <float.h>

extern LegoFloat g_inv255;

// GLOBAL: LEGORACERS 0x004b4790
const LegoFloat g_raceTrailManagerMaxFloat = FLT_MAX;

DECOMP_SIZE_ASSERT(RaceTrailManager, 0x0c)
DECOMP_SIZE_ASSERT(RaceTrailManager::Trail, 0x2a4)
DECOMP_SIZE_ASSERT(RaceTrailManager::Trail::Sample, 0x4c)

// FUNCTION: LEGORACERS 0x004513d0 FOLDED
void RaceTrailManager::Trail::FUN_004513d0(GolD3DRenderDevice*)
{
}

// FUNCTION: LEGORACERS 0x00492930
RaceTrailManager::Trail::Trail()
{
	Reset();
}

// FUNCTION: LEGORACERS 0x00492950
RaceTrailManager::Trail::~Trail()
{
	Destroy();
}

// FUNCTION: LEGORACERS 0x00492960
void RaceTrailManager::Trail::Destroy()
{
	if (m_model) {
		m_golExport->DestroyModel(m_model);
		m_model = NULL;
	}

	m_entity.ResetModelState();
	Reset();
}

// FUNCTION: LEGORACERS 0x00492990
void RaceTrailManager::Trail::Reset()
{
	LegoU32 zero = 0;

	m_golExport = NULL;
	m_model = NULL;
	m_flags = static_cast<LegoU8>(zero);
	m_durationMs = zero;
	m_sampleMs = zero;
	m_headIndex = zero;
	m_tailIndex = zero;
	m_unk0x0b0 = zero;
	m_sampleCount = zero;
	m_ringSize = zero;
	m_pointCount = zero;
	m_unk0x0c0 = zero;
	m_centerX = 0.0f;
	m_centerY = 0.0f;
	m_centerZ = 0.0f;
	m_endScale = 0.0f;
	m_endAlpha = 0.0f;
	m_color.m_red = 0xff;
	m_color.m_grn = 0xff;
	m_color.m_blu = 0xff;
	m_color.m_alp = 0xff;

	for (LegoU32 i = 0; i < sizeOfArray(m_samples); i++) {
		m_samples[i].m_durationMs = zero;
		m_samples[i].m_center.m_x = 0.0f;
		m_samples[i].m_center.m_y = 0.0f;
		m_samples[i].m_center.m_z = 0.0f;

		for (LegoU32 j = 0; j < 5; j++) {
			m_samples[i].m_points[j].m_x = 0.0f;
			m_samples[i].m_points[j].m_y = 0.0f;
			m_samples[i].m_points[j].m_z = 0.0f;
		}
	}
}

// FUNCTION: LEGORACERS 0x00492a50
void RaceTrailManager::Trail::Initialize(GolD3DRenderDevice* p_renderer, GolExport* p_golExport)
{
	m_golExport = p_golExport;
	m_model = p_golExport->CreateModel();
	m_model->Allocate(p_renderer, 1, 0x6a, 0x35, 4, 1);
	m_entity.SetPrimaryModel(m_model, g_raceTrailManagerMaxFloat);
}

// FUNCTION: LEGORACERS 0x00492a90
void RaceTrailManager::Trail::SetMaterial(GolD3DRenderDevice*, GolMaterial* p_material)
{
	m_model->GetMaterialTable()->SetEntry(0, p_material);
}

// FUNCTION: LEGORACERS 0x00492ab0
void RaceTrailManager::Trail::SetColor(const ColorRGBA* p_color)
{
	m_color.m_red = p_color->m_red;
	m_color.m_grn = p_color->m_grn;
	m_color.m_blu = p_color->m_blu;
	m_color.m_alp = p_color->m_alp;
}

// FUNCTION: LEGORACERS 0x00492ae0
void RaceTrailManager::Trail::Start(Params* p_params)
{
	m_flags = c_active | c_firstSample;

	m_durationMs = p_params->m_durationMs;
	m_sampleCount = p_params->m_sampleCount;
	m_ringSize = m_sampleCount + 2;
	m_pointCount = p_params->m_pointCount;
	m_unk0x0c0 = p_params->m_unk0x10;
	m_endScale = p_params->m_endScale;
	m_endAlpha = p_params->m_endAlpha;

	if (p_params->m_unk0x0c != 0) {
		m_flags = c_active | c_flags0x09cBit2 | c_firstSample;
	}

	LegoU32 value = m_durationMs;
	m_sampleMs = value / m_sampleCount;
	LegoU32 alignedValue = m_sampleCount * m_sampleMs;
	if (alignedValue != value) {
		m_durationMs = alignedValue;
	}

	m_headIndex = 0;
	m_tailIndex = 0;
	m_unk0x0b0 = 0;

	for (LegoU32 i = 0; i < m_ringSize; i++) {
		m_samples[i].m_durationMs = 0;
		m_samples[i].m_center.m_x = 0.0f;
		m_samples[i].m_center.m_y = 0.0f;
		m_samples[i].m_center.m_z = 0.0f;

		for (LegoU32 j = 0; j < m_pointCount; j++) {
			m_samples[i].m_points[j].m_x = 0.0f;
			m_samples[i].m_points[j].m_y = 0.0f;
			m_samples[i].m_points[j].m_z = 0.0f;
		}
	}
}

// FUNCTION: LEGORACERS 0x00492bd0
void RaceTrailManager::Trail::Release()
{
	m_flags |= c_released;
}

// FUNCTION: LEGORACERS 0x00492be0
void RaceTrailManager::Trail::Update(LegoU32 p_elapsedMs)
{
	LegoU8 flags = m_flags;
	if (!(flags & c_active)) {
		return;
	}

	if (flags & c_firstSample) {
		return;
	}

	switch (flags & c_saturated) {
	case 0:
		if (flags & c_released) {
			flags |= c_saturated;
			m_flags = flags;
		}
		break;
	}

	if (m_flags & c_released) {
		AdvanceTail(p_elapsedMs);
		if (m_flags & c_active) {
			RebuildGeometry();
		}
	}
}

#ifdef COMPAT_MODE

void RaceTrailManager::Trail::AdvanceTail(LegoU32 p_elapsedMs)
{
	LegoU32 queuedDuration = 0;
	LegoU32 index = m_tailIndex;

	do {
		index++;
		if (index == m_ringSize) {
			index = 0;
		}

		queuedDuration += m_samples[index].m_durationMs;
	} while (index != m_headIndex);

	LegoU8 flags = m_flags;
	if (!(flags & c_saturated) && queuedDuration > m_durationMs) {
		m_flags = flags | c_saturated;
	}

	if (!(m_flags & c_saturated) || p_elapsedMs == 0) {
		return;
	}

	LegoU32 remainingMs = p_elapsedMs;
	do {
		LegoU32 stepMs = remainingMs;
		if (remainingMs > m_sampleMs) {
			stepMs = m_sampleMs;
			remainingMs -= stepMs;
		}
		else {
			remainingMs = 0;
		}

		LegoU32 currentIndex = m_tailIndex;
		LegoU32 nextIndex = (currentIndex + 1) % m_ringSize;
		if (stepMs > 0) {
			Sample* next = &m_samples[nextIndex];
			LegoU32 durationMs = next->m_durationMs;
			if (stepMs >= durationMs) {
				next->m_durationMs = 0;
				stepMs -= durationMs;
				m_tailIndex = currentIndex + 1;
				if (m_tailIndex == m_ringSize) {
					m_tailIndex = 0;
				}

				currentIndex = m_tailIndex;
				nextIndex = (currentIndex + 1) % m_ringSize;
			}

			queuedDuration = 0;
			index = currentIndex;
			do {
				index++;
				if (index == m_ringSize) {
					index = 0;
				}

				queuedDuration += m_samples[index].m_durationMs;
			} while (index != m_headIndex);

			if (stepMs > 0 && queuedDuration > 0) {
				next = &m_samples[nextIndex];
				durationMs = next->m_durationMs;
				if (stepMs > durationMs) {
					stepMs = durationMs - 1;
				}

				LegoFloat amount = static_cast<LegoFloat>(static_cast<LegoS32>(stepMs)) /
								   static_cast<LegoFloat>(static_cast<LegoS32>(durationMs));
				next->m_durationMs = durationMs - stepMs;

				Sample* current = &m_samples[m_tailIndex];
				for (LegoU32 i = 0; i < m_pointCount; i++) {
					current->m_points[i].m_x += (next->m_points[i].m_x - current->m_points[i].m_x) * amount;
					current->m_points[i].m_y += (next->m_points[i].m_y - current->m_points[i].m_y) * amount;
					current->m_points[i].m_z += (next->m_points[i].m_z - current->m_points[i].m_z) * amount;
				}

				current->m_center.m_x += (next->m_center.m_x - current->m_center.m_x) * amount;
				current->m_center.m_y += (next->m_center.m_y - current->m_center.m_y) * amount;
				current->m_center.m_z += (next->m_center.m_z - current->m_center.m_z) * amount;
			}
		}

		if (m_flags & c_released) {
			if (m_tailIndex == m_headIndex || queuedDuration == 0) {
				m_flags = 0;
				return;
			}
		}
	} while (remainingMs > 0);
}

#else

// FUNCTION: LEGORACERS 0x00492c30
__declspec(naked) void RaceTrailManager::Trail::AdvanceTail(LegoU32 p_elapsedMs)
{
	__asm {
		mov eax, dword ptr [ecx + 0xac]
		mov edx, dword ptr [ecx + 0xa8]
		sub esp, 0x30
		push ebx
		push ebp
		push esi
		mov esi, dword ptr [ecx + 0xb8]
		push edi
		mov edi, dword ptr [esp + 0x44]
		xor ebx, ebx
	L_AdvanceTail_1:
		inc eax
		cmp eax, esi
		jne L_AdvanceTail_0
		xor eax, eax
	L_AdvanceTail_0:
		lea ebp, [eax + eax*8]
		lea ebp, [eax + ebp*2]
		add ebx, dword ptr [ecx + ebp*4 + 0xdc]
		cmp eax, edx
		jne L_AdvanceTail_1
		mov al, byte ptr [ecx + 0x9c]
		test al, 0x10
		jne L_AdvanceTail_2
		cmp ebx, dword ptr [ecx + 0xa0]
		jbe L_AdvanceTail_2
		or al, 0x10
		mov byte ptr [ecx + 0x9c], al
	L_AdvanceTail_2:
		test byte ptr [ecx + 0x9c], 0x10
		je L_AdvanceTail_3
		mov eax, dword ptr [esp + 0x44]
		test eax, eax
		jbe L_AdvanceTail_3
		jmp L_AdvanceTail_4
	L_AdvanceTail_17:
		mov edi, dword ptr [esp + 0x10]
	L_AdvanceTail_4:
		mov esi, dword ptr [ecx + 0xa4]
		cmp edi, esi
		jbe L_AdvanceTail_5
		sub edi, esi
		mov dword ptr [esp + 0x44], esi
		mov dword ptr [esp + 0x10], edi
		jmp L_AdvanceTail_6
	L_AdvanceTail_5:
		mov esi, edi
		mov dword ptr [esp + 0x10], 0
		mov dword ptr [esp + 0x44], esi
	L_AdvanceTail_6:
		mov edi, dword ptr [ecx + 0xac]
		mov ebp, dword ptr [ecx + 0xb8]
		xor edx, edx
		lea eax, [edi + 1]
		div ebp
		test esi, esi
		jbe L_AdvanceTail_7
		lea eax, [edx + edx*8]
		lea eax, [edx + eax*2]
		mov ebx, dword ptr [ecx + eax*4 + 0xdc]
		lea eax, [ecx + eax*4 + 0xdc]
		cmp esi, ebx
		jb L_AdvanceTail_8
		mov dword ptr [eax], 0
		mov edi, dword ptr [ecx + 0xac]
		mov ebp, dword ptr [ecx + 0xb8]
		sub esi, ebx
		inc edi
		mov dword ptr [esp + 0x44], esi
		mov eax, edi
		mov dword ptr [ecx + 0xac], edi
		cmp eax, ebp
		jne L_AdvanceTail_9
		mov dword ptr [ecx + 0xac], 0
	L_AdvanceTail_9:
		mov edi, dword ptr [ecx + 0xac]
		xor edx, edx
		lea eax, [edi + 1]
		div ebp
	L_AdvanceTail_8:
		mov eax, edi
		xor ebx, ebx
	L_AdvanceTail_11:
		inc eax
		cmp eax, ebp
		jne L_AdvanceTail_10
		xor eax, eax
	L_AdvanceTail_10:
		lea edi, [eax + eax*8]
		lea edi, [eax + edi*2]
		add ebx, dword ptr [ecx + edi*4 + 0xdc]
		mov edi, dword ptr [ecx + 0xa8]
		cmp eax, edi
		jne L_AdvanceTail_11
		test esi, esi
		mov dword ptr [esp + 0x18], ebx
		jbe L_AdvanceTail_7
		test ebx, ebx
		jbe L_AdvanceTail_7
		lea eax, [edx + edx*8]
		lea edx, [edx + eax*2]
		mov eax, dword ptr [ecx + edx*4 + 0xdc]
		cmp esi, eax
		mov dword ptr [esp + 0x14], eax
		lea ebp, [ecx + edx*4]
		jbe L_AdvanceTail_12
		lea esi, [eax - 1]
		mov dword ptr [esp + 0x44], esi
	L_AdvanceTail_12:
		fild dword ptr [esp + 0x44]
		sub eax, esi
		xor esi, esi
		mov dword ptr [ebp + 0xdc], eax
		mov eax, dword ptr [ecx + 0xbc]
		fild dword ptr [esp + 0x14]
		test eax, eax
		fdivp st(1), st(0)
		jbe L_AdvanceTail_13
		xor edi, edi
		lea edx, [ebp + 0xe8]
	L_AdvanceTail_14:
		mov eax, dword ptr [ecx + 0xac]
		inc esi
		fld dword ptr [edx - 8]
		lea ebx, [eax + eax*8]
		add edx, 0xc
		lea eax, [eax + ebx*2]
		lea eax, [edi + eax*4]
		add edi, 0xc
		fsub dword ptr [eax + ecx + 0xe0]
		fld dword ptr [edx - 0x10]
		fsub dword ptr [eax + ecx + 0xe4]
		lea eax, [eax + ecx + 0xe0]
		fstp dword ptr [esp + 0x20]
		fld dword ptr [edx - 0xc]
		fsub dword ptr [eax + 8]
		fstp dword ptr [esp + 0x24]
		fmul st(0), st(1)
		fld dword ptr [esp + 0x20]
		fmul st(0), st(2)
		fstp dword ptr [esp + 0x2c]
		fld dword ptr [esp + 0x24]
		fmul st(0), st(2)
		fstp dword ptr [esp + 0x30]
		fadd dword ptr [eax]
		fstp dword ptr [eax]
		fld dword ptr [esp + 0x2c]
		fadd dword ptr [eax + 4]
		fstp dword ptr [eax + 4]
		fld dword ptr [esp + 0x30]
		fadd dword ptr [eax + 8]
		fstp dword ptr [eax + 8]
		cmp esi, dword ptr [ecx + 0xbc]
		jb L_AdvanceTail_14
		mov ebx, dword ptr [esp + 0x18]
	L_AdvanceTail_13:
		mov eax, dword ptr [ecx + 0xac]
		fld dword ptr [ebp + 0x11c]
		lea edx, [eax + eax*8]
		lea eax, [eax + edx*2]
		fsub dword ptr [ecx + eax*4 + 0x11c]
		fld dword ptr [ebp + 0x120]
		fsub dword ptr [ecx + eax*4 + 0x120]
		lea eax, [ecx + eax*4 + 0x11c]
		fstp dword ptr [esp + 0x20]
		fld dword ptr [ebp + 0x124]
		fsub dword ptr [eax + 8]
		fstp dword ptr [esp + 0x24]
		fmul st(0), st(1)
		fld dword ptr [esp + 0x20]
		fmul st(0), st(2)
		fstp dword ptr [esp + 0x38]
		fld dword ptr [esp + 0x24]
		fmul st(0), st(2)
		fstp dword ptr [esp + 0x3c]
		fadd dword ptr [eax]
		fstp dword ptr [eax]
		fstp st(0)
		fld dword ptr [esp + 0x38]
		fadd dword ptr [eax + 4]
		fstp dword ptr [eax + 4]
		fld dword ptr [esp + 0x3c]
		fadd dword ptr [eax + 8]
		fstp dword ptr [eax + 8]
	L_AdvanceTail_7:
		test byte ptr [ecx + 0x9c], 2
		je L_AdvanceTail_15
		mov edx, dword ptr [ecx + 0xac]
		mov eax, dword ptr [ecx + 0xa8]
		cmp edx, eax
		je L_AdvanceTail_16
		test ebx, ebx
		je L_AdvanceTail_16
	L_AdvanceTail_15:
		mov eax, dword ptr [esp + 0x10]
		test eax, eax
		ja L_AdvanceTail_17
		pop edi
		pop esi
		pop ebp
		pop ebx
		add esp, 0x30
		ret 4
	L_AdvanceTail_16:
		mov byte ptr [ecx + 0x9c], 0
	L_AdvanceTail_3:
		pop edi
		pop esi
		pop ebp
		pop ebx
		add esp, 0x30
		ret 4
	}
}

#endif

// FUNCTION: LEGORACERS 0x00492ee0
void RaceTrailManager::Trail::AddSampleWithCenter(LegoU32 p_elapsedMs, GolVec3* p_positions, GolVec3 p_center)
{
	m_centerX = p_center.m_x;
	m_centerY = p_center.m_y;
	m_centerZ = p_center.m_z;
	AddSample(p_elapsedMs, p_positions);
}

// FUNCTION: LEGORACERS 0x00492f10
void RaceTrailManager::Trail::AddSample(LegoU32 p_elapsedMs, GolVec3* p_positions)
{
	LegoU32 remainingMs = p_elapsedMs;

	if (m_flags & c_firstSample) {
		for (LegoU32 i = 0; i < m_pointCount; i++) {
			m_samples[m_headIndex].m_points[i].m_x = p_positions[i].m_x;
			m_samples[m_headIndex].m_points[i].m_y = p_positions[i].m_y;
			m_samples[m_headIndex].m_points[i].m_z = p_positions[i].m_z;
		}

		m_samples[m_headIndex].m_center.m_x = m_centerX;
		m_samples[m_headIndex].m_center.m_y = m_centerY;
		m_samples[m_headIndex].m_center.m_z = m_centerZ;
		m_headIndex++;
		m_flags &= ~c_firstSample;
		return;
	}

	if (remainingMs > 0) {
		do {
			LegoU32 stepMs = m_sampleMs - 1;
			if (remainingMs > stepMs) {
				remainingMs -= m_sampleMs;
			}
			else {
				stepMs = remainingMs;
				remainingMs = 0;
			}

			m_samples[m_headIndex].m_durationMs += stepMs;
			AdvanceTail(stepMs);

			if (m_samples[m_headIndex].m_durationMs >= m_sampleMs) {
				LegoU32 overflowMs = m_samples[m_headIndex].m_durationMs - m_sampleMs;
				m_samples[m_headIndex].m_durationMs = m_sampleMs;

				m_headIndex++;
				if (m_headIndex == m_ringSize) {
					m_headIndex = 0;
				}

				if (overflowMs > m_sampleMs) {
					overflowMs = m_sampleMs;
				}

				m_samples[m_headIndex].m_durationMs = overflowMs;
			}

			for (LegoU32 i = 0; i < m_pointCount; i++) {
				m_samples[m_headIndex].m_points[i].m_x = p_positions[i].m_x;
				m_samples[m_headIndex].m_points[i].m_y = p_positions[i].m_y;
				m_samples[m_headIndex].m_points[i].m_z = p_positions[i].m_z;
			}

			m_samples[m_headIndex].m_center.m_x = m_centerX;
			m_samples[m_headIndex].m_center.m_y = m_centerY;
			m_samples[m_headIndex].m_center.m_z = m_centerZ;
		} while (remainingMs > 0);
	}

	RebuildGeometry();
	m_flags |= c_drawable;
}

// FUNCTION: LEGORACERS 0x004931a0
void RaceTrailManager::Trail::RebuildGeometry()
{
	LegoU32 frameCount;
	if (m_headIndex > m_tailIndex) {
		frameCount = m_headIndex - m_tailIndex + 1;
	}
	else {
		frameCount = m_ringSize - m_tailIndex + m_headIndex + 1;
	}

	Sample* activeFrame = &m_samples[m_headIndex];
	GolVec3 center;
	center.m_x = activeFrame->m_points[0].m_x;
	center.m_y = activeFrame->m_points[1].m_y;
	center.m_z = activeFrame->m_points[2].m_z;
	m_entity.SetPosition(center);

	GdbVertexArray* vertexArray;
	LegoFloat uStep = 1.0f / static_cast<LegoFloat>(static_cast<LegoS32>(m_pointCount + 1));
	LegoFloat vStep = 1.0f / static_cast<LegoFloat>(static_cast<LegoS32>(m_sampleCount + 1));
	m_model->GetVertexArray(&vertexArray);

	ColorRGBA color = m_color;
	LegoU8 alphaStep = static_cast<LegoU8>(static_cast<LegoS32>(
		((static_cast<LegoFloat>(m_color.m_alp) * g_inv255 - m_endAlpha) /
		 static_cast<LegoFloat>(static_cast<LegoS32>(m_sampleCount + 1))) *
		255.0f
	));

	LegoU32 vertexIndex = 0;
	LegoU32 elapsedMs = 0;
	GolVec2 texCoord;
	texCoord.m_x = 0.0f;
	texCoord.m_y = 0.0f;

	for (LegoU32 frameOffset = 0; frameOffset < frameCount; frameOffset++) {
		LegoU32 frameIndex = (m_headIndex + m_ringSize - frameOffset) % m_ringSize;
		Sample* frame = &m_samples[frameIndex];

		LegoFloat tailAmount;
		if (frameOffset) {
			tailAmount = static_cast<LegoFloat>(static_cast<LegoS32>(elapsedMs)) /
						 static_cast<LegoFloat>(static_cast<LegoS32>(m_durationMs)) * (1.0f - m_endScale);
			color.m_alp -= alphaStep;
		}
		else {
			tailAmount = 0.0f;
		}

		texCoord.m_x = 0.0f;
		for (LegoU32 pointIndex = 0; pointIndex < m_pointCount; pointIndex++) {
			if (vertexIndex < 0x6a) {
				GolVec3 position = frame->m_points[pointIndex];
				if (m_endScale != 1.0f && tailAmount != 0.0f) {
					position.m_x += (frame->m_center.m_x - position.m_x) * tailAmount;
					position.m_y += (frame->m_center.m_y - position.m_y) * tailAmount;
					position.m_z += (frame->m_center.m_z - position.m_z) * tailAmount;
				}

				position.m_x -= center.m_x;
				position.m_y -= center.m_y;
				position.m_z -= center.m_z;
				vertexArray->SetPosition(vertexIndex, position);
				vertexArray->SetColor(vertexIndex, color);
				vertexArray->SetTextureCoordinate(vertexIndex, texCoord);
				vertexIndex++;
			}

			texCoord.m_x += uStep;
		}

		texCoord.m_y += vStep;
		elapsedMs += frame->m_durationMs;
	}

	m_model->AddFlagsWithBounds(1, FALSE);

	LegoU32 rowsPerGroup = 0x40 / m_pointCount;
	LegoU32 groupCount;
	if (rowsPerGroup > frameCount) {
		rowsPerGroup = frameCount;
		groupCount = 1;
	}
	else {
		groupCount = frameCount / (rowsPerGroup - 1) + 1;
	}

	GdbModelIndexArrayBase* indexArrayBase;
	m_model->GetIndexArrayInto(&indexArrayBase);
	GdbModelIndexArray* indexArray = static_cast<GdbModelIndexArray*>(indexArrayBase);
	GdbModelIndexArray::Indices* indices = indexArray->GetMutableIndices();

	LegoU32 triangleIndex = 0;
	for (LegoU32 rowIndex = 0; rowIndex < rowsPerGroup - 1; rowIndex++) {
		LegoU32 rowBase = rowIndex * m_pointCount;
		for (LegoU32 pointIndex = 0; pointIndex < m_pointCount; pointIndex++) {
			LegoU32 nextPoint = (pointIndex + 1) % m_pointCount;
			if (triangleIndex < 0x35) {
				indices[triangleIndex].m_a = static_cast<LegoU8>(rowBase + pointIndex + m_pointCount);
				indices[triangleIndex].m_b = static_cast<LegoU8>(rowBase + pointIndex);
				indices[triangleIndex].m_c = static_cast<LegoU8>(rowBase + m_pointCount + nextPoint);
				indices[triangleIndex].m_x = 0;
				triangleIndex++;
			}

			if (triangleIndex < 0x35) {
				indices[triangleIndex].m_a = static_cast<LegoU8>(rowBase + m_pointCount + nextPoint);
				indices[triangleIndex].m_b = static_cast<LegoU8>(rowBase + pointIndex);
				indices[triangleIndex].m_c = static_cast<LegoU8>(rowBase + nextPoint);
				indices[triangleIndex].m_x = 0;
				triangleIndex++;
			}
		}
	}

	LegoU32 batchTriangleCount = triangleIndex;
	if (m_pointCount > 2) {
		LegoU32 lastRowBase = (rowsPerGroup - 1) * m_pointCount;
		for (LegoU32 pointIndex = 1; pointIndex < m_pointCount - 1; pointIndex++) {
			if (triangleIndex < 0x35) {
				indices[triangleIndex].m_a = static_cast<LegoU8>(lastRowBase);
				indices[triangleIndex].m_b = static_cast<LegoU8>(lastRowBase + pointIndex);
				indices[triangleIndex].m_c = static_cast<LegoU8>(lastRowBase + pointIndex + 1);
				indices[triangleIndex].m_x = 0;
				triangleIndex++;
			}
		}
	}

	m_model->AddFlags(1);

	LegoU32* groups = m_model->GetMutableGroups();
	groups[0] = GolModel::c_groupTypeMaterial;
	LegoU32 groupWordIndex = 1;
	LegoU32 firstVertex = 0;
	for (LegoU32 groupIndex = 0; groupIndex < groupCount; groupIndex++) {
		if (groupWordIndex < 4) {
			groups[groupWordIndex++] =
				(firstVertex & GolModel::c_materialMatrixIndexMask) |
				(((rowsPerGroup * m_pointCount + 0xffff) << GolModel::c_groupDataUpperWordShift) &
				 (GolModel::c_vertexCountMask << GolModel::c_groupDataUpperWordShift));
		}

		LegoU32 groupTriangleCount = batchTriangleCount;
		if (groupIndex >= groupCount - 1) {
			groupTriangleCount = triangleIndex;
		}

		if (groupWordIndex < 4) {
			groups[groupWordIndex++] =
				GolModel::c_groupTypeTriangleBatch |
				((groupTriangleCount & GolModel::c_triangleCountMask) << GolModel::c_groupDataUpperWordShift);
		}

		firstVertex += rowsPerGroup * m_pointCount;
	}

	if (groupWordIndex < 4) {
		groups[groupWordIndex] = GolModel::c_groupTypeEnd;
	}
	else if (groups[3] != GolModel::c_groupTypeEnd) {
		groups[3] = GolModel::c_groupTypeEnd;
	}
}

// FUNCTION: LEGORACERS 0x00493790
void RaceTrailManager::Trail::DrawTransparent(GolD3DRenderDevice* p_renderer)
{
	if (m_flags & c_drawable) {
		p_renderer->DrawModelEntity(&m_entity);
	}
}

// FUNCTION: LEGORACERS 0x004937b0
RaceTrailManager::RaceTrailManager()
{
	m_trails = NULL;
	m_trailCount = 0;
}

// FUNCTION: LEGORACERS 0x004937f0
RaceTrailManager::~RaceTrailManager()
{
	Destroy();
}

// FUNCTION: LEGORACERS 0x00493800
void RaceTrailManager::Destroy()
{
	if (m_trails) {
		for (LegoU32 i = 0; i < m_trailCount; i++) {
			m_trails[i].Destroy();
		}

		Trail* trails = m_trails;
		delete[] trails;

		m_trails = NULL;
	}

	m_trailCount = 0;
}

// FUNCTION: LEGORACERS 0x00493850
void RaceTrailManager::Initialize(GolD3DRenderDevice* p_renderer, GolExport* p_golExport, LegoU32 p_trailCount)
{
	if (m_trails) {
		Destroy();
	}

	m_trails = new Trail[p_trailCount];
	if (m_trails == NULL) {
		GOL_FATALERROR(c_golErrorOutOfMemory);
	}

	m_trailCount = p_trailCount;
	GolMaterial* material = p_renderer->FindMaterialByName("streak");

	LegoU32 i;
	for (i = 0; i < m_trailCount; i++) {
		m_trails[i].Initialize(p_renderer, p_golExport);
		m_trails[i].SetMaterial(p_renderer, material);
	}
}

// FUNCTION: LEGORACERS 0x004939b0
RaceTrailManager::Trail* RaceTrailManager::AcquireTrail(Trail::Params* p_params)
{
	LegoU32 count = m_trailCount;
	LegoU32 i;

	for (i = 0; i < count; i++) {
		if (!(m_trails[i].m_flags & Trail::c_active)) {
			break;
		}
	}

	if (i == count) {
		return NULL;
	}

	m_trails[i].Start(p_params);
	return &m_trails[i];
}

// FUNCTION: LEGORACERS 0x00493a10
void RaceTrailManager::ReleaseTrail(Trail* p_trail)
{
	p_trail->Release();
}

// FUNCTION: LEGORACERS 0x00493a20
LegoU32 RaceTrailManager::Update(LegoU32 p_elapsedMs)
{
	LegoU32 i;
	LegoU32 result = m_trailCount;

	for (i = 0; i < result; i++) {
		if (m_trails[i].IsActive()) {
			m_trails[i].Update(p_elapsedMs);
		}

		result = m_trailCount;
	}

	return result;
}

// FUNCTION: LEGORACERS 0x00493a60
LegoU32 RaceTrailManager::DrawOpaque(GolD3DRenderDevice* p_renderer)
{
	LegoU32 i;
	LegoU32 result = m_trailCount;

	for (i = 0; i < result; i++) {
		if (m_trails[i].IsActive()) {
			m_trails[i].FUN_004513d0(p_renderer);
		}

		result = m_trailCount;
	}

	return result;
}

// FUNCTION: LEGORACERS 0x00493aa0
LegoU32 RaceTrailManager::DrawTransparent(GolD3DRenderDevice* p_renderer)
{
	LegoU32 i;
	LegoU32 result = m_trailCount;

	for (i = 0; i < result; i++) {
		if (m_trails[i].IsActive()) {
			m_trails[i].DrawTransparent(p_renderer);
		}

		result = m_trailCount;
	}

	return result;
}

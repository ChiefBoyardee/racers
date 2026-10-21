#include "golmodelrenderstate.h"

#include "gdbvertexarray.h"
#include "golmaterial.h"
#include "golmodelbase.h"
#include "golmodelentity.h"

#include <string.h>

DECOMP_SIZE_ASSERT(GolModelRenderState, 0x104)
DECOMP_SIZE_ASSERT(GolD3DRenderState::CommandVertex, 0x20)
DECOMP_SIZE_ASSERT(GolD3DRenderState::MaterialCommand, 0x1c)
DECOMP_SIZE_ASSERT(GolD3DRenderState::DrawCommand, 0x1c)

// GLOBAL: GOLDP 0x1005ce10
// GLOBAL: LEGORACERS 0x004be2d8
GolModelRenderState::DrawFunction GolModelRenderState::g_drawFunctions[8] = {
	&GolModelRenderState::LightVertices0,
	&GolModelRenderState::LightVertices1,
	&GolModelRenderState::LightVertices2,
	&GolModelRenderState::LightVertices3,
	&GolModelRenderState::LightVertices4,
	&GolModelRenderState::LightVertices5,
	&GolModelRenderState::LightVertices6,
	&GolModelRenderState::LightVertices7,
};

// FUNCTION: LEGORACERS 0x0040ead0
GolModelRenderState::GolModelRenderState()
{
	Reset();
}

// FUNCTION: LEGORACERS 0x0040eaf0
void GolModelRenderState::Reset()
{
	m_materialColorValue.m_red = 0xff;
	m_materialColorValue.m_grn = 0xff;
	m_materialColorValue.m_blu = 0xff;
	m_materialColorValue.m_alp = 0xff;
	m_activeMaterialColor.m_red = 0xff;
	m_activeMaterialColor.m_grn = 0xff;
	m_activeMaterialColor.m_blu = 0xff;
	m_activeMaterialColor.m_alp = 0xff;
	m_lightCount = 0;
	m_materialColor = NULL;
	::memset(m_lights, 0, sizeof(m_lights));
	::memset(m_lightColors, 0, sizeof(m_lightColors));
	::memset(m_lightColorProducts, 0, sizeof(m_lightColorProducts));
	::memset(m_lightDirections, 0, sizeof(m_lightDirections));
	m_activeMaterial = NULL;
	m_model = NULL;
	m_vertexArray = NULL;
	m_drawFunction = g_drawFunctions[m_lightCount];
}

// FUNCTION: LEGORACERS 0x0040eb60
void GolModelRenderState::ResetLighting()
{
	Reset();
}

// FUNCTION: LEGORACERS 0x0040eb70
void GolModelRenderState::SetAmbientColor(const GolRenderDevice::MaterialColor* p_material)
{
	m_materialColor = p_material;
	m_materialColorValue = p_material->GetColor();
	m_drawFunction = g_drawFunctions[m_lightCount];
	UpdateMaterialCaches();
}

// FUNCTION: LEGORACERS 0x0040eba0
void GolModelRenderState::AddLight(const GolRenderDevice::Light* p_light)
{
	if (m_lightCount < sizeOfArray(m_lights)) {
		m_lightColors[m_lightCount] = p_light->GetColor();
		m_lightDirections[m_lightCount] = p_light->m_direction;
		m_lights[m_lightCount] = p_light;
		m_lightCount++;
		m_drawFunction = g_drawFunctions[m_lightCount];
		UpdateMaterialCaches();
	}
}

#ifdef COMPAT_MODE

void GolModelRenderState::UpdateMaterialCaches()
{
	GolMaterial* activeMaterial = m_activeMaterial;
	LegoU32 zero = 0;

	if (activeMaterial == NULL) {
		m_activeMaterialColor.m_red = zero;
		m_activeMaterialColor.m_grn = zero;
		m_activeMaterialColor.m_blu = zero;
		m_activeMaterialColor.m_alp = 0xff;

		for (LegoU32 i = zero; i < m_lightCount; i++) {
			m_lightColorProducts[i].m_red = 0.0f;
			m_lightColorProducts[i].m_grn = 0.0f;
			m_lightColorProducts[i].m_blu = 0.0f;
		}

		return;
	}

	GolMaterialColor ambient = activeMaterial->GetAmbient();
	m_activeMaterialColor.m_red = (static_cast<LegoU32>(ambient.m_red & 0xff) * m_materialColorValue.m_red) >> 8;
	m_activeMaterialColor.m_grn = (static_cast<LegoU32>(ambient.m_grn & 0xff) * m_materialColorValue.m_grn) >> 8;
	m_activeMaterialColor.m_blu = (static_cast<LegoU32>(ambient.m_blu & 0xff) * m_materialColorValue.m_blu) >> 8;
	m_activeMaterialColor.m_alp = ambient.m_alp;

	GolMaterialColor diffuse = activeMaterial->GetDiffuse();
	if (m_lightCount > zero) {
		LegoU32 i = 0;
		LegoU32 red = diffuse.m_red & 0xff;
		LegoU32 grn = diffuse.m_grn & 0xff;
		LegoU32 blu = diffuse.m_blu & 0xff;
		FColorRGB* lightProduct = m_lightColorProducts;
		ColorRGBA* lightColor = m_lightColors;

		while (i < m_lightCount) {
			LegoS32 product = static_cast<LegoS32>((red * lightColor->m_red) >> 8);
			lightProduct->m_red = static_cast<LegoFloat>(product);

			product = static_cast<LegoS32>((grn * lightColor->m_grn) >> 8);
			lightProduct->m_grn = static_cast<LegoFloat>(product);

			product = static_cast<LegoS32>((blu * lightColor->m_blu) >> 8);
			lightProduct->m_blu = static_cast<LegoFloat>(product);
			lightProduct++;
			lightColor++;
			i++;
		}
	}
}

#else

// FUNCTION: LEGORACERS 0x0040ec00
__declspec(naked) void GolModelRenderState::UpdateMaterialCaches()
{
	__asm {
		sub esp, 0x10
		push ebx
		push esi
		mov esi, dword ptr [ecx + 0x2c]
		xor ebx, ebx
		cmp esi, ebx
		push edi
		jne L_UpdateMaterialCaches_0
		mov eax, dword ptr [ecx + 4]
		xor edx, edx
		cmp eax, ebx
		mov byte ptr [ecx + 0x30], bl
		mov byte ptr [ecx + 0x31], bl
		mov byte ptr [ecx + 0x32], bl
		mov byte ptr [ecx + 0x33], 0xff
		jbe L_UpdateMaterialCaches_1
		lea eax, [ecx + 0x5c]
	L_UpdateMaterialCaches_2:
		mov dword ptr [eax - 4], ebx
		mov dword ptr [eax], ebx
		mov dword ptr [eax + 4], ebx
		mov esi, dword ptr [ecx + 4]
		inc edx
		add eax, 0xc
		cmp edx, esi
		jb L_UpdateMaterialCaches_2
		pop edi
		pop esi
		pop ebx
		add esp, 0x10
		ret
	L_UpdateMaterialCaches_0:
		mov eax, dword ptr [esi + 0x10]
		xor edx, edx
		mov dl, byte ptr [ecx + 0xc]
		mov dword ptr [esp + 0x14], eax
		and eax, 0xff
		imul edx, eax
		shr edx, 8
		mov byte ptr [ecx + 0x30], dl
		mov edx, dword ptr [esp + 0x15]
		xor eax, eax
		and edx, 0xff
		mov al, byte ptr [ecx + 0xd]
		imul eax, edx
		shr eax, 8
		mov byte ptr [ecx + 0x31], al
		xor eax, eax
		mov al, byte ptr [ecx + 0xe]
		mov edx, eax
		mov eax, dword ptr [esp + 0x16]
		mov edi, eax
		mov byte ptr [ecx + 0x33], ah
		and edi, 0xff
		mov eax, dword ptr [ecx + 4]
		imul edx, edi
		shr edx, 8
		mov byte ptr [ecx + 0x32], dl
		mov esi, dword ptr [esi + 0xc]
		xor edi, edi
		cmp eax, ebx
		mov dword ptr [esp + 0x14], esi
		jbe L_UpdateMaterialCaches_1
		mov edx, dword ptr [esp + 0x15]
		mov eax, dword ptr [esp + 0x16]
		push ebp
		mov ebp, esi
		and ebp, 0xff
		and edx, 0xff
		and eax, 0xff
		mov dword ptr [esp + 0x10], edx
		mov dword ptr [esp + 0x18], eax
		lea eax, [ecx + 0x5c]
		lea esi, [ecx + 0x3d]
		jmp L_UpdateMaterialCaches_3
	L_UpdateMaterialCaches_4:
		mov edx, dword ptr [esp + 0x10]
	L_UpdateMaterialCaches_3:
		xor ebx, ebx
		add eax, 0xc
		mov bl, byte ptr [esi - 1]
		add esi, 4
		imul ebx, ebp
		shr ebx, 8
		mov dword ptr [esp + 0x14], ebx
		xor ebx, ebx
		fild dword ptr [esp + 0x14]
		fstp dword ptr [eax - 0x10]
		mov bl, byte ptr [esi - 4]
		imul ebx, edx
		shr ebx, 8
		mov dword ptr [esp + 0x14], ebx
		xor edx, edx
		fild dword ptr [esp + 0x14]
		fstp dword ptr [eax - 0xc]
		mov dl, byte ptr [esi - 3]
		imul edx, dword ptr [esp + 0x18]
		shr edx, 8
		mov dword ptr [esp + 0x14], edx
		inc edi
		fild dword ptr [esp + 0x14]
		fstp dword ptr [eax - 8]
		cmp edi, dword ptr [ecx + 4]
		jb L_UpdateMaterialCaches_4
		pop ebp
	L_UpdateMaterialCaches_1:
		pop edi
		pop esi
		pop ebx
		add esp, 0x10
		ret
	}
}

#endif

// FUNCTION: LEGORACERS 0x0040ed40
void GolModelRenderState::Begin(GolD3DRenderDevice*, GolModelEntity* p_model, LegoU32 p_lodIndex)
{
	m_model = p_model->GetModel(p_lodIndex);
	m_model->GetVertexArray(&m_vertexArray);
}

// FUNCTION: LEGORACERS 0x0040ed60
void GolModelRenderState::End(GolD3DRenderDevice*, GolModelEntity*, LegoU32)
{
	m_model->AddFlagsWithBounds(1, FALSE);
}

// FUNCTION: LEGORACERS 0x0040ed70
void GolModelRenderState::ProcessVertices(DrawCommand* p_command)
{
	if (p_command->m_material != m_activeMaterial) {
		m_activeMaterial = p_command->m_material;
		UpdateMaterialCaches();
	}

	(this->*m_drawFunction)(
		p_command->m_vertices,
		p_command->m_outputFirst,
		p_command->m_firstVertex,
		p_command->m_vertexCount
	);
}

// FUNCTION: GOLDP 0x1002c020 FOLDED
// FUNCTION: LEGORACERS 0x004513d0 FOLDED
void GolModelRenderState::ProcessMaterial(MaterialCommand*)
{
	// empty
}

// FUNCTION: LEGORACERS 0x004113b0 FOLDED
undefined4 GolModelRenderState::ProcessVerticesPrelit(DrawCommand*)
{
	return 0;
}

// FUNCTION: GOLDP 0x10024780
// FUNCTION: LEGORACERS 0x0040edb0
void GolModelRenderState::LightVertices0(const CommandVertex*, LegoU32, LegoU32 p_firstVertex, LegoU32 p_vertexCount)
{
	for (LegoU32 i = p_firstVertex; i < p_firstVertex + p_vertexCount; i++) {
		m_vertexArray->SetColor(i, m_activeMaterialColor);
	}
}

// FUNCTION: GOLDP 0x100247b0
// FUNCTION: LEGORACERS 0x0040ede0
void GolModelRenderState::LightVertices1(
	const CommandVertex* p_vertices,
	LegoU32 p_outputFirst,
	LegoU32 p_firstVertex,
	LegoU32 p_vertexCount
)
{
	ColorRGBA color;
	LegoS32 redBase = m_activeMaterialColor.m_red;
	LegoS32 grnBase = m_activeMaterialColor.m_grn;
	LegoS32 bluBase = m_activeMaterialColor.m_blu;
	color.m_alp = m_activeMaterialColor.m_alp;

	const CommandVertex* vertex = p_vertices + p_outputFirst;
	const CommandVertex* end = vertex + p_vertexCount;

	if (vertex < end) {
		do {
			LegoFloat dot = vertex->m_nx * m_lightDirections[0].m_x;
			dot += vertex->m_nz * m_lightDirections[0].m_z;
			dot += vertex->m_ny * m_lightDirections[0].m_y;

			LegoFloat redProduct = m_lightColorProducts[0].m_red;
			redProduct *= dot;
			LegoS32 red = redBase - static_cast<LegoS32>(redProduct);

			LegoFloat grnProduct = m_lightColorProducts[0].m_grn;
			grnProduct *= dot;
			LegoS32 grn = grnBase - static_cast<LegoS32>(grnProduct);

			LegoFloat bluProduct = m_lightColorProducts[0].m_blu;
			bluProduct *= dot;
			LegoS32 blu = bluBase - static_cast<LegoS32>(bluProduct);

			if (red < redBase) {
				color.m_red = static_cast<LegoU8>(redBase);
			}
			else {
				color.m_red = 0xff;
				if (red <= 0xff) {
					color.m_red = static_cast<LegoU8>(red);
				}
			}

			if (grn < grnBase) {
				color.m_grn = static_cast<LegoU8>(grnBase);
			}
			else {
				color.m_grn = 0xff;
				if (grn <= 0xff) {
					color.m_grn = static_cast<LegoU8>(grn);
				}
			}

			if (blu < bluBase) {
				color.m_blu = static_cast<LegoU8>(bluBase);
			}
			else {
				color.m_blu = 0xff;
				if (blu <= 0xff) {
					color.m_blu = static_cast<LegoU8>(blu);
				}
			}

			m_vertexArray->SetColor(p_firstVertex++, color);
			vertex++;
		} while (vertex < end);
	}
}

// FUNCTION: GOLDP 0x100248e0
// FUNCTION: LEGORACERS 0x0040ef10
void GolModelRenderState::LightVertices2(
	const CommandVertex* p_vertices,
	LegoU32 p_outputFirst,
	LegoU32 p_firstVertex,
	LegoU32 p_vertexCount
)
{
	ColorRGBA color;
	LegoS32 redBase = m_activeMaterialColor.m_red;
	LegoS32 grnBase = m_activeMaterialColor.m_grn;
	LegoS32 bluBase = m_activeMaterialColor.m_blu;
	color.m_alp = m_activeMaterialColor.m_alp;

	const CommandVertex* vertex = p_vertices + p_outputFirst;
	const CommandVertex* end = vertex + p_vertexCount;

	if (vertex < end) {
		do {
			LegoFloat dot = vertex->m_nx;
			dot *= m_lightDirections[0].m_x;
			LegoFloat normalProduct = vertex->m_ny;
			normalProduct *= m_lightDirections[0].m_y;
			dot += normalProduct;
			normalProduct = vertex->m_nz;
			normalProduct *= m_lightDirections[0].m_z;
			dot += normalProduct;

			LegoFloat redProduct = m_lightColorProducts[0].m_red;
			redProduct *= dot;
			LegoS32 red = redBase - static_cast<LegoS32>(redProduct);

			LegoFloat grnProduct = m_lightColorProducts[0].m_grn;
			grnProduct *= dot;
			LegoS32 grn = grnBase - static_cast<LegoS32>(grnProduct);

			LegoFloat bluProduct = m_lightColorProducts[0].m_blu;
			bluProduct *= dot;
			LegoS32 blu = bluBase;
			blu -= static_cast<LegoS32>(bluProduct);

			dot = vertex->m_ny * m_lightDirections[1].m_y;
			dot += vertex->m_nz * m_lightDirections[1].m_z;
			dot += vertex->m_nx * m_lightDirections[1].m_x;

			redProduct = m_lightColorProducts[1].m_red;
			redProduct *= dot;
			red -= static_cast<LegoS32>(redProduct);

			grnProduct = m_lightColorProducts[1].m_grn;
			grnProduct *= dot;
			grn -= static_cast<LegoS32>(grnProduct);

			bluProduct = m_lightColorProducts[1].m_blu;
			bluProduct *= dot;
			blu -= static_cast<LegoS32>(bluProduct);

			if (red < redBase) {
				color.m_red = static_cast<LegoU8>(redBase);
			}
			else {
				color.m_red = 0xff;
				if (red <= 0xff) {
					color.m_red = static_cast<LegoU8>(red);
				}
			}

			if (grn < grnBase) {
				color.m_grn = static_cast<LegoU8>(grnBase);
			}
			else {
				color.m_grn = 0xff;
				if (grn <= 0xff) {
					color.m_grn = static_cast<LegoU8>(grn);
				}
			}

			if (blu < bluBase) {
				color.m_blu = static_cast<LegoU8>(bluBase);
			}
			else {
				color.m_blu = 0xff;
				if (blu <= 0xff) {
					color.m_blu = static_cast<LegoU8>(blu);
				}
			}

			m_vertexArray->SetColor(p_firstVertex++, color);
			vertex++;
		} while (vertex < end);
	}
}

// FUNCTION: GOLDP 0x10024a70
// FUNCTION: LEGORACERS 0x0040f0a0
void GolModelRenderState::LightVertices3(
	const CommandVertex* p_vertices,
	LegoU32 p_outputFirst,
	LegoU32 p_firstVertex,
	LegoU32 p_vertexCount
)
{
	ColorRGBA color;
	LegoS32 redBase = m_activeMaterialColor.m_red;
	LegoS32 grnBase = m_activeMaterialColor.m_grn;
	LegoS32 bluBase = m_activeMaterialColor.m_blu;
	color.m_alp = m_activeMaterialColor.m_alp;

	const CommandVertex* vertex = p_vertices + p_outputFirst;
	const CommandVertex* end = vertex + p_vertexCount;

	if (vertex < end) {
		do {
			LegoFloat dot = vertex->m_nx;
			dot *= m_lightDirections[0].m_x;
			LegoFloat normalProduct = vertex->m_ny;
			normalProduct *= m_lightDirections[0].m_y;
			dot += normalProduct;
			normalProduct = vertex->m_nz;
			normalProduct *= m_lightDirections[0].m_z;
			dot += normalProduct;

			LegoFloat redProduct = m_lightColorProducts[0].m_red;
			redProduct *= dot;
			LegoS32 red = redBase - static_cast<LegoS32>(redProduct);

			LegoFloat grnProduct = m_lightColorProducts[0].m_grn;
			grnProduct *= dot;
			LegoS32 grn = grnBase - static_cast<LegoS32>(grnProduct);

			LegoFloat bluProduct = m_lightColorProducts[0].m_blu;
			bluProduct *= dot;
			LegoS32 blu = bluBase - static_cast<LegoS32>(bluProduct);

			dot = vertex->m_ny * m_lightDirections[1].m_y;
			dot += vertex->m_nz * m_lightDirections[1].m_z;
			dot += vertex->m_nx * m_lightDirections[1].m_x;

			redProduct = m_lightColorProducts[1].m_red;
			redProduct *= dot;
			red -= static_cast<LegoS32>(redProduct);

			grnProduct = m_lightColorProducts[1].m_grn;
			grnProduct *= dot;
			grn -= static_cast<LegoS32>(grnProduct);

			bluProduct = m_lightColorProducts[1].m_blu;
			bluProduct *= dot;
			blu -= static_cast<LegoS32>(bluProduct);

			dot = (m_lightDirections[2].m_z * vertex->m_nz + vertex->m_ny * m_lightDirections[2].m_y) +
				  m_lightDirections[2].m_x * vertex->m_nx;

			redProduct = m_lightColorProducts[2].m_red;
			redProduct *= dot;
			red -= static_cast<LegoS32>(redProduct);

			grnProduct = m_lightColorProducts[2].m_grn;
			grnProduct *= dot;
			grn -= static_cast<LegoS32>(grnProduct);

			bluProduct = m_lightColorProducts[2].m_blu;
			bluProduct *= dot;
			blu -= static_cast<LegoS32>(bluProduct);

			if (red < redBase) {
				color.m_red = static_cast<LegoU8>(redBase);
			}
			else {
				color.m_red = 0xff;
				if (red <= 0xff) {
					color.m_red = static_cast<LegoU8>(red);
				}
			}

			if (grn < grnBase) {
				color.m_grn = static_cast<LegoU8>(grnBase);
			}
			else {
				color.m_grn = 0xff;
				if (grn <= 0xff) {
					color.m_grn = static_cast<LegoU8>(grn);
				}
			}

			if (blu < bluBase) {
				color.m_blu = static_cast<LegoU8>(bluBase);
			}
			else {
				color.m_blu = 0xff;
				if (blu <= 0xff) {
					color.m_blu = static_cast<LegoU8>(blu);
				}
			}

			m_vertexArray->SetColor(p_firstVertex++, color);
			vertex++;
		} while (vertex < end);
	}
}

// FUNCTION: GOLDP 0x10024c50
// FUNCTION: LEGORACERS 0x0040f280
void GolModelRenderState::LightVertices4(
	const CommandVertex* p_vertices,
	LegoU32 p_outputFirst,
	LegoU32 p_firstVertex,
	LegoU32 p_vertexCount
)
{
	ColorRGBA color;
	color.m_alp = m_activeMaterialColor.m_alp;

	LegoS32 redBase = m_activeMaterialColor.m_red;
	LegoS32 grnBase = m_activeMaterialColor.m_grn;
	LegoS32 bluBase = m_activeMaterialColor.m_blu;
	const CommandVertex* vertex = p_vertices + p_outputFirst;
	const CommandVertex* end = vertex + p_vertexCount;

	if (vertex < end) {
		do {
			LegoFloat dot = vertex->m_ny * m_lightDirections[0].m_y +
							(vertex->m_nz * m_lightDirections[0].m_z + vertex->m_nx * m_lightDirections[0].m_x);
			LegoS32 red = redBase - static_cast<LegoS32>(m_lightColorProducts[0].m_red * dot);
			LegoS32 grn = grnBase - static_cast<LegoS32>(m_lightColorProducts[0].m_grn * dot);
			LegoS32 blu = bluBase - static_cast<LegoS32>(m_lightColorProducts[0].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[1].m_y +
				  (vertex->m_nz * m_lightDirections[1].m_z + vertex->m_nx * m_lightDirections[1].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[1].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[1].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[1].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[2].m_y +
				  (vertex->m_nz * m_lightDirections[2].m_z + vertex->m_nx * m_lightDirections[2].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[2].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[2].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[2].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[3].m_y +
				  (vertex->m_nz * m_lightDirections[3].m_z + vertex->m_nx * m_lightDirections[3].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[3].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[3].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[3].m_blu * dot);

			if (red < redBase) {
				color.m_red = static_cast<LegoU8>(redBase);
			}
			else {
				color.m_red = 0xff;
				if (red <= 0xff) {
					color.m_red = static_cast<LegoU8>(red);
				}
			}

			if (grn < grnBase) {
				color.m_grn = static_cast<LegoU8>(grnBase);
			}
			else {
				color.m_grn = 0xff;
				if (grn <= 0xff) {
					color.m_grn = static_cast<LegoU8>(grn);
				}
			}

			if (blu < bluBase) {
				color.m_blu = static_cast<LegoU8>(bluBase);
			}
			else {
				color.m_blu = 0xff;
				if (blu <= 0xff) {
					color.m_blu = static_cast<LegoU8>(blu);
				}
			}

			m_vertexArray->SetColor(p_firstVertex++, color);
			vertex++;
		} while (vertex < end);
	}
}

// FUNCTION: GOLDP 0x10024e90
// FUNCTION: LEGORACERS 0x0040f4c0
void GolModelRenderState::LightVertices5(
	const CommandVertex* p_vertices,
	LegoU32 p_outputFirst,
	LegoU32 p_firstVertex,
	LegoU32 p_vertexCount
)
{
	ColorRGBA color;
	color.m_alp = m_activeMaterialColor.m_alp;

	LegoS32 redBase = m_activeMaterialColor.m_red;
	LegoS32 grnBase = m_activeMaterialColor.m_grn;
	LegoS32 bluBase = m_activeMaterialColor.m_blu;
	const CommandVertex* vertex = p_vertices + p_outputFirst;
	const CommandVertex* end = vertex + p_vertexCount;

	if (vertex < end) {
		do {
			LegoFloat dot = vertex->m_ny * m_lightDirections[0].m_y +
							(vertex->m_nz * m_lightDirections[0].m_z + vertex->m_nx * m_lightDirections[0].m_x);
			LegoS32 red = redBase - static_cast<LegoS32>(m_lightColorProducts[0].m_red * dot);
			LegoS32 grn = grnBase - static_cast<LegoS32>(m_lightColorProducts[0].m_grn * dot);
			LegoS32 blu = bluBase - static_cast<LegoS32>(m_lightColorProducts[0].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[1].m_y +
				  (vertex->m_nz * m_lightDirections[1].m_z + vertex->m_nx * m_lightDirections[1].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[1].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[1].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[1].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[2].m_y +
				  (vertex->m_nz * m_lightDirections[2].m_z + vertex->m_nx * m_lightDirections[2].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[2].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[2].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[2].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[3].m_y +
				  (vertex->m_nz * m_lightDirections[3].m_z + vertex->m_nx * m_lightDirections[3].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[3].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[3].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[3].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[4].m_y +
				  (vertex->m_nz * m_lightDirections[4].m_z + vertex->m_nx * m_lightDirections[4].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[4].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[4].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[4].m_blu * dot);

			if (red < redBase) {
				color.m_red = static_cast<LegoU8>(redBase);
			}
			else {
				color.m_red = 0xff;
				if (red <= 0xff) {
					color.m_red = static_cast<LegoU8>(red);
				}
			}

			if (grn < grnBase) {
				color.m_grn = static_cast<LegoU8>(grnBase);
			}
			else {
				color.m_grn = 0xff;
				if (grn <= 0xff) {
					color.m_grn = static_cast<LegoU8>(grn);
				}
			}

			if (blu < bluBase) {
				color.m_blu = static_cast<LegoU8>(bluBase);
			}
			else {
				color.m_blu = 0xff;
				if (blu <= 0xff) {
					color.m_blu = static_cast<LegoU8>(blu);
				}
			}

			m_vertexArray->SetColor(p_firstVertex++, color);
			vertex++;
		} while (vertex < end);
	}
}

// FUNCTION: GOLDP 0x10025130
// FUNCTION: LEGORACERS 0x0040f760
void GolModelRenderState::LightVertices6(
	const CommandVertex* p_vertices,
	LegoU32 p_outputFirst,
	LegoU32 p_firstVertex,
	LegoU32 p_vertexCount
)
{
	ColorRGBA color;
	color.m_alp = m_activeMaterialColor.m_alp;

	LegoS32 redBase = m_activeMaterialColor.m_red;
	LegoS32 grnBase = m_activeMaterialColor.m_grn;
	LegoS32 bluBase = m_activeMaterialColor.m_blu;
	const CommandVertex* vertex = p_vertices + p_outputFirst;
	const CommandVertex* end = vertex + p_vertexCount;

	if (vertex < end) {
		do {
			LegoFloat dot = vertex->m_ny * m_lightDirections[0].m_y +
							(vertex->m_nz * m_lightDirections[0].m_z + vertex->m_nx * m_lightDirections[0].m_x);
			LegoS32 red = redBase - static_cast<LegoS32>(m_lightColorProducts[0].m_red * dot);
			LegoS32 grn = grnBase - static_cast<LegoS32>(m_lightColorProducts[0].m_grn * dot);
			LegoS32 blu = bluBase - static_cast<LegoS32>(m_lightColorProducts[0].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[1].m_y +
				  (vertex->m_nz * m_lightDirections[1].m_z + vertex->m_nx * m_lightDirections[1].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[1].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[1].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[1].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[2].m_y +
				  (vertex->m_nz * m_lightDirections[2].m_z + vertex->m_nx * m_lightDirections[2].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[2].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[2].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[2].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[3].m_y +
				  (vertex->m_nz * m_lightDirections[3].m_z + vertex->m_nx * m_lightDirections[3].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[3].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[3].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[3].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[4].m_y +
				  (vertex->m_nz * m_lightDirections[4].m_z + vertex->m_nx * m_lightDirections[4].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[4].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[4].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[4].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[5].m_y +
				  (vertex->m_nz * m_lightDirections[5].m_z + vertex->m_nx * m_lightDirections[5].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[5].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[5].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[5].m_blu * dot);

			if (red < redBase) {
				color.m_red = static_cast<LegoU8>(redBase);
			}
			else {
				color.m_red = 0xff;
				if (red <= 0xff) {
					color.m_red = static_cast<LegoU8>(red);
				}
			}

			if (grn < grnBase) {
				color.m_grn = static_cast<LegoU8>(grnBase);
			}
			else {
				color.m_grn = 0xff;
				if (grn <= 0xff) {
					color.m_grn = static_cast<LegoU8>(grn);
				}
			}

			if (blu < bluBase) {
				color.m_blu = static_cast<LegoU8>(bluBase);
			}
			else {
				color.m_blu = 0xff;
				if (blu <= 0xff) {
					color.m_blu = static_cast<LegoU8>(blu);
				}
			}

			m_vertexArray->SetColor(p_firstVertex++, color);
			vertex++;
		} while (vertex < end);
	}
}

// FUNCTION: GOLDP 0x10025420
// FUNCTION: LEGORACERS 0x0040fa50
void GolModelRenderState::LightVertices7(
	const CommandVertex* p_vertices,
	LegoU32 p_outputFirst,
	LegoU32 p_firstVertex,
	LegoU32 p_vertexCount
)
{
	ColorRGBA color;
	color.m_alp = m_activeMaterialColor.m_alp;

	LegoS32 redBase = m_activeMaterialColor.m_red;
	LegoS32 grnBase = m_activeMaterialColor.m_grn;
	LegoS32 bluBase = m_activeMaterialColor.m_blu;
	const CommandVertex* vertex = p_vertices + p_outputFirst;
	const CommandVertex* end = vertex + p_vertexCount;

	if (vertex < end) {
		do {
			LegoFloat dot = vertex->m_ny * m_lightDirections[0].m_y +
							(vertex->m_nz * m_lightDirections[0].m_z + vertex->m_nx * m_lightDirections[0].m_x);
			LegoS32 red = redBase - static_cast<LegoS32>(m_lightColorProducts[0].m_red * dot);
			LegoS32 grn = grnBase - static_cast<LegoS32>(m_lightColorProducts[0].m_grn * dot);
			LegoS32 blu = bluBase - static_cast<LegoS32>(m_lightColorProducts[0].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[1].m_y +
				  (vertex->m_nz * m_lightDirections[1].m_z + vertex->m_nx * m_lightDirections[1].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[1].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[1].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[1].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[2].m_y +
				  (vertex->m_nz * m_lightDirections[2].m_z + vertex->m_nx * m_lightDirections[2].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[2].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[2].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[2].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[3].m_y +
				  (vertex->m_nz * m_lightDirections[3].m_z + vertex->m_nx * m_lightDirections[3].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[3].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[3].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[3].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[4].m_y +
				  (vertex->m_nz * m_lightDirections[4].m_z + vertex->m_nx * m_lightDirections[4].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[4].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[4].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[4].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[5].m_y +
				  (vertex->m_nz * m_lightDirections[5].m_z + vertex->m_nx * m_lightDirections[5].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[5].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[5].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[5].m_blu * dot);

			dot = vertex->m_ny * m_lightDirections[6].m_y +
				  (vertex->m_nz * m_lightDirections[6].m_z + vertex->m_nx * m_lightDirections[6].m_x);
			red -= static_cast<LegoS32>(m_lightColorProducts[6].m_red * dot);
			grn -= static_cast<LegoS32>(m_lightColorProducts[6].m_grn * dot);
			blu -= static_cast<LegoS32>(m_lightColorProducts[6].m_blu * dot);

			if (red < redBase) {
				color.m_red = static_cast<LegoU8>(redBase);
			}
			else {
				color.m_red = 0xff;
				if (red <= 0xff) {
					color.m_red = static_cast<LegoU8>(red);
				}
			}

			if (grn < grnBase) {
				color.m_grn = static_cast<LegoU8>(grnBase);
			}
			else {
				color.m_grn = 0xff;
				if (grn <= 0xff) {
					color.m_grn = static_cast<LegoU8>(grn);
				}
			}

			if (blu < bluBase) {
				color.m_blu = static_cast<LegoU8>(bluBase);
			}
			else {
				color.m_blu = 0xff;
				if (blu <= 0xff) {
					color.m_blu = static_cast<LegoU8>(blu);
				}
			}

			m_vertexArray->SetColor(p_firstVertex++, color);
			vertex++;
		} while (vertex < end);
	}
}

#include "golmodelrenderstate.h"

#include "gdbvertexarray.h"
#include "golmaterial.h"
#include "golmodelbase.h"
#include "golmodelentity.h"

#include <string.h>

extern "C" long _ftol();

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

// FUNCTION: LEGORACERS 0x0040ec00
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

#ifdef COMPAT_MODE

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

#else

// FUNCTION: GOLDP 0x100248e0
// FUNCTION: LEGORACERS 0x0040ef10
__declspec(naked) void GolModelRenderState::LightVertices2(
	const CommandVertex* p_vertices,
	LegoU32 p_outputFirst,
	LegoU32 p_firstVertex,
	LegoU32 p_vertexCount
)
{
	__asm {
		sub esp, 0x10
		push ebx
		push ebp
		mov ebp, dword ptr [esp + 0x1c]
		push esi
		push edi
		mov edi, dword ptr [esp + 0x28]
		mov esi, ecx
		xor eax, eax
		shl edi, 5
		mov al, byte ptr [esi + 0x31]
		add edi, ebp
		mov ebp, dword ptr [esp + 0x30]
		mov dword ptr [esp + 0x18], eax
		xor eax, eax
		xor ebx, ebx
		mov al, byte ptr [esi + 0x32]
		mov bl, byte ptr [esi + 0x30]
		shl ebp, 5
		mov dword ptr [esp + 0x1c], eax
		mov al, byte ptr [esi + 0x33]
		add ebp, edi
		mov dword ptr [esp + 0x14], ebx
		cmp edi, ebp
		mov byte ptr [esp + 0x13], al
		jae L_LightVertices2_0
		jmp L_LightVertices2_1
	L_LightVertices2_8:
		mov ebx, dword ptr [esp + 0x14]
	L_LightVertices2_1:
		fld dword ptr [edi + 0x14]
		fmul dword ptr [esi + 0xac]
		fld dword ptr [edi + 0x18]
		fmul dword ptr [esi + 0xb0]
		faddp st(1), st(0)
		fld dword ptr [edi + 0x1c]
		fmul dword ptr [esi + 0xb4]
		faddp st(1), st(0)
		fld dword ptr [esi + 0x58]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x5c]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x60]
		mov ecx, dword ptr [esp + 0x18]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		fstp st(0)
		fld dword ptr [esi + 0xbc]
		fmul dword ptr [edi + 0x18]
		fld dword ptr [esi + 0xc0]
		fmul dword ptr [edi + 0x1c]
		mov ecx, dword ptr [esp + 0x1c]
		faddp st(1), st(0)
		fld dword ptr [esi + 0xb8]
		fmul dword ptr [edi + 0x14]
		sub ecx, eax
		mov dword ptr [esp + 0x24], ecx
		faddp st(1), st(0)
		fld dword ptr [esi + 0x64]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x68]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x6c]
		mov ecx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		mov ecx, dword ptr [esp + 0x24]
		sub ecx, eax
		mov eax, dword ptr [esp + 0x14]
		cmp ebx, eax
		fstp st(0)
		jge L_LightVertices2_2
		mov byte ptr [esp + 0x10], al
		jmp L_LightVertices2_3
	L_LightVertices2_2:
		cmp ebx, 0xff
		mov byte ptr [esp + 0x10], 0xff
		jg L_LightVertices2_3
		mov byte ptr [esp + 0x10], bl
	L_LightVertices2_3:
		mov eax, dword ptr [esp + 0x28]
		mov edx, dword ptr [esp + 0x18]
		cmp eax, edx
		jge L_LightVertices2_4
		mov byte ptr [esp + 0x11], dl
		jmp L_LightVertices2_5
	L_LightVertices2_4:
		cmp eax, 0xff
		mov byte ptr [esp + 0x11], 0xff
		jg L_LightVertices2_5
		mov byte ptr [esp + 0x11], al
	L_LightVertices2_5:
		mov eax, dword ptr [esp + 0x1c]
		cmp ecx, eax
		jge L_LightVertices2_6
		mov byte ptr [esp + 0x12], al
		jmp L_LightVertices2_7
	L_LightVertices2_6:
		cmp ecx, 0xff
		mov byte ptr [esp + 0x12], 0xff
		jg L_LightVertices2_7
		mov byte ptr [esp + 0x12], cl
	L_LightVertices2_7:
		mov ecx, dword ptr [esp + 0x2c]
		lea ebx, [esp + 0x10]
		mov eax, ecx
		inc ecx
		mov dword ptr [esp + 0x2c], ecx
		mov ecx, dword ptr [esi + 0x38]
		push ebx
		push eax
		mov edx, dword ptr [ecx]
		call dword ptr [edx + 0x30]
		add edi, 0x20
		cmp edi, ebp
		jb L_LightVertices2_8
	L_LightVertices2_0:
		pop edi
		pop esi
		pop ebp
		pop ebx
		add esp, 0x10
		ret 0x10
	}
}

#endif

#ifdef COMPAT_MODE

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

#else

// FUNCTION: GOLDP 0x10024a70
// FUNCTION: LEGORACERS 0x0040f0a0
__declspec(naked) void GolModelRenderState::LightVertices3(
	const CommandVertex* p_vertices,
	LegoU32 p_outputFirst,
	LegoU32 p_firstVertex,
	LegoU32 p_vertexCount
)
{
	__asm {
		sub esp, 0x10
		push ebx
		push ebp
		mov ebp, dword ptr [esp + 0x1c]
		push esi
		push edi
		mov edi, dword ptr [esp + 0x28]
		mov esi, ecx
		xor eax, eax
		shl edi, 5
		mov al, byte ptr [esi + 0x31]
		add edi, ebp
		mov ebp, dword ptr [esp + 0x30]
		mov dword ptr [esp + 0x18], eax
		xor eax, eax
		xor ebx, ebx
		mov al, byte ptr [esi + 0x32]
		mov bl, byte ptr [esi + 0x30]
		shl ebp, 5
		mov dword ptr [esp + 0x1c], eax
		mov al, byte ptr [esi + 0x33]
		add ebp, edi
		mov dword ptr [esp + 0x14], ebx
		cmp edi, ebp
		mov byte ptr [esp + 0x13], al
		jae L_LightVertices3_0
		jmp L_LightVertices3_1
	L_LightVertices3_8:
		mov ebx, dword ptr [esp + 0x14]
	L_LightVertices3_1:
		fld dword ptr [edi + 0x14]
		fmul dword ptr [esi + 0xac]
		fld dword ptr [edi + 0x18]
		fmul dword ptr [esi + 0xb0]
		faddp st(1), st(0)
		fld dword ptr [edi + 0x1c]
		fmul dword ptr [esi + 0xb4]
		faddp st(1), st(0)
		fld dword ptr [esi + 0x58]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x5c]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x60]
		mov ecx, dword ptr [esp + 0x18]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		fstp st(0)
		fld dword ptr [esi + 0xbc]
		fmul dword ptr [edi + 0x18]
		fld dword ptr [esi + 0xc0]
		fmul dword ptr [edi + 0x1c]
		mov ecx, dword ptr [esp + 0x1c]
		faddp st(1), st(0)
		fld dword ptr [esi + 0xb8]
		fmul dword ptr [edi + 0x14]
		sub ecx, eax
		mov dword ptr [esp + 0x24], ecx
		faddp st(1), st(0)
		fld dword ptr [esi + 0x64]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x68]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x6c]
		mov edx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub edx, eax
		mov dword ptr [esp + 0x28], edx
		call _ftol
		mov ecx, dword ptr [esp + 0x24]
		fstp st(0)
		fld dword ptr [esi + 0xcc]
		fmul dword ptr [edi + 0x1c]
		fld dword ptr [edi + 0x18]
		sub ecx, eax
		mov dword ptr [esp + 0x24], ecx
		fmul dword ptr [esi + 0xc8]
		faddp st(1), st(0)
		fld dword ptr [esi + 0xc4]
		fmul dword ptr [edi + 0x14]
		faddp st(1), st(0)
		fld dword ptr [esi + 0x70]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x74]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x78]
		mov ecx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		mov ecx, dword ptr [esp + 0x24]
		sub ecx, eax
		mov eax, dword ptr [esp + 0x14]
		cmp ebx, eax
		fstp st(0)
		jge L_LightVertices3_2
		mov byte ptr [esp + 0x10], al
		jmp L_LightVertices3_3
	L_LightVertices3_2:
		cmp ebx, 0xff
		mov byte ptr [esp + 0x10], 0xff
		jg L_LightVertices3_3
		mov byte ptr [esp + 0x10], bl
	L_LightVertices3_3:
		mov eax, dword ptr [esp + 0x28]
		mov edx, dword ptr [esp + 0x18]
		cmp eax, edx
		jge L_LightVertices3_4
		mov byte ptr [esp + 0x11], dl
		jmp L_LightVertices3_5
	L_LightVertices3_4:
		cmp eax, 0xff
		mov byte ptr [esp + 0x11], 0xff
		jg L_LightVertices3_5
		mov byte ptr [esp + 0x11], al
	L_LightVertices3_5:
		mov eax, dword ptr [esp + 0x1c]
		cmp ecx, eax
		jge L_LightVertices3_6
		mov byte ptr [esp + 0x12], al
		jmp L_LightVertices3_7
	L_LightVertices3_6:
		cmp ecx, 0xff
		mov byte ptr [esp + 0x12], 0xff
		jg L_LightVertices3_7
		mov byte ptr [esp + 0x12], cl
	L_LightVertices3_7:
		mov ecx, dword ptr [esp + 0x2c]
		lea ebx, [esp + 0x10]
		mov eax, ecx
		inc ecx
		mov dword ptr [esp + 0x2c], ecx
		mov ecx, dword ptr [esi + 0x38]
		push ebx
		push eax
		mov edx, dword ptr [ecx]
		call dword ptr [edx + 0x30]
		add edi, 0x20
		cmp edi, ebp
		jb L_LightVertices3_8
	L_LightVertices3_0:
		pop edi
		pop esi
		pop ebp
		pop ebx
		add esp, 0x10
		ret 0x10
	}
}

#endif

#ifdef COMPAT_MODE

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

#else

// FUNCTION: GOLDP 0x10024c50
// FUNCTION: LEGORACERS 0x0040f280
__declspec(naked) void GolModelRenderState::LightVertices4(
	const CommandVertex* p_vertices,
	LegoU32 p_outputFirst,
	LegoU32 p_firstVertex,
	LegoU32 p_vertexCount
)
{
	__asm {
		sub esp, 0x10
		push ebx
		push ebp
		mov ebp, dword ptr [esp + 0x1c]
		push esi
		push edi
		mov edi, dword ptr [esp + 0x28]
		mov esi, ecx
		xor eax, eax
		shl edi, 5
		mov al, byte ptr [esi + 0x31]
		add edi, ebp
		mov ebp, dword ptr [esp + 0x30]
		mov dword ptr [esp + 0x18], eax
		xor eax, eax
		xor ebx, ebx
		mov al, byte ptr [esi + 0x32]
		mov bl, byte ptr [esi + 0x30]
		shl ebp, 5
		mov dword ptr [esp + 0x1c], eax
		mov al, byte ptr [esi + 0x33]
		add ebp, edi
		mov dword ptr [esp + 0x14], ebx
		cmp edi, ebp
		mov byte ptr [esp + 0x13], al
		jae L_LightVertices4_0
		jmp L_LightVertices4_1
	L_LightVertices4_8:
		mov ebx, dword ptr [esp + 0x14]
	L_LightVertices4_1:
		fld dword ptr [edi + 0x14]
		fmul dword ptr [esi + 0xac]
		fld dword ptr [edi + 0x18]
		fmul dword ptr [esi + 0xb0]
		faddp st(1), st(0)
		fld dword ptr [edi + 0x1c]
		fmul dword ptr [esi + 0xb4]
		faddp st(1), st(0)
		fld dword ptr [esi + 0x58]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x5c]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x60]
		mov ecx, dword ptr [esp + 0x18]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		fstp st(0)
		fld dword ptr [esi + 0xbc]
		fmul dword ptr [edi + 0x18]
		fld dword ptr [esi + 0xc0]
		fmul dword ptr [edi + 0x1c]
		mov ecx, dword ptr [esp + 0x1c]
		faddp st(1), st(0)
		fld dword ptr [esi + 0xb8]
		fmul dword ptr [edi + 0x14]
		sub ecx, eax
		mov dword ptr [esp + 0x24], ecx
		faddp st(1), st(0)
		fld dword ptr [esi + 0x64]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x68]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x6c]
		mov ecx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		mov edx, dword ptr [esp + 0x24]
		fstp st(0)
		fld dword ptr [esi + 0xcc]
		fmul dword ptr [edi + 0x1c]
		fld dword ptr [edi + 0x18]
		sub edx, eax
		mov dword ptr [esp + 0x24], edx
		fmul dword ptr [esi + 0xc8]
		faddp st(1), st(0)
		fld dword ptr [esi + 0xc4]
		fmul dword ptr [edi + 0x14]
		faddp st(1), st(0)
		fld dword ptr [esi + 0x70]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x74]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x78]
		mov edx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub edx, eax
		mov dword ptr [esp + 0x28], edx
		call _ftol
		fstp st(0)
		fld dword ptr [esi + 0xd8]
		fmul dword ptr [edi + 0x1c]
		fld dword ptr [edi + 0x14]
		fmul dword ptr [esi + 0xd0]
		mov ecx, dword ptr [esp + 0x24]
		faddp st(1), st(0)
		fld dword ptr [edi + 0x18]
		fmul dword ptr [esi + 0xd4]
		sub ecx, eax
		mov dword ptr [esp + 0x24], ecx
		faddp st(1), st(0)
		fld dword ptr [esi + 0x7c]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x80]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x84]
		mov ecx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		mov ecx, dword ptr [esp + 0x24]
		sub ecx, eax
		mov eax, dword ptr [esp + 0x14]
		cmp ebx, eax
		fstp st(0)
		jge L_LightVertices4_2
		mov byte ptr [esp + 0x10], al
		jmp L_LightVertices4_3
	L_LightVertices4_2:
		cmp ebx, 0xff
		mov byte ptr [esp + 0x10], 0xff
		jg L_LightVertices4_3
		mov byte ptr [esp + 0x10], bl
	L_LightVertices4_3:
		mov eax, dword ptr [esp + 0x28]
		mov edx, dword ptr [esp + 0x18]
		cmp eax, edx
		jge L_LightVertices4_4
		mov byte ptr [esp + 0x11], dl
		jmp L_LightVertices4_5
	L_LightVertices4_4:
		cmp eax, 0xff
		mov byte ptr [esp + 0x11], 0xff
		jg L_LightVertices4_5
		mov byte ptr [esp + 0x11], al
	L_LightVertices4_5:
		mov eax, dword ptr [esp + 0x1c]
		cmp ecx, eax
		jge L_LightVertices4_6
		mov byte ptr [esp + 0x12], al
		jmp L_LightVertices4_7
	L_LightVertices4_6:
		cmp ecx, 0xff
		mov byte ptr [esp + 0x12], 0xff
		jg L_LightVertices4_7
		mov byte ptr [esp + 0x12], cl
	L_LightVertices4_7:
		mov ecx, dword ptr [esp + 0x2c]
		lea ebx, [esp + 0x10]
		mov eax, ecx
		inc ecx
		mov dword ptr [esp + 0x2c], ecx
		mov ecx, dword ptr [esi + 0x38]
		push ebx
		push eax
		mov edx, dword ptr [ecx]
		call dword ptr [edx + 0x30]
		add edi, 0x20
		cmp edi, ebp
		jb L_LightVertices4_8
	L_LightVertices4_0:
		pop edi
		pop esi
		pop ebp
		pop ebx
		add esp, 0x10
		ret 0x10
	}
}

#endif

#ifdef COMPAT_MODE

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

#else

// FUNCTION: GOLDP 0x10024e90
// FUNCTION: LEGORACERS 0x0040f4c0
__declspec(naked) void GolModelRenderState::LightVertices5(
	const CommandVertex* p_vertices,
	LegoU32 p_outputFirst,
	LegoU32 p_firstVertex,
	LegoU32 p_vertexCount
)
{
	__asm {
		sub esp, 0x10
		push ebx
		push ebp
		mov ebp, dword ptr [esp + 0x1c]
		push esi
		push edi
		mov edi, dword ptr [esp + 0x28]
		mov esi, ecx
		xor eax, eax
		shl edi, 5
		mov al, byte ptr [esi + 0x31]
		add edi, ebp
		mov ebp, dword ptr [esp + 0x30]
		mov dword ptr [esp + 0x18], eax
		xor eax, eax
		xor ebx, ebx
		mov al, byte ptr [esi + 0x32]
		mov bl, byte ptr [esi + 0x30]
		shl ebp, 5
		mov dword ptr [esp + 0x1c], eax
		mov al, byte ptr [esi + 0x33]
		add ebp, edi
		mov dword ptr [esp + 0x14], ebx
		cmp edi, ebp
		mov byte ptr [esp + 0x13], al
		jae L_LightVertices5_0
		jmp L_LightVertices5_1
	L_LightVertices5_8:
		mov ebx, dword ptr [esp + 0x14]
	L_LightVertices5_1:
		fld dword ptr [edi + 0x14]
		fmul dword ptr [esi + 0xac]
		fld dword ptr [edi + 0x18]
		fmul dword ptr [esi + 0xb0]
		faddp st(1), st(0)
		fld dword ptr [edi + 0x1c]
		fmul dword ptr [esi + 0xb4]
		faddp st(1), st(0)
		fld dword ptr [esi + 0x58]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x5c]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x60]
		mov ecx, dword ptr [esp + 0x18]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		fstp st(0)
		fld dword ptr [esi + 0xbc]
		fmul dword ptr [edi + 0x18]
		fld dword ptr [esi + 0xc0]
		fmul dword ptr [edi + 0x1c]
		mov ecx, dword ptr [esp + 0x1c]
		faddp st(1), st(0)
		fld dword ptr [esi + 0xb8]
		fmul dword ptr [edi + 0x14]
		sub ecx, eax
		mov dword ptr [esp + 0x24], ecx
		faddp st(1), st(0)
		fld dword ptr [esi + 0x64]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x68]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x6c]
		mov edx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub edx, eax
		mov dword ptr [esp + 0x28], edx
		call _ftol
		mov ecx, dword ptr [esp + 0x24]
		fstp st(0)
		fld dword ptr [esi + 0xcc]
		fmul dword ptr [edi + 0x1c]
		fld dword ptr [edi + 0x18]
		sub ecx, eax
		mov dword ptr [esp + 0x24], ecx
		fmul dword ptr [esi + 0xc8]
		faddp st(1), st(0)
		fld dword ptr [esi + 0xc4]
		fmul dword ptr [edi + 0x14]
		faddp st(1), st(0)
		fld dword ptr [esi + 0x70]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x74]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x78]
		mov ecx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		fstp st(0)
		fld dword ptr [esi + 0xd8]
		fmul dword ptr [edi + 0x1c]
		fld dword ptr [edi + 0x14]
		fmul dword ptr [esi + 0xd0]
		mov edx, dword ptr [esp + 0x24]
		faddp st(1), st(0)
		fld dword ptr [edi + 0x18]
		fmul dword ptr [esi + 0xd4]
		sub edx, eax
		mov dword ptr [esp + 0x24], edx
		faddp st(1), st(0)
		fld dword ptr [esi + 0x7c]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x80]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x84]
		mov edx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub edx, eax
		mov dword ptr [esp + 0x28], edx
		call _ftol
		fstp st(0)
		fld dword ptr [esi + 0xdc]
		fmul dword ptr [edi + 0x14]
		fld dword ptr [esi + 0xe0]
		mov ecx, dword ptr [esp + 0x24]
		fmul dword ptr [edi + 0x18]
		sub ecx, eax
		faddp st(1), st(0)
		fld dword ptr [esi + 0xe4]
		mov dword ptr [esp + 0x24], ecx
		fmul dword ptr [edi + 0x1c]
		faddp st(1), st(0)
		fld dword ptr [esi + 0x88]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x8c]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x90]
		mov ecx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		mov ecx, dword ptr [esp + 0x24]
		sub ecx, eax
		mov eax, dword ptr [esp + 0x14]
		cmp ebx, eax
		fstp st(0)
		jge L_LightVertices5_2
		mov byte ptr [esp + 0x10], al
		jmp L_LightVertices5_3
	L_LightVertices5_2:
		cmp ebx, 0xff
		mov byte ptr [esp + 0x10], 0xff
		jg L_LightVertices5_3
		mov byte ptr [esp + 0x10], bl
	L_LightVertices5_3:
		mov eax, dword ptr [esp + 0x28]
		mov edx, dword ptr [esp + 0x18]
		cmp eax, edx
		jge L_LightVertices5_4
		mov byte ptr [esp + 0x11], dl
		jmp L_LightVertices5_5
	L_LightVertices5_4:
		cmp eax, 0xff
		mov byte ptr [esp + 0x11], 0xff
		jg L_LightVertices5_5
		mov byte ptr [esp + 0x11], al
	L_LightVertices5_5:
		mov eax, dword ptr [esp + 0x1c]
		cmp ecx, eax
		jge L_LightVertices5_6
		mov byte ptr [esp + 0x12], al
		jmp L_LightVertices5_7
	L_LightVertices5_6:
		cmp ecx, 0xff
		mov byte ptr [esp + 0x12], 0xff
		jg L_LightVertices5_7
		mov byte ptr [esp + 0x12], cl
	L_LightVertices5_7:
		mov ecx, dword ptr [esp + 0x2c]
		lea ebx, [esp + 0x10]
		mov eax, ecx
		inc ecx
		mov dword ptr [esp + 0x2c], ecx
		mov ecx, dword ptr [esi + 0x38]
		push ebx
		push eax
		mov edx, dword ptr [ecx]
		call dword ptr [edx + 0x30]
		add edi, 0x20
		cmp edi, ebp
		jb L_LightVertices5_8
	L_LightVertices5_0:
		pop edi
		pop esi
		pop ebp
		pop ebx
		add esp, 0x10
		ret 0x10
	}
}

#endif

#ifdef COMPAT_MODE

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

#else

// FUNCTION: GOLDP 0x10025130
// FUNCTION: LEGORACERS 0x0040f760
__declspec(naked) void GolModelRenderState::LightVertices6(
	const CommandVertex* p_vertices,
	LegoU32 p_outputFirst,
	LegoU32 p_firstVertex,
	LegoU32 p_vertexCount
)
{
	__asm {
		sub esp, 0x10
		push ebx
		push ebp
		mov ebp, dword ptr [esp + 0x1c]
		push esi
		push edi
		mov edi, dword ptr [esp + 0x28]
		mov esi, ecx
		xor eax, eax
		shl edi, 5
		mov al, byte ptr [esi + 0x31]
		add edi, ebp
		mov ebp, dword ptr [esp + 0x30]
		mov dword ptr [esp + 0x18], eax
		xor eax, eax
		xor ebx, ebx
		mov al, byte ptr [esi + 0x32]
		mov bl, byte ptr [esi + 0x30]
		shl ebp, 5
		mov dword ptr [esp + 0x1c], eax
		mov al, byte ptr [esi + 0x33]
		add ebp, edi
		mov dword ptr [esp + 0x14], ebx
		cmp edi, ebp
		mov byte ptr [esp + 0x13], al
		jae L_LightVertices6_0
		jmp L_LightVertices6_1
	L_LightVertices6_8:
		mov ebx, dword ptr [esp + 0x14]
	L_LightVertices6_1:
		fld dword ptr [edi + 0x14]
		fmul dword ptr [esi + 0xac]
		fld dword ptr [edi + 0x18]
		fmul dword ptr [esi + 0xb0]
		faddp st(1), st(0)
		fld dword ptr [edi + 0x1c]
		fmul dword ptr [esi + 0xb4]
		faddp st(1), st(0)
		fld dword ptr [esi + 0x58]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x5c]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x60]
		mov ecx, dword ptr [esp + 0x18]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		fstp st(0)
		fld dword ptr [esi + 0xbc]
		fmul dword ptr [edi + 0x18]
		fld dword ptr [esi + 0xc0]
		fmul dword ptr [edi + 0x1c]
		mov ecx, dword ptr [esp + 0x1c]
		faddp st(1), st(0)
		fld dword ptr [esi + 0xb8]
		fmul dword ptr [edi + 0x14]
		sub ecx, eax
		mov dword ptr [esp + 0x24], ecx
		faddp st(1), st(0)
		fld dword ptr [esi + 0x64]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x68]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x6c]
		mov ecx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		mov edx, dword ptr [esp + 0x24]
		fstp st(0)
		fld dword ptr [esi + 0xcc]
		fmul dword ptr [edi + 0x1c]
		fld dword ptr [edi + 0x18]
		sub edx, eax
		mov dword ptr [esp + 0x24], edx
		fmul dword ptr [esi + 0xc8]
		faddp st(1), st(0)
		fld dword ptr [esi + 0xc4]
		fmul dword ptr [edi + 0x14]
		faddp st(1), st(0)
		fld dword ptr [esi + 0x70]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x74]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x78]
		mov edx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub edx, eax
		mov dword ptr [esp + 0x28], edx
		call _ftol
		fstp st(0)
		fld dword ptr [esi + 0xd8]
		fmul dword ptr [edi + 0x1c]
		fld dword ptr [edi + 0x14]
		fmul dword ptr [esi + 0xd0]
		mov ecx, dword ptr [esp + 0x24]
		faddp st(1), st(0)
		fld dword ptr [edi + 0x18]
		fmul dword ptr [esi + 0xd4]
		sub ecx, eax
		mov dword ptr [esp + 0x24], ecx
		faddp st(1), st(0)
		fld dword ptr [esi + 0x7c]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x80]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x84]
		mov ecx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		fstp st(0)
		fld dword ptr [esi + 0xdc]
		fmul dword ptr [edi + 0x14]
		fld dword ptr [esi + 0xe0]
		mov edx, dword ptr [esp + 0x24]
		fmul dword ptr [edi + 0x18]
		sub edx, eax
		faddp st(1), st(0)
		fld dword ptr [esi + 0xe4]
		mov dword ptr [esp + 0x24], edx
		fmul dword ptr [edi + 0x1c]
		faddp st(1), st(0)
		fld dword ptr [esi + 0x88]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x8c]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x90]
		mov edx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub edx, eax
		mov dword ptr [esp + 0x28], edx
		call _ftol
		fstp st(0)
		fld dword ptr [esi + 0xec]
		fmul dword ptr [edi + 0x18]
		fld dword ptr [esi + 0xf0]
		fmul dword ptr [edi + 0x1c]
		mov ecx, dword ptr [esp + 0x24]
		faddp st(1), st(0)
		fld dword ptr [esi + 0xe8]
		fmul dword ptr [edi + 0x14]
		sub ecx, eax
		mov dword ptr [esp + 0x24], ecx
		faddp st(1), st(0)
		fld dword ptr [esi + 0x94]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x98]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x9c]
		mov ecx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		mov ecx, dword ptr [esp + 0x24]
		sub ecx, eax
		mov eax, dword ptr [esp + 0x14]
		cmp ebx, eax
		fstp st(0)
		jge L_LightVertices6_2
		mov byte ptr [esp + 0x10], al
		jmp L_LightVertices6_3
	L_LightVertices6_2:
		cmp ebx, 0xff
		mov byte ptr [esp + 0x10], 0xff
		jg L_LightVertices6_3
		mov byte ptr [esp + 0x10], bl
	L_LightVertices6_3:
		mov eax, dword ptr [esp + 0x28]
		mov edx, dword ptr [esp + 0x18]
		cmp eax, edx
		jge L_LightVertices6_4
		mov byte ptr [esp + 0x11], dl
		jmp L_LightVertices6_5
	L_LightVertices6_4:
		cmp eax, 0xff
		mov byte ptr [esp + 0x11], 0xff
		jg L_LightVertices6_5
		mov byte ptr [esp + 0x11], al
	L_LightVertices6_5:
		mov eax, dword ptr [esp + 0x1c]
		cmp ecx, eax
		jge L_LightVertices6_6
		mov byte ptr [esp + 0x12], al
		jmp L_LightVertices6_7
	L_LightVertices6_6:
		cmp ecx, 0xff
		mov byte ptr [esp + 0x12], 0xff
		jg L_LightVertices6_7
		mov byte ptr [esp + 0x12], cl
	L_LightVertices6_7:
		mov ecx, dword ptr [esp + 0x2c]
		lea ebx, [esp + 0x10]
		mov eax, ecx
		inc ecx
		mov dword ptr [esp + 0x2c], ecx
		mov ecx, dword ptr [esi + 0x38]
		push ebx
		push eax
		mov edx, dword ptr [ecx]
		call dword ptr [edx + 0x30]
		add edi, 0x20
		cmp edi, ebp
		jb L_LightVertices6_8
	L_LightVertices6_0:
		pop edi
		pop esi
		pop ebp
		pop ebx
		add esp, 0x10
		ret 0x10
	}
}

#endif

#ifdef COMPAT_MODE

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

#else

// FUNCTION: GOLDP 0x10025420
// FUNCTION: LEGORACERS 0x0040fa50
__declspec(naked) void GolModelRenderState::LightVertices7(
	const CommandVertex* p_vertices,
	LegoU32 p_outputFirst,
	LegoU32 p_firstVertex,
	LegoU32 p_vertexCount
)
{
	__asm {
		sub esp, 0x10
		push ebx
		push ebp
		mov ebp, dword ptr [esp + 0x1c]
		push esi
		push edi
		mov edi, dword ptr [esp + 0x28]
		mov esi, ecx
		xor eax, eax
		shl edi, 5
		mov al, byte ptr [esi + 0x31]
		add edi, ebp
		mov ebp, dword ptr [esp + 0x30]
		mov dword ptr [esp + 0x18], eax
		xor eax, eax
		xor ebx, ebx
		mov al, byte ptr [esi + 0x32]
		mov bl, byte ptr [esi + 0x30]
		shl ebp, 5
		mov dword ptr [esp + 0x1c], eax
		mov al, byte ptr [esi + 0x33]
		add ebp, edi
		mov dword ptr [esp + 0x14], ebx
		cmp edi, ebp
		mov byte ptr [esp + 0x13], al
		jae L_LightVertices7_0
		jmp L_LightVertices7_1
	L_LightVertices7_8:
		mov ebx, dword ptr [esp + 0x14]
	L_LightVertices7_1:
		fld dword ptr [edi + 0x14]
		fmul dword ptr [esi + 0xac]
		fld dword ptr [edi + 0x18]
		fmul dword ptr [esi + 0xb0]
		faddp st(1), st(0)
		fld dword ptr [edi + 0x1c]
		fmul dword ptr [esi + 0xb4]
		faddp st(1), st(0)
		fld dword ptr [esi + 0x58]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x5c]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x60]
		mov ecx, dword ptr [esp + 0x18]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		fstp st(0)
		fld dword ptr [esi + 0xbc]
		fmul dword ptr [edi + 0x18]
		fld dword ptr [esi + 0xc0]
		fmul dword ptr [edi + 0x1c]
		mov ecx, dword ptr [esp + 0x1c]
		faddp st(1), st(0)
		fld dword ptr [esi + 0xb8]
		fmul dword ptr [edi + 0x14]
		sub ecx, eax
		mov dword ptr [esp + 0x24], ecx
		faddp st(1), st(0)
		fld dword ptr [esi + 0x64]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x68]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x6c]
		mov edx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub edx, eax
		mov dword ptr [esp + 0x28], edx
		call _ftol
		mov ecx, dword ptr [esp + 0x24]
		fstp st(0)
		fld dword ptr [esi + 0xcc]
		fmul dword ptr [edi + 0x1c]
		fld dword ptr [edi + 0x18]
		sub ecx, eax
		mov dword ptr [esp + 0x24], ecx
		fmul dword ptr [esi + 0xc8]
		faddp st(1), st(0)
		fld dword ptr [esi + 0xc4]
		fmul dword ptr [edi + 0x14]
		faddp st(1), st(0)
		fld dword ptr [esi + 0x70]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x74]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x78]
		mov ecx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		fstp st(0)
		fld dword ptr [esi + 0xd8]
		fmul dword ptr [edi + 0x1c]
		fld dword ptr [edi + 0x14]
		fmul dword ptr [esi + 0xd0]
		mov edx, dword ptr [esp + 0x24]
		faddp st(1), st(0)
		fld dword ptr [edi + 0x18]
		fmul dword ptr [esi + 0xd4]
		sub edx, eax
		mov dword ptr [esp + 0x24], edx
		faddp st(1), st(0)
		fld dword ptr [esi + 0x7c]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x80]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x84]
		mov edx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub edx, eax
		mov dword ptr [esp + 0x28], edx
		call _ftol
		fstp st(0)
		fld dword ptr [esi + 0xdc]
		fmul dword ptr [edi + 0x14]
		fld dword ptr [esi + 0xe0]
		mov ecx, dword ptr [esp + 0x24]
		fmul dword ptr [edi + 0x18]
		sub ecx, eax
		faddp st(1), st(0)
		fld dword ptr [esi + 0xe4]
		mov dword ptr [esp + 0x24], ecx
		fmul dword ptr [edi + 0x1c]
		faddp st(1), st(0)
		fld dword ptr [esi + 0x88]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x8c]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x90]
		mov ecx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		fstp st(0)
		fld dword ptr [esi + 0xec]
		fmul dword ptr [edi + 0x18]
		fld dword ptr [esi + 0xf0]
		fmul dword ptr [edi + 0x1c]
		mov edx, dword ptr [esp + 0x24]
		faddp st(1), st(0)
		fld dword ptr [esi + 0xe8]
		fmul dword ptr [edi + 0x14]
		sub edx, eax
		mov dword ptr [esp + 0x24], edx
		faddp st(1), st(0)
		fld dword ptr [esi + 0x94]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0x98]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0x9c]
		mov edx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub edx, eax
		mov dword ptr [esp + 0x28], edx
		call _ftol
		fstp st(0)
		fld dword ptr [esi + 0xfc]
		fmul dword ptr [edi + 0x1c]
		fld dword ptr [edi + 0x18]
		fmul dword ptr [esi + 0xf8]
		mov ecx, dword ptr [esp + 0x24]
		faddp st(1), st(0)
		fld dword ptr [esi + 0xf4]
		fmul dword ptr [edi + 0x14]
		sub ecx, eax
		mov dword ptr [esp + 0x24], ecx
		faddp st(1), st(0)
		fld dword ptr [esi + 0xa0]
		fmul st(0), st(1)
		call _ftol
		fld dword ptr [esi + 0xa4]
		fmul st(0), st(1)
		sub ebx, eax
		call _ftol
		fld dword ptr [esi + 0xa8]
		mov ecx, dword ptr [esp + 0x28]
		fmul st(0), st(1)
		sub ecx, eax
		mov dword ptr [esp + 0x28], ecx
		call _ftol
		mov ecx, dword ptr [esp + 0x24]
		sub ecx, eax
		mov eax, dword ptr [esp + 0x14]
		cmp ebx, eax
		fstp st(0)
		jge L_LightVertices7_2
		mov byte ptr [esp + 0x10], al
		jmp L_LightVertices7_3
	L_LightVertices7_2:
		cmp ebx, 0xff
		mov byte ptr [esp + 0x10], 0xff
		jg L_LightVertices7_3
		mov byte ptr [esp + 0x10], bl
	L_LightVertices7_3:
		mov eax, dword ptr [esp + 0x28]
		mov edx, dword ptr [esp + 0x18]
		cmp eax, edx
		jge L_LightVertices7_4
		mov byte ptr [esp + 0x11], dl
		jmp L_LightVertices7_5
	L_LightVertices7_4:
		cmp eax, 0xff
		mov byte ptr [esp + 0x11], 0xff
		jg L_LightVertices7_5
		mov byte ptr [esp + 0x11], al
	L_LightVertices7_5:
		mov eax, dword ptr [esp + 0x1c]
		cmp ecx, eax
		jge L_LightVertices7_6
		mov byte ptr [esp + 0x12], al
		jmp L_LightVertices7_7
	L_LightVertices7_6:
		cmp ecx, 0xff
		mov byte ptr [esp + 0x12], 0xff
		jg L_LightVertices7_7
		mov byte ptr [esp + 0x12], cl
	L_LightVertices7_7:
		mov ecx, dword ptr [esp + 0x2c]
		lea ebx, [esp + 0x10]
		mov eax, ecx
		inc ecx
		mov dword ptr [esp + 0x2c], ecx
		mov ecx, dword ptr [esi + 0x38]
		push ebx
		push eax
		mov edx, dword ptr [ecx]
		call dword ptr [edx + 0x30]
		add edi, 0x20
		cmp edi, ebp
		jb L_LightVertices7_8
	L_LightVertices7_0:
		pop edi
		pop esi
		pop ebp
		pop ebx
		add esp, 0x10
		ret 0x10
	}
}

#endif

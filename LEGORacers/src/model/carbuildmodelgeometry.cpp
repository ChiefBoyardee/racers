#include "audio/directsoundconversions.h"
#include "core/gol.h"
#include "gdbmodelindexarray.h"
#include "gdbvertexarray.h"
#include "golanimatedentity.h"
#include "golmaterial.h"
#include "golmateriallibrary.h"
#include "golmodelbase.h"
#include "goltexture.h"
#include "material/goltexturelist.h"
#include "model/carbuildmodel.h"
#include "model/legocolortable.h"
#include "render/gold3drenderdevice.h"
#include "surface/color.h"
#include "surface/gold3dtexture.h"
#include "world/golworlddatabase.h"

#include <float.h>
#include <string.h>

DECOMP_SIZE_ASSERT(CarBuildModel::BuildVertex, 0x28)
DECOMP_SIZE_ASSERT(CarBuildModel::BuildPrimitive, 0x10)
DECOMP_SIZE_ASSERT(CarBuildModel::BuildPrimitiveBounds, 0x18)
DECOMP_SIZE_ASSERT(CarBuildModel::VertexUse, 0x04)

// GLOBAL: LEGORACERS 0x004ceba4
static CarBuildModel::BuildVertex* g_carBuildModelSortVertices;

// GLOBAL: LEGORACERS 0x004c7674
static CarBuildModel::VertexUse g_carBuildModelVertexUses[CarBuildModel::c_buildVertexCapacity];

// GLOBAL: LEGORACERS 0x004b480c
static const LegoFloat g_carBuildModelNegativePlaneEpsilon = -0.0049999999f;

// FUNCTION: LEGORACERS 0x00495020
LegoS32 CarBuildModel::ResetBuildVertexTree()
{
	m_buildVertexTreeRoot = NULL;
	m_buildVertexCount = 0;

	return 0;
}

// FUNCTION: LEGORACERS 0x00495030
LegoS32 CarBuildModel::AddBuildVertex(BuildVertex* p_vertex)
{
	BuildVertex* vertex = &m_buildVertices[m_buildVertexCount];
	*vertex = *p_vertex;
	m_buildVertices[m_buildVertexCount].m_index = static_cast<LegoU16>(m_buildVertexCount);

	BuildVertex* existing = InsertOrFindBuildVertex(&m_buildVertexTreeRoot, &m_buildVertices[m_buildVertexCount]);
	if (existing != NULL) {
		return existing->m_index;
	}

	m_buildVertexCount++;

	return m_buildVertexCount - 1;
}

// FUNCTION: LEGORACERS 0x004950c0
CarBuildModel::BuildVertex* CarBuildModel::InsertOrFindBuildVertex(BuildVertex** p_root, BuildVertex* p_vertex)
{
	p_vertex->m_left = NULL;
	p_vertex->m_right = NULL;

	BuildVertex* node = *p_root;
	if (node != NULL) {
		do {
			LegoS32 compare = CompareBuildVertex(p_vertex, node);
			if (compare != 0) {
				if (compare > 0) {
					if (node->m_right == NULL) {
						p_vertex->m_parent = node;
						node->m_right = p_vertex;
						node = NULL;
						break;
					}

					node = node->m_right;
				}
				else {
					if (node->m_left == NULL) {
						p_vertex->m_parent = node;
						node->m_left = p_vertex;
						node = NULL;
						break;
					}

					node = node->m_left;
				}
			}
			else {
				break;
			}
		} while (node != NULL);

		if (node != NULL) {
			return node;
		}

		RebalanceBuildVertexInsert(p_root, p_vertex);
		return NULL;
	}

	p_vertex->m_parent = NULL;
	p_vertex->m_treeBlack = TRUE;
	*p_root = p_vertex;

	return NULL;
}

// FUNCTION: LEGORACERS 0x00495160
LegoS32 __fastcall CarBuildModel::CompareBuildVertex(const BuildVertex* p_lhs, const BuildVertex* p_rhs)
{
	LegoS32 lhsKey;
	LegoS32 rhsKey;
	::memcpy(&lhsKey, &p_lhs->m_position.m_x, sizeof(lhsKey));
	::memcpy(&rhsKey, &p_rhs->m_position.m_x, sizeof(rhsKey));
	LegoS32 result = lhsKey - rhsKey;
	if (result != 0) {
		return result;
	}

	::memcpy(&lhsKey, &p_lhs->m_position.m_y, sizeof(lhsKey));
	::memcpy(&rhsKey, &p_rhs->m_position.m_y, sizeof(rhsKey));
	result = lhsKey - rhsKey;
	if (result != 0) {
		return result;
	}

	::memcpy(&lhsKey, &p_lhs->m_position.m_z, sizeof(lhsKey));
	::memcpy(&rhsKey, &p_rhs->m_position.m_z, sizeof(rhsKey));
	result = lhsKey - rhsKey;
	if (result != 0) {
		return result;
	}

	result = static_cast<LegoU8>(p_lhs->m_normalX) - static_cast<LegoU8>(p_rhs->m_normalX);
	if (result != 0) {
		return result;
	}

	result = static_cast<LegoU8>(p_lhs->m_normalY) - static_cast<LegoU8>(p_rhs->m_normalY);
	if (result != 0) {
		return result;
	}

	result = static_cast<LegoU8>(p_lhs->m_normalZ) - static_cast<LegoU8>(p_rhs->m_normalZ);
	if (result != 0) {
		return result;
	}

	::memcpy(&lhsKey, &p_lhs->m_textureCoordinate.m_x, sizeof(lhsKey));
	::memcpy(&rhsKey, &p_rhs->m_textureCoordinate.m_x, sizeof(rhsKey));
	result = lhsKey - rhsKey;
	if (result != 0) {
		return result;
	}

	::memcpy(&lhsKey, &p_lhs->m_textureCoordinate.m_y, sizeof(lhsKey));
	::memcpy(&rhsKey, &p_rhs->m_textureCoordinate.m_y, sizeof(rhsKey));
	return lhsKey - rhsKey;
}

// FUNCTION: LEGORACERS 0x004951c0
void __fastcall CarBuildModel::RebalanceBuildVertexInsert(BuildVertex** p_root, BuildVertex* p_vertex)
{
	p_vertex->m_treeBlack = FALSE;

	while (p_vertex->m_parent != NULL && !p_vertex->m_parent->m_treeBlack) {
		if (p_vertex->m_parent == p_vertex->m_parent->m_parent->m_left) {
			BuildVertex* uncle = p_vertex->m_parent->m_parent->m_right;
			if (uncle != NULL && !uncle->m_treeBlack) {
				uncle->m_treeBlack = TRUE;
				p_vertex->m_parent->m_treeBlack = TRUE;
				p_vertex = p_vertex->m_parent->m_parent;
				p_vertex->m_treeBlack = FALSE;
			}
			else {
				if (p_vertex == p_vertex->m_parent->m_right) {
					p_vertex = p_vertex->m_parent;
					RotateBuildVertexLeft(p_root, p_vertex);
				}

				p_vertex->m_parent->m_treeBlack = TRUE;
				p_vertex->m_parent->m_parent->m_treeBlack = FALSE;
				RotateBuildVertexRight(p_root, p_vertex->m_parent->m_parent);
			}
		}
		else {
			BuildVertex* uncle = p_vertex->m_parent->m_parent->m_left;
			if (uncle != NULL && !uncle->m_treeBlack) {
				uncle->m_treeBlack = TRUE;
				p_vertex->m_parent->m_treeBlack = TRUE;
				p_vertex = p_vertex->m_parent->m_parent;
				p_vertex->m_treeBlack = FALSE;
			}
			else {
				if (p_vertex == p_vertex->m_parent->m_left) {
					p_vertex = p_vertex->m_parent;
					RotateBuildVertexRight(p_root, p_vertex);
				}

				p_vertex->m_parent->m_treeBlack = TRUE;
				p_vertex->m_parent->m_parent->m_treeBlack = FALSE;
				RotateBuildVertexLeft(p_root, p_vertex->m_parent->m_parent);
			}
		}
	}

	(*p_root)->m_treeBlack = TRUE;
}

// FUNCTION: LEGORACERS 0x00495290
void __fastcall CarBuildModel::RotateBuildVertexLeft(BuildVertex** p_root, BuildVertex* p_vertex)
{
	BuildVertex* pivot = p_vertex->m_right;
	BuildVertex* child = pivot->m_left;
	p_vertex->m_right = child;
	if (child != NULL) {
		child->m_parent = p_vertex;
	}

	BuildVertex* parent = p_vertex->m_parent;
	pivot->m_parent = parent;
	if (parent != NULL) {
		if (p_vertex == p_vertex->m_parent->m_left) {
			p_vertex->m_parent->m_left = pivot;
		}
		else {
			p_vertex->m_parent->m_right = pivot;
		}
	}
	else {
		*p_root = pivot;
	}

	pivot->m_left = p_vertex;
	p_vertex->m_parent = pivot;
}

// FUNCTION: LEGORACERS 0x004952d0
void __fastcall CarBuildModel::RotateBuildVertexRight(BuildVertex** p_root, BuildVertex* p_vertex)
{
	BuildVertex* pivot = p_vertex->m_left;
	BuildVertex* child = pivot->m_right;
	p_vertex->m_left = child;
	if (child != NULL) {
		child->m_parent = p_vertex;
	}

	BuildVertex* parent = p_vertex->m_parent;
	pivot->m_parent = parent;
	if (parent != NULL) {
		if (p_vertex == p_vertex->m_parent->m_left) {
			p_vertex->m_parent->m_left = pivot;
		}
		else {
			p_vertex->m_parent->m_right = pivot;
		}
	}
	else {
		*p_root = pivot;
	}

	pivot->m_right = p_vertex;
	p_vertex->m_parent = pivot;
}

// FUNCTION: LEGORACERS 0x00495310
LegoS32 CarBuildModel::ComparePrimitiveDrawOrder(const void* p_lhs, const void* p_rhs)
{
	const BuildPrimitive* lhs = *static_cast<BuildPrimitive* const*>(p_lhs);
	const BuildPrimitive* rhs = *static_cast<BuildPrimitive* const*>(p_rhs);

	LegoS32 result = lhs->m_materialIndex - rhs->m_materialIndex;
	if (result == 0) {
		LegoFloat delta = g_carBuildModelSortVertices[lhs->m_vertexIndices[0]].m_position.m_z -
						  g_carBuildModelSortVertices[rhs->m_vertexIndices[0]].m_position.m_z;
		::memcpy(&result, &delta, sizeof(result));
	}

	return result;
}

// FUNCTION: LEGORACERS 0x00495360
void CarBuildModel::GetBuildPrimitiveBounds(BuildPrimitive* p_primitive, BuildPrimitiveBounds* p_bounds)
{
	BuildVertex* vertex = &g_carBuildModelSortVertices[p_primitive->m_vertexIndices[0]];

	p_bounds->m_minX = p_bounds->m_maxX = vertex->m_position.m_x;
	p_bounds->m_minY = p_bounds->m_maxY = vertex->m_position.m_y;
	p_bounds->m_minZ = p_bounds->m_maxZ = vertex->m_position.m_z;

	for (LegoS32 i = 1; i < p_primitive->m_vertexCount; i++) {
		vertex = &g_carBuildModelSortVertices[p_primitive->m_vertexIndices[i]];

		if (p_bounds->m_minX > vertex->m_position.m_x) {
			p_bounds->m_minX = vertex->m_position.m_x;
		}
		if (p_bounds->m_maxX < vertex->m_position.m_x) {
			p_bounds->m_maxX = vertex->m_position.m_x;
		}
		if (p_bounds->m_minY > vertex->m_position.m_y) {
			p_bounds->m_minY = vertex->m_position.m_y;
		}
		if (p_bounds->m_maxY < vertex->m_position.m_y) {
			p_bounds->m_maxY = vertex->m_position.m_y;
		}
		if (p_bounds->m_minZ > vertex->m_position.m_z) {
			p_bounds->m_minZ = vertex->m_position.m_z;
		}
		if (p_bounds->m_maxZ < vertex->m_position.m_z) {
			p_bounds->m_maxZ = vertex->m_position.m_z;
		}
	}
}

// FUNCTION: LEGORACERS 0x00495440
void CarBuildModel::ResolvePrimitiveIntersections()
{
#define SWAP_ACTIVE_BUILD_PRIMITIVES()                                                                                 \
	do {                                                                                                               \
		swappedPrimitive = m_buildPrimitiveOrder[lhsIndex];                                                            \
		m_buildPrimitiveOrder[lhsIndex] = m_buildPrimitiveOrder[rhsIndex];                                             \
		m_buildPrimitiveOrder[rhsIndex] = swappedPrimitive;                                                            \
		swappedBounds = lhsBounds;                                                                                     \
		lhsBounds = rhsBounds;                                                                                         \
		rhsBounds = swappedBounds;                                                                                     \
	} while (0)

#define ROTATE_BUILD_PRIMITIVE_TO_MIN(coordMember, minField)                                                           \
	for (;;) {                                                                                                         \
		BuildVertex* vertices = m_buildVertices;                                                                       \
		LegoS32 old0 = primitive->m_vertexIndices[0];                                                                  \
		if (vertices[old0].m_position.coordMember == lhsBounds.minField &&                                             \
			vertices[primitive->m_vertexIndices[1]].m_position.coordMember == lhsBounds.minField) {                    \
			break;                                                                                                     \
		}                                                                                                              \
		LegoU16 old3 = primitive->m_vertexIndices[3];                                                                  \
		primitive->m_vertexIndices[0] = primitive->m_vertexIndices[1];                                                 \
		LegoU16 old2 = primitive->m_vertexIndices[2];                                                                  \
		primitive->m_vertexIndices[1] = old3;                                                                          \
		primitive->m_vertexIndices[3] = old2;                                                                          \
		primitive->m_vertexIndices[2] = old0;                                                                          \
	}

#define ROTATE_BUILD_PRIMITIVE_TO_MAX(coordMember, maxField)                                                           \
	while (m_buildVertices[primitive->m_vertexIndices[2]].m_position.coordMember != lhsBounds.maxField ||              \
		   m_buildVertices[primitive->m_vertexIndices[3]].m_position.coordMember != lhsBounds.maxField) {              \
		LegoU16 old2 = primitive->m_vertexIndices[2];                                                                  \
		LegoS32 old0 = primitive->m_vertexIndices[0];                                                                  \
		primitive->m_vertexIndices[0] = primitive->m_vertexIndices[1];                                                 \
		LegoU16 old3 = primitive->m_vertexIndices[3];                                                                  \
		primitive->m_vertexIndices[3] = old2;                                                                          \
		primitive->m_vertexIndices[1] = old3;                                                                          \
		primitive->m_vertexIndices[2] = old0;                                                                          \
	}

#define ALLOCATE_OUTSIDE_BUILD_PRIMITIVE_MIN()                                                                         \
	do {                                                                                                               \
		outsidePrimitive = m_buildPrimitiveOrder[m_buildPrimitiveCount];                                               \
		m_buildPrimitiveOrder[m_buildPrimitiveCount] = m_buildPrimitiveOrder[planePrimitiveCount];                     \
		m_buildPrimitiveOrder[planePrimitiveCount] = outsidePrimitive;                                                 \
		planePrimitiveCount++;                                                                                         \
		m_buildPrimitiveCount++;                                                                                       \
		*outsidePrimitive = *primitive;                                                                                \
		if (m_buildPrimitiveCount >= c_buildPrimitiveCapacity) {                                                       \
			m_buildStatus |= c_buildStatusOverflow;                                                                    \
			return;                                                                                                    \
		}                                                                                                              \
	} while (0)

#define ALLOCATE_OUTSIDE_BUILD_PRIMITIVE_MAX()                                                                         \
	outsidePrimitive = m_buildPrimitiveOrder[m_buildPrimitiveCount];                                                   \
	m_buildPrimitiveOrder[m_buildPrimitiveCount] = m_buildPrimitiveOrder[planePrimitiveCount];                         \
	m_buildPrimitiveOrder[planePrimitiveCount] = outsidePrimitive;                                                     \
	planePrimitiveCount++;                                                                                             \
	m_buildPrimitiveCount++;                                                                                           \
	*outsidePrimitive = *primitive;                                                                                    \
	if (m_buildPrimitiveCount >= c_buildPrimitiveCapacity) {                                                           \
		m_buildStatus |= c_buildStatusOverflow;                                                                        \
		return;                                                                                                        \
	}

#define SET_SPLIT_VERTEX_POSITION_X(vertex, boundsField, splitCoordinate)                                              \
	lhsBounds.boundsField = (splitCoordinate);                                                                         \
	splitVertex.m_position.m_x = lhsBounds.boundsField;                                                                \
	splitVertex.m_position.m_y = (vertex)->m_position.m_y;                                                             \
	splitVertex.m_position.m_z = (vertex)->m_position.m_z

#define SET_SPLIT_VERTEX_POSITION_Y(vertex, boundsField, splitCoordinate)                                              \
	splitVertex.m_position.m_x = (vertex)->m_position.m_x;                                                             \
	lhsBounds.boundsField = (splitCoordinate);                                                                         \
	splitVertex.m_position.m_y = lhsBounds.boundsField;                                                                \
	splitVertex.m_position.m_z = (vertex)->m_position.m_z

#define SET_SPLIT_VERTEX_POSITION_Z(vertex, boundsField, splitCoordinate)                                              \
	splitVertex.m_position.m_x = (vertex)->m_position.m_x;                                                             \
	splitVertex.m_position.m_y = (vertex)->m_position.m_y;                                                             \
	lhsBounds.boundsField = (splitCoordinate);                                                                         \
	splitVertex.m_position.m_z = lhsBounds.boundsField

#define CLIP_BUILD_PRIMITIVE_MIN(coordMember, minField, maxField, secondCoordMember, setPosition)                      \
	if (lhsBounds.minField != rhsBounds.minField) {                                                                    \
		if (rhsBounds.minField < lhsBounds.minField) {                                                                 \
			SWAP_ACTIVE_BUILD_PRIMITIVES();                                                                            \
		}                                                                                                              \
		primitive = m_buildPrimitiveOrder[lhsIndex];                                                                   \
		ROTATE_BUILD_PRIMITIVE_TO_MIN(coordMember, minField);                                                          \
		ALLOCATE_OUTSIDE_BUILD_PRIMITIVE_MIN();                                                                        \
		rightScale = rhsBounds.minField - lhsBounds.minField;                                                          \
		BuildVertex* firstLowVertex = &m_buildVertices[primitive->m_vertexIndices[0]];                                 \
		rightScale = rightScale / (lhsBounds.maxField - lhsBounds.minField);                                           \
		BuildVertex* firstHighVertex = &m_buildVertices[primitive->m_vertexIndices[2]];                                \
		setPosition(firstLowVertex, minField, rhsBounds.minField);                                                     \
		leftScale = 1.0f - rightScale;                                                                                 \
		InterpolateBuildVertex(&splitVertex, firstLowVertex, firstHighVertex, leftScale, rightScale);                  \
		splitFirstIndex = static_cast<LegoU16>(AddBuildVertex(&splitVertex));                                          \
		outsidePrimitive->m_vertexIndices[2] = splitFirstIndex;                                                        \
		primitive->m_vertexIndices[0] = splitFirstIndex;                                                               \
		secondLowVertex = &m_buildVertices[primitive->m_vertexIndices[1]];                                             \
		secondHighVertex = &m_buildVertices[primitive->m_vertexIndices[3]];                                            \
		splitVertex.m_position.secondCoordMember = secondLowVertex->m_position.secondCoordMember;                      \
		InterpolateBuildVertex(&splitVertex, secondLowVertex, secondHighVertex, leftScale, rightScale);                \
		splitSecondIndex = static_cast<LegoU16>(AddBuildVertex(&splitVertex));                                         \
		outsidePrimitive->m_vertexIndices[3] = splitSecondIndex;                                                       \
		primitive->m_vertexIndices[1] = splitSecondIndex;                                                              \
	}

#define CLIP_BUILD_PRIMITIVE_MAX(coordMember, minField, maxField, secondCoordMember, setPosition)                      \
	if (lhsBounds.maxField != rhsBounds.maxField) {                                                                    \
		if (rhsBounds.maxField > lhsBounds.maxField) {                                                                 \
			SWAP_ACTIVE_BUILD_PRIMITIVES();                                                                            \
		}                                                                                                              \
		primitive = m_buildPrimitiveOrder[lhsIndex];                                                                   \
		ROTATE_BUILD_PRIMITIVE_TO_MAX(coordMember, maxField);                                                          \
		ALLOCATE_OUTSIDE_BUILD_PRIMITIVE_MAX();                                                                        \
		rightScale = rhsBounds.maxField - lhsBounds.minField;                                                          \
		BuildVertex* firstLowVertex = &m_buildVertices[primitive->m_vertexIndices[0]];                                 \
		rightScale = rightScale / (lhsBounds.maxField - lhsBounds.minField);                                           \
		BuildVertex* firstHighVertex = &m_buildVertices[primitive->m_vertexIndices[2]];                                \
		setPosition(firstLowVertex, maxField, rhsBounds.maxField);                                                     \
		leftScale = 1.0f - rightScale;                                                                                 \
		InterpolateBuildVertex(&splitVertex, firstLowVertex, firstHighVertex, leftScale, rightScale);                  \
		splitFirstIndex = static_cast<LegoU16>(AddBuildVertex(&splitVertex));                                          \
		outsidePrimitive->m_vertexIndices[0] = splitFirstIndex;                                                        \
		primitive->m_vertexIndices[2] = splitFirstIndex;                                                               \
		secondLowVertex = &m_buildVertices[primitive->m_vertexIndices[1]];                                             \
		secondHighVertex = &m_buildVertices[primitive->m_vertexIndices[3]];                                            \
		splitVertex.m_position.secondCoordMember = secondLowVertex->m_position.secondCoordMember;                      \
		InterpolateBuildVertex(&splitVertex, secondLowVertex, secondHighVertex, leftScale, rightScale);                \
		splitSecondIndex = static_cast<LegoU16>(AddBuildVertex(&splitVertex));                                         \
		outsidePrimitive->m_vertexIndices[1] = splitSecondIndex;                                                       \
		primitive->m_vertexIndices[3] = splitSecondIndex;                                                              \
	}

#define REMOVE_MATCHED_BUILD_PRIMITIVES()                                                                              \
	do {                                                                                                               \
		planePrimitiveCount--;                                                                                         \
		m_buildPrimitiveCount--;                                                                                       \
		BuildPrimitive* removedPrimitive = m_buildPrimitiveOrder[rhsIndex];                                            \
		m_buildPrimitiveOrder[rhsIndex] = m_buildPrimitiveOrder[planePrimitiveCount];                                  \
		m_buildPrimitiveOrder[planePrimitiveCount] = m_buildPrimitiveOrder[m_buildPrimitiveCount];                     \
		m_buildPrimitiveOrder[m_buildPrimitiveCount] = removedPrimitive;                                               \
		planePrimitiveCount--;                                                                                         \
		m_buildPrimitiveCount--;                                                                                       \
		removedPrimitive = m_buildPrimitiveOrder[lhsIndex];                                                            \
		m_buildPrimitiveOrder[lhsIndex] = m_buildPrimitiveOrder[planePrimitiveCount];                                  \
		m_buildPrimitiveOrder[planePrimitiveCount] = m_buildPrimitiveOrder[m_buildPrimitiveCount];                     \
		m_buildPrimitiveOrder[m_buildPrimitiveCount] = removedPrimitive;                                               \
		rhsIndex = lhsIndex;                                                                                           \
		GetBuildPrimitiveBounds(m_buildPrimitiveOrder[lhsIndex], &lhsBounds);                                          \
	} while (0)

	LegoS32 i;
	LegoS32 planePrimitiveCount;
	LegoS32 lhsIndex;
	LegoS32 rhsIndex;
	LegoU16 splitFirstIndex;
	LegoU16 splitSecondIndex;
	LegoFloat leftScale;
	LegoFloat rightScale;
	BuildPrimitive* primitive;
	BuildPrimitive* outsidePrimitive;
	BuildPrimitive* swappedPrimitive;
	BuildPrimitive* lhs;
	BuildPrimitiveBounds swappedBounds;
	BuildVertex* secondLowVertex;
	BuildVertex* secondHighVertex;
	BuildVertex splitVertex;

	{
		for (i = 0, planePrimitiveCount = 0; i < m_buildPrimitiveCount; i++) {
			BuildPrimitive* primitive = m_buildPrimitiveOrder[i];
			if ((primitive->m_flags & 0x89) == 0x89 && primitive->m_vertexCount == 4) {
				m_buildPrimitiveOrder[i] = m_buildPrimitiveOrder[planePrimitiveCount++];
				m_buildPrimitiveOrder[planePrimitiveCount - 1] = primitive;
			}
		}

		for (lhsIndex = 0; lhsIndex < planePrimitiveCount - 1; lhsIndex++) {
			BuildPrimitiveBounds lhsBounds;
			GetBuildPrimitiveBounds(m_buildPrimitiveOrder[lhsIndex], &lhsBounds);

			for (rhsIndex = lhsIndex + 1; rhsIndex < planePrimitiveCount; rhsIndex++) {
				BuildPrimitiveBounds rhsBounds;
				GetBuildPrimitiveBounds(m_buildPrimitiveOrder[rhsIndex], &rhsBounds);

				LegoFloat planeDelta = lhsBounds.m_minZ - rhsBounds.m_minZ;
				lhs = m_buildPrimitiveOrder[lhsIndex];
				BuildPrimitive* rhs = m_buildPrimitiveOrder[rhsIndex];
				if (g_carBuildModelNegativePlaneEpsilon < planeDelta && planeDelta < g_minAudibleSoundVolume &&
					lhs->m_partIndex != rhs->m_partIndex && lhsBounds.m_minX < rhsBounds.m_maxX &&
					rhsBounds.m_minX < lhsBounds.m_maxX && lhsBounds.m_minY < rhsBounds.m_maxY &&
					rhsBounds.m_minY < lhsBounds.m_maxY) {
					CLIP_BUILD_PRIMITIVE_MIN(m_y, m_minY, m_maxY, m_x, SET_SPLIT_VERTEX_POSITION_Y);
					CLIP_BUILD_PRIMITIVE_MAX(m_y, m_minY, m_maxY, m_x, SET_SPLIT_VERTEX_POSITION_Y);
					CLIP_BUILD_PRIMITIVE_MIN(m_x, m_minX, m_maxX, m_y, SET_SPLIT_VERTEX_POSITION_X);
					CLIP_BUILD_PRIMITIVE_MAX(m_x, m_minX, m_maxX, m_y, SET_SPLIT_VERTEX_POSITION_X);
					REMOVE_MATCHED_BUILD_PRIMITIVES();
				}
			}
		}
	}

	{
		for (i = 0, planePrimitiveCount = 0; i < m_buildPrimitiveCount; i++) {
			BuildPrimitive* primitive = m_buildPrimitiveOrder[i];
			if ((primitive->m_flags & 0x85) == 0x85 && primitive->m_vertexCount == 4) {
				m_buildPrimitiveOrder[i] = m_buildPrimitiveOrder[planePrimitiveCount++];
				m_buildPrimitiveOrder[planePrimitiveCount - 1] = primitive;
			}
		}

		for (lhsIndex = 0; lhsIndex < planePrimitiveCount - 1; lhsIndex++) {
			BuildPrimitiveBounds lhsBounds;
			GetBuildPrimitiveBounds(m_buildPrimitiveOrder[lhsIndex], &lhsBounds);

			for (rhsIndex = lhsIndex + 1; rhsIndex < planePrimitiveCount; rhsIndex++) {
				BuildPrimitiveBounds rhsBounds;
				GetBuildPrimitiveBounds(m_buildPrimitiveOrder[rhsIndex], &rhsBounds);

				lhs = m_buildPrimitiveOrder[lhsIndex];
				BuildPrimitive* rhs = m_buildPrimitiveOrder[rhsIndex];
				if (lhsBounds.m_minY == rhsBounds.m_minY && lhs->m_partIndex != rhs->m_partIndex &&
					lhsBounds.m_minX < rhsBounds.m_maxX && rhsBounds.m_minX < lhsBounds.m_maxX &&
					lhsBounds.m_minZ < rhsBounds.m_maxZ && rhsBounds.m_minZ < lhsBounds.m_maxZ) {
					CLIP_BUILD_PRIMITIVE_MIN(m_z, m_minZ, m_maxZ, m_x, SET_SPLIT_VERTEX_POSITION_Z);
					CLIP_BUILD_PRIMITIVE_MAX(m_z, m_minZ, m_maxZ, m_x, SET_SPLIT_VERTEX_POSITION_Z);
					CLIP_BUILD_PRIMITIVE_MIN(m_x, m_minX, m_maxX, m_z, SET_SPLIT_VERTEX_POSITION_X);
					CLIP_BUILD_PRIMITIVE_MAX(m_x, m_minX, m_maxX, m_z, SET_SPLIT_VERTEX_POSITION_X);
					REMOVE_MATCHED_BUILD_PRIMITIVES();
				}
			}
		}
	}

	{
		for (i = 0, planePrimitiveCount = 0; i < m_buildPrimitiveCount; i++) {
			BuildPrimitive* primitive = m_buildPrimitiveOrder[i];
			if ((primitive->m_flags & 0x83) == 0x83 && primitive->m_vertexCount == 4) {
				m_buildPrimitiveOrder[i] = m_buildPrimitiveOrder[planePrimitiveCount++];
				m_buildPrimitiveOrder[planePrimitiveCount - 1] = primitive;
			}
		}

		for (lhsIndex = 0; lhsIndex < planePrimitiveCount - 1; lhsIndex++) {
			BuildPrimitiveBounds lhsBounds;
			GetBuildPrimitiveBounds(m_buildPrimitiveOrder[lhsIndex], &lhsBounds);

			for (rhsIndex = lhsIndex + 1; rhsIndex < planePrimitiveCount; rhsIndex++) {
				BuildPrimitiveBounds rhsBounds;
				GetBuildPrimitiveBounds(m_buildPrimitiveOrder[rhsIndex], &rhsBounds);

				lhs = m_buildPrimitiveOrder[lhsIndex];
				BuildPrimitive* rhs = m_buildPrimitiveOrder[rhsIndex];
				if (lhsBounds.m_minX == rhsBounds.m_minX && lhs->m_partIndex != rhs->m_partIndex &&
					lhsBounds.m_minY < rhsBounds.m_maxY && rhsBounds.m_minY < lhsBounds.m_maxY &&
					lhsBounds.m_minZ < rhsBounds.m_maxZ && rhsBounds.m_minZ < lhsBounds.m_maxZ) {
					CLIP_BUILD_PRIMITIVE_MIN(m_z, m_minZ, m_maxZ, m_y, SET_SPLIT_VERTEX_POSITION_Z);
					CLIP_BUILD_PRIMITIVE_MAX(m_z, m_minZ, m_maxZ, m_y, SET_SPLIT_VERTEX_POSITION_Z);
					CLIP_BUILD_PRIMITIVE_MIN(m_y, m_minY, m_maxY, m_z, SET_SPLIT_VERTEX_POSITION_Y);
					CLIP_BUILD_PRIMITIVE_MAX(m_y, m_minY, m_maxY, m_z, SET_SPLIT_VERTEX_POSITION_Y);
					REMOVE_MATCHED_BUILD_PRIMITIVES();
				}
			}
		}
	}

#undef REMOVE_MATCHED_BUILD_PRIMITIVES
#undef CLIP_BUILD_PRIMITIVE_MAX
#undef CLIP_BUILD_PRIMITIVE_MIN
#undef SET_SPLIT_VERTEX_POSITION_Z
#undef SET_SPLIT_VERTEX_POSITION_Y
#undef SET_SPLIT_VERTEX_POSITION_X
#undef ALLOCATE_OUTSIDE_BUILD_PRIMITIVE_MAX
#undef ALLOCATE_OUTSIDE_BUILD_PRIMITIVE_MIN
#undef ROTATE_BUILD_PRIMITIVE_TO_MAX
#undef ROTATE_BUILD_PRIMITIVE_TO_MIN
#undef SWAP_ACTIVE_BUILD_PRIMITIVES
}

// FUNCTION: LEGORACERS 0x004972a0
void CarBuildModel::InterpolateBuildVertex(
	BuildVertex* p_dest,
	BuildVertex* p_left,
	BuildVertex* p_right,
	LegoFloat p_leftScale,
	LegoFloat p_rightScale
)
{
	LegoFloat leftX = p_left->m_textureCoordinate.m_x;
	LegoFloat rightX = p_right->m_textureCoordinate.m_x;
	p_dest->m_textureCoordinate.m_x = leftX * p_leftScale + rightX * p_rightScale;

	LegoFloat leftY = p_left->m_textureCoordinate.m_y;
	LegoFloat rightY = p_right->m_textureCoordinate.m_y;
	p_dest->m_textureCoordinate.m_y = leftY * p_leftScale + rightY * p_rightScale;

	p_dest->m_normalX = static_cast<LegoS8>(p_left->m_normalX * p_leftScale + p_right->m_normalX * p_rightScale);
	p_dest->m_normalY = static_cast<LegoS8>(p_left->m_normalY * p_leftScale + p_right->m_normalY * p_rightScale);
	p_dest->m_normalZ = static_cast<LegoS8>(p_left->m_normalZ * p_leftScale + p_right->m_normalZ * p_rightScale);
}

#ifdef COMPAT_MODE

LegoS32 CarBuildModel::BuildBatches(LegoS8 p_buildFlags)
{
	::memset(g_carBuildModelVertexUses, 0, sizeof(g_carBuildModelVertexUses));

	LegoS32 primitiveIndex = 0;
	LegoS32 previousMaterialIndex = -1;
	LegoS32 accumulatedReferenceCount = 0;
	m_batchVertexCount = 0;

	LegoS32 vertexCount = 2;
	LegoS32 materialRunEnd = primitiveIndex;
	LegoS32 currentMaterialIndex = previousMaterialIndex;
	LegoS32 lastPrimitiveIndex = m_buildPrimitiveCount - 1;

	if (lastPrimitiveIndex <= 0) {
		return lastPrimitiveIndex;
	}

	while (TRUE) {
		LegoS32 batchVertexCount = m_batchVertexCount;
		LegoS32 bestAddedVertexCount = 0x42;
		LegoS32 bestPrimitiveIndex = 0;

		if (batchVertexCount) {
			BuildPrimitive** primitiveOrder = m_buildPrimitiveOrder;
			if (previousMaterialIndex == primitiveOrder[primitiveIndex]->m_materialIndex) {
				LegoS32 searchEnd;
				if (p_buildFlags & 0x80) {
					searchEnd = materialRunEnd;
				}
				else {
					searchEnd = primitiveIndex + c_modelBatchVertexCapacity;
					if (searchEnd > materialRunEnd) {
						searchEnd = materialRunEnd;
					}
				}

				for (LegoS32 candidate = primitiveIndex; candidate < searchEnd; candidate++) {
					BuildPrimitive* primitive = primitiveOrder[candidate];
					LegoS32 addedVertexCount = primitive->m_vertexCount;
					vertexCount = addedVertexCount;

					for (LegoS32 i = 0; i < vertexCount; i++) {
						if (g_carBuildModelVertexUses[primitive->m_vertexIndices[i]].m_inBatch) {
							addedVertexCount--;
						}
					}

					if (addedVertexCount == 0) {
						bestPrimitiveIndex = candidate;
						bestAddedVertexCount = 0;
						candidate = m_buildPrimitiveCount;
					}
					else if (addedVertexCount < bestAddedVertexCount) {
						bestAddedVertexCount = addedVertexCount;
						bestPrimitiveIndex = candidate;
					}
				}
			}
		}

		if (previousMaterialIndex != m_buildPrimitiveOrder[primitiveIndex]->m_materialIndex) {
			if (batchVertexCount > 0) {
				for (LegoS32 i = 0; i < m_batchVertexCount; i++) {
					g_carBuildModelVertexUses[m_batchBuildVertexIndices[i]].m_inBatch = FALSE;
				}
			}

			currentMaterialIndex = m_buildPrimitiveOrder[primitiveIndex]->m_materialIndex;
			materialRunEnd = primitiveIndex;
			if (materialRunEnd < m_buildPrimitiveCount) {
				while (TRUE) {
					BuildPrimitive* primitive = m_buildPrimitiveOrder[materialRunEnd];
					if (currentMaterialIndex != primitive->m_materialIndex) {
						break;
					}

					vertexCount = primitive->m_vertexCount;
					for (LegoS32 i = 0; i < vertexCount; i++) {
						g_carBuildModelVertexUses[primitive->m_vertexIndices[i]].m_referenceCount++;
					}

					materialRunEnd++;
					if (materialRunEnd >= m_buildPrimitiveCount) {
						break;
					}
				}
			}
		}

		if (static_cast<LegoU32>(bestAddedVertexCount + m_batchVertexCount) > c_modelBatchVertexCapacity) {
			bestPrimitiveIndex = 0;
			LegoS32 bestReferenceCount = 0x7fffffff;
			for (LegoS32 candidate = primitiveIndex; candidate < materialRunEnd; candidate++) {
				BuildPrimitive* primitive = m_buildPrimitiveOrder[candidate];

				if (vertexCount > 0) {
					for (LegoS32 i = 0; i < vertexCount; i++) {
						accumulatedReferenceCount +=
							g_carBuildModelVertexUses[primitive->m_vertexIndices[i]].m_referenceCount;
					}
				}

				if (accumulatedReferenceCount < bestReferenceCount) {
					bestReferenceCount = accumulatedReferenceCount;
					bestPrimitiveIndex = candidate;
				}
			}

			if (m_batchVertexCount > 0) {
				for (LegoS32 i = 0; i < m_batchVertexCount; i++) {
					g_carBuildModelVertexUses[m_batchBuildVertexIndices[i]].m_inBatch = FALSE;
				}
			}
			m_batchVertexCount = 0;
		}

		if (bestPrimitiveIndex) {
			BuildPrimitive* primitive = m_buildPrimitiveOrder[primitiveIndex];
			m_buildPrimitiveOrder[primitiveIndex] = m_buildPrimitiveOrder[bestPrimitiveIndex];
			m_buildPrimitiveOrder[bestPrimitiveIndex] = primitive;
		}

		BuildPrimitive* primitive = m_buildPrimitiveOrder[primitiveIndex];
		for (LegoS32 i = 0; i < primitive->m_vertexCount; i++) {
			LegoS32 vertexIndex = primitive->m_vertexIndices[i];
			g_carBuildModelVertexUses[vertexIndex].m_referenceCount--;
			if (!g_carBuildModelVertexUses[vertexIndex].m_inBatch) {
				g_carBuildModelVertexUses[vertexIndex].m_inBatch = TRUE;
				m_batchBuildVertexIndices[m_batchVertexCount] = vertexIndex;
				m_batchVertexCount++;
			}
		}

		primitiveIndex++;
		if (primitiveIndex >= m_buildPrimitiveCount - 1) {
			return m_buildPrimitiveCount - 1;
		}
		previousMaterialIndex = currentMaterialIndex;
	}
}

#else

// FUNCTION: LEGORACERS 0x00497360
__declspec(naked) LegoS32 CarBuildModel::BuildBatches(LegoS8 p_buildFlags)
{
	__asm {
		sub esp, 0x20
		push ebx
		push ebp
		mov edx, ecx
		push edi
		mov ecx, 0x1d4c
		xor eax, eax
		mov edi, offset g_carBuildModelVertexUses
		xor ebx, ebx
		rep stosd
		mov eax, dword ptr [edx + 0x1ebc]
		or ebp, 0xffffffff
		dec eax
		mov dword ptr [esp + 0x24], ebx
		test eax, eax
		mov dword ptr [edx + 0x1ee0], ebx
		mov dword ptr [esp + 0xc], 2
		mov dword ptr [esp + 0x10], ebx
		mov dword ptr [esp + 0x20], ebp
		mov dword ptr [esp + 0x28], ebx
		jle L_BuildBatches_0
		push esi
		jmp L_BuildBatches_1
	L_BuildBatches_32:
		mov ebp, dword ptr [esp + 0x24]
	L_BuildBatches_1:
		mov ecx, dword ptr [edx + 0x1ee0]
		mov dword ptr [esp + 0x1c], 0x42
		test ecx, ecx
		mov dword ptr [esp + 0x18], 0
		je L_BuildBatches_2
		mov esi, dword ptr [edx + 0x1e38]
		xor edi, edi
		mov eax, dword ptr [esi + ebx*4]
		mov di, word ptr [eax + 2]
		cmp ebp, edi
		jne L_BuildBatches_2
		test byte ptr [esp + 0x34], 0x80
		je L_BuildBatches_3
		mov ebp, dword ptr [esp + 0x14]
		jmp L_BuildBatches_4
	L_BuildBatches_3:
		mov eax, dword ptr [esp + 0x14]
		lea ebp, [ebx + 0x40]
		cmp ebp, eax
		mov dword ptr [esp + 0x20], ebp
		jle L_BuildBatches_5
		mov ebp, eax
	L_BuildBatches_4:
		mov dword ptr [esp + 0x20], ebp
	L_BuildBatches_5:
		cmp ebx, ebp
		mov edi, ebx
		jge L_BuildBatches_6
	L_BuildBatches_12:
		mov eax, dword ptr [esi + edi*4]
		xor ecx, ecx
		mov cl, byte ptr [eax]
		test ecx, ecx
		mov dword ptr [esp + 0x10], ecx
		jle L_BuildBatches_7
		add eax, 8
		mov ebp, ecx
	L_BuildBatches_9:
		xor ebx, ebx
		mov bx, word ptr [eax]
		cmp byte ptr [ebx*4 + g_carBuildModelVertexUses + 2], 0
		je L_BuildBatches_8
		dec ecx
	L_BuildBatches_8:
		add eax, 2
		dec ebp
		jne L_BuildBatches_9
		mov ebx, dword ptr [esp + 0x2c]
		mov ebp, dword ptr [esp + 0x20]
	L_BuildBatches_7:
		test ecx, ecx
		jne L_BuildBatches_10
		mov dword ptr [esp + 0x18], edi
		mov edi, dword ptr [edx + 0x1ebc]
		mov dword ptr [esp + 0x1c], ecx
		jmp L_BuildBatches_11
	L_BuildBatches_10:
		cmp ecx, dword ptr [esp + 0x1c]
		jge L_BuildBatches_11
		mov dword ptr [esp + 0x1c], ecx
		mov dword ptr [esp + 0x18], edi
	L_BuildBatches_11:
		inc edi
		cmp edi, ebp
		jl L_BuildBatches_12
		jmp L_BuildBatches_6
	L_BuildBatches_2:
		mov eax, dword ptr [edx + 0x1e38]
		xor esi, esi
		mov eax, dword ptr [eax + ebx*4]
		mov si, word ptr [eax + 2]
		cmp ebp, esi
		je L_BuildBatches_6
		xor eax, eax
		test ecx, ecx
		jle L_BuildBatches_13
		lea ecx, [edx + 0x1e3c]
	L_BuildBatches_14:
		xor esi, esi
		inc eax
		mov si, word ptr [ecx]
		add ecx, 2
		mov byte ptr [esi*4 + g_carBuildModelVertexUses + 2], 0
		mov esi, dword ptr [edx + 0x1ee0]
		cmp eax, esi
		jl L_BuildBatches_14
	L_BuildBatches_13:
		mov ecx, dword ptr [edx + 0x1e38]
		mov esi, ebx
		mov eax, dword ptr [ecx + ebx*4]
		xor ecx, ecx
		mov cx, word ptr [eax + 2]
		mov eax, dword ptr [edx + 0x1ebc]
		cmp ebx, eax
		mov dword ptr [esp + 0x24], ecx
		jge L_BuildBatches_15
		jmp L_BuildBatches_16
	L_BuildBatches_19:
		mov ecx, dword ptr [esp + 0x24]
	L_BuildBatches_16:
		mov eax, dword ptr [edx + 0x1e38]
		xor edi, edi
		mov eax, dword ptr [eax + esi*4]
		mov di, word ptr [eax + 2]
		cmp ecx, edi
		jne L_BuildBatches_15
		xor ecx, ecx
		mov cl, byte ptr [eax]
		mov edi, ecx
		test edi, edi
		mov dword ptr [esp + 0x10], edi
		jle L_BuildBatches_17
		mov ecx, 8
	L_BuildBatches_18:
		mov eax, dword ptr [edx + 0x1e38]
		add ecx, 2
		mov ebp, dword ptr [eax + esi*4]
		xor eax, eax
		mov ax, word ptr [ecx + ebp - 2]
		inc word ptr [eax*4 + g_carBuildModelVertexUses]
		dec edi
		jne L_BuildBatches_18
	L_BuildBatches_17:
		mov eax, dword ptr [edx + 0x1ebc]
		inc esi
		cmp esi, eax
		jl L_BuildBatches_19
	L_BuildBatches_15:
		mov dword ptr [esp + 0x14], esi
	L_BuildBatches_6:
		mov eax, dword ptr [edx + 0x1ee0]
		mov ecx, dword ptr [esp + 0x1c]
		add eax, ecx
		cmp eax, 0x40
		jbe L_BuildBatches_20
		mov eax, dword ptr [esp + 0x14]
		mov dword ptr [esp + 0x18], 0
		cmp ebx, eax
		mov ebp, 0x7fffffff
		mov edi, ebx
		jge L_BuildBatches_21
	L_BuildBatches_25:
		mov ecx, dword ptr [esp + 0x10]
		test ecx, ecx
		jle L_BuildBatches_22
		mov eax, dword ptr [edx + 0x1e38]
		mov eax, dword ptr [eax + edi*4]
		add eax, 8
	L_BuildBatches_23:
		xor esi, esi
		xor ebx, ebx
		mov si, word ptr [eax]
		add eax, 2
		mov bx, word ptr [esi*4 + g_carBuildModelVertexUses]
		mov esi, dword ptr [esp + 0x28]
		add esi, ebx
		dec ecx
		mov dword ptr [esp + 0x28], esi
		jne L_BuildBatches_23
		mov ebx, dword ptr [esp + 0x2c]
	L_BuildBatches_22:
		mov eax, dword ptr [esp + 0x28]
		cmp ebp, eax
		jle L_BuildBatches_24
		mov ebp, eax
		mov dword ptr [esp + 0x18], edi
	L_BuildBatches_24:
		mov eax, dword ptr [esp + 0x14]
		inc edi
		cmp edi, eax
		jl L_BuildBatches_25
	L_BuildBatches_21:
		mov ecx, dword ptr [edx + 0x1ee0]
		xor eax, eax
		test ecx, ecx
		jle L_BuildBatches_26
		lea ecx, [edx + 0x1e3c]
	L_BuildBatches_27:
		xor esi, esi
		inc eax
		mov si, word ptr [ecx]
		add ecx, 2
		mov byte ptr [esi*4 + g_carBuildModelVertexUses + 2], 0
		mov esi, dword ptr [edx + 0x1ee0]
		cmp eax, esi
		jl L_BuildBatches_27
	L_BuildBatches_26:
		mov dword ptr [edx + 0x1ee0], 0
	L_BuildBatches_20:
		mov esi, dword ptr [esp + 0x18]
		test esi, esi
		je L_BuildBatches_28
		mov eax, dword ptr [edx + 0x1e38]
		mov edi, dword ptr [eax + esi*4]
		mov ecx, dword ptr [eax + ebx*4]
		mov dword ptr [eax + ebx*4], edi
		mov eax, dword ptr [edx + 0x1e38]
		mov dword ptr [eax + esi*4], ecx
	L_BuildBatches_28:
		mov ecx, dword ptr [edx + 0x1e38]
		xor ebp, ebp
		lea eax, [ecx + ebx*4]
		mov ecx, dword ptr [ecx + ebx*4]
		cmp byte ptr [ecx], 0
		jbe L_BuildBatches_29
		mov edi, 8
	L_BuildBatches_31:
		mov ecx, dword ptr [eax]
		xor eax, eax
		mov ax, word ptr [edi + ecx]
		dec word ptr [eax*4 + g_carBuildModelVertexUses]
		mov cl, byte ptr [eax*4 + g_carBuildModelVertexUses + 2]
		test cl, cl
		jne L_BuildBatches_30
		mov byte ptr [eax*4 + g_carBuildModelVertexUses + 2], 1
		mov ecx, dword ptr [edx + 0x1ee0]
		mov word ptr [edx + ecx*2 + 0x1e3c], ax
		mov eax, dword ptr [edx + 0x1ee0]
		inc eax
		mov dword ptr [edx + 0x1ee0], eax
	L_BuildBatches_30:
		mov eax, dword ptr [edx + 0x1e38]
		xor ecx, ecx
		inc ebp
		add edi, 2
		mov esi, dword ptr [eax + ebx*4]
		lea eax, [eax + ebx*4]
		mov cl, byte ptr [esi]
		cmp ebp, ecx
		jl L_BuildBatches_31
	L_BuildBatches_29:
		mov eax, dword ptr [edx + 0x1ebc]
		inc ebx
		dec eax
		mov dword ptr [esp + 0x2c], ebx
		cmp ebx, eax
		jl L_BuildBatches_32
		pop esi
	L_BuildBatches_0:
		pop edi
		pop ebp
		pop ebx
		add esp, 0x20
		ret 4
	}
}

#endif

// FUNCTION: LEGORACERS 0x00497690
void CarBuildModel::FinalizeBuild(LegoU8 p_buildFlags)
{
	LegoS32 i;

	for (i = m_buildPrimitiveCount; i < c_buildPrimitiveCapacity; i++) {
		m_buildPrimitiveOrder[i] = &m_buildPrimitives[i];
	}

	g_carBuildModelSortVertices = m_buildVertices;

	if (p_buildFlags & c_finalizeResolveIntersectionsFlag) {
		ResolvePrimitiveIntersections();
		if (m_buildStatus & c_buildStatusOverflow) {
			return;
		}
	}

	if (p_buildFlags & c_finalizeCullSingleMaterialFlag) {
		for (i = 0; i < m_buildPrimitiveCount; i++) {
			if (m_buildPrimitiveOrder[i]->m_commandFlags == c_buildPrimitiveCommandMaterial1) {
				m_buildPrimitiveCount--;
				BuildPrimitive* primitive = m_buildPrimitiveOrder[i--];
				m_buildPrimitiveOrder[i + 1] = m_buildPrimitiveOrder[m_buildPrimitiveCount];
				m_buildPrimitiveOrder[m_buildPrimitiveCount] = primitive;
			}
		}
	}

	LegoS32 splitPrimitiveCount = m_buildPrimitiveCount;
	for (i = 0; i < splitPrimitiveCount; i++) {
		if (m_buildPrimitiveOrder[i]->m_vertexCount == 4) {
			m_buildPrimitiveOrder[i]->m_vertexCount = 3;

			*m_buildPrimitiveOrder[m_buildPrimitiveCount] = *m_buildPrimitiveOrder[i];
			m_buildPrimitiveOrder[m_buildPrimitiveCount]->m_vertexIndices[0] =
				m_buildPrimitiveOrder[i]->m_vertexIndices[2];
			m_buildPrimitiveOrder[m_buildPrimitiveCount]->m_vertexIndices[2] =
				m_buildPrimitiveOrder[i]->m_vertexIndices[3];
			m_buildPrimitiveCount++;

			if (m_buildPrimitiveCount >= c_buildPrimitiveCapacity) {
				m_buildStatus |= c_buildStatusOverflow;
				return;
			}
		}
	}

	::qsort(m_buildPrimitiveOrder, m_buildPrimitiveCount, sizeof(BuildPrimitive*), ComparePrimitiveDrawOrder);

	if (p_buildFlags & c_finalizePostSortFlag) {
		BuildBatches(p_buildFlags);
	}
}

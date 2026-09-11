#pragma once
#include "ModernRendererTypes.h"
#include <cmath>
#include <limits>
#include <map>
#include <new>
#include <tuple>
#include <vector>

// CPU-only conversion, also compiled by the portable regression tests.
// The Mesh template is instantiated with the actual Mesh_t by BMD::BuildModernMesh.
enum class ModernBMDGeometryError
{
    None, InvalidCounts, MissingData, UnsupportedPolygon,
    InvalidVertexIndex, InvalidNormalIndex, InvalidUVIndex,
    InvalidPositionBone, InvalidNormalBone, NonFiniteData, AllocationFailed
};

struct ModernBMDGeometry
{
    std::vector<ModernBMDVertex> Vertices;
    std::vector<std::uint32_t> Indices;
};

template<class Mesh>
bool BuildModernBMDGeometry(const Mesh& mesh, int boneCount, ModernBMDGeometry& output,
                           ModernBMDGeometryError& error)
{
    error = ModernBMDGeometryError::None;
    // These are the bounds of the initial Main BMD transport, not asset guesses.
    if (boneCount <= 0 || boneCount > 200 ||
        mesh.NumVertices <= 0 || mesh.NumVertices > 15000 ||
        mesh.NumNormals <= 0 || mesh.NumNormals > 15000 ||
        mesh.NumTexCoords <= 0 || mesh.NumTexCoords > 32767 ||
        mesh.NumTriangles <= 0 || mesh.NumTriangles > 32767)
    {
        error = ModernBMDGeometryError::InvalidCounts;
        return false;
    }
    if (!mesh.Vertices || !mesh.Normals || !mesh.TexCoords || !mesh.Triangles)
    {
        error = ModernBMDGeometryError::MissingData;
        return false;
    }

    try
    {
        ModernBMDGeometry converted;
        const std::size_t cornerCount = static_cast<std::size_t>(mesh.NumTriangles) * 3;
        converted.Vertices.reserve(cornerCount);
        converted.Indices.reserve(cornerCount);
        // Deduplicate a full corner, not position alone: retain hard normals and UV seams.
        std::map<std::tuple<int, int, int>, std::uint32_t> corners;
        for (int triangleIndex = 0; triangleIndex < mesh.NumTriangles; ++triangleIndex)
        {
            const auto& triangle = mesh.Triangles[triangleIndex];
            if (triangle.Polygon != 3)
            {
                error = ModernBMDGeometryError::UnsupportedPolygon;
                return false; // No implicit triangulation that could change legacy winding.
            }
            for (int corner = 0; corner < 3; ++corner)
            {
                const int vertexIndex = triangle.VertexIndex[corner];
                const int normalIndex = triangle.NormalIndex[corner];
                const int uvIndex = triangle.TexCoordIndex[corner];
                if (vertexIndex < 0 || vertexIndex >= mesh.NumVertices)
                {
                    error = ModernBMDGeometryError::InvalidVertexIndex; return false;
                }
                if (normalIndex < 0 || normalIndex >= mesh.NumNormals)
                {
                    error = ModernBMDGeometryError::InvalidNormalIndex; return false;
                }
                if (uvIndex < 0 || uvIndex >= mesh.NumTexCoords)
                {
                    error = ModernBMDGeometryError::InvalidUVIndex; return false;
                }
                const auto key = std::make_tuple(vertexIndex, normalIndex, uvIndex);
                const auto existing = corners.find(key);
                if (existing != corners.end())
                {
                    converted.Indices.push_back(existing->second);
                    continue;
                }
                const auto& position = mesh.Vertices[vertexIndex];
                const auto& normal = mesh.Normals[normalIndex];
                const auto& uv = mesh.TexCoords[uvIndex];
                if (position.Node < 0 || position.Node >= boneCount)
                {
                    error = ModernBMDGeometryError::InvalidPositionBone; return false;
                }
                if (normal.Node < 0 || normal.Node >= boneCount)
                {
                    error = ModernBMDGeometryError::InvalidNormalBone; return false;
                }
                ModernBMDVertex vertex{};
                for (int component = 0; component < 3; ++component)
                {
                    if (!std::isfinite(position.Position[component]) ||
                        !std::isfinite(normal.Normal[component]))
                    {
                        error = ModernBMDGeometryError::NonFiniteData; return false;
                    }
                    vertex.Position[component] = position.Position[component];
                    vertex.Normal[component] = normal.Normal[component];
                }
                if (!std::isfinite(uv.TexCoordU) || !std::isfinite(uv.TexCoordV))
                {
                    error = ModernBMDGeometryError::NonFiniteData; return false;
                }
                // Preserve Main UVs here. A future material/import policy may flip V once.
                vertex.UV[0] = uv.TexCoordU;
                vertex.UV[1] = uv.TexCoordV;
                vertex.PositionBone = static_cast<std::uint16_t>(position.Node);
                vertex.NormalBone = static_cast<std::uint16_t>(normal.Node);
                vertex.OriginalVertexId = static_cast<std::uint32_t>(vertexIndex);
                const auto index = static_cast<std::uint32_t>(converted.Vertices.size());
                converted.Vertices.push_back(vertex);
                corners.emplace(key, index);
                converted.Indices.push_back(index);
            }
        }
        // Transactional: a rejected asset never leaves half-converted output.
        output.Vertices.swap(converted.Vertices);
        output.Indices.swap(converted.Indices);
        return true;
    }
    catch (const std::bad_alloc&)
    {
        error = ModernBMDGeometryError::AllocationFailed;
        return false;
    }
}

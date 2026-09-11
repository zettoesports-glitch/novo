#include "../source/ModernBMDGeometry.h"
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
struct VertexInput { short Node; float Position[3]; };
struct NormalInput { short Node; float Normal[3]; };
struct UVInput { float TexCoordU, TexCoordV; };
struct TriangleInput { char Polygon; short VertexIndex[4], NormalIndex[4], TexCoordIndex[4]; };
struct MeshInput
{
    short NumVertices, NumNormals, NumTexCoords, NumTriangles;
    VertexInput* Vertices;
    NormalInput* Normals;
    UVInput* TexCoords;
    TriangleInput* Triangles;
};
void Check(bool value, const char* message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
}

int main()
{
    VertexInput positions[] = {{0,{0,0,0}}, {1,{1,0,0}}, {0,{0,1,0}}};
    NormalInput normals[] = {{1,{0,0,1}}, {0,{0,1,0}}};
    UVInput uv[] = {{0,0.25f}, {1,0}, {0,1}, {0.5f,0.75f}};
    TriangleInput triangles[] = {
        {3,{0,1,2,0},{0,0,0,0},{0,1,2,0}},
        {3,{0,2,1,0},{0,0,0,0},{0,2,1,0}}
    };
    MeshInput mesh{3,2,4,2,positions,normals,uv,triangles};
    ModernBMDGeometry result;
    ModernBMDGeometryError error;
    Check(BuildModernBMDGeometry(mesh,2,result,error), "valid mesh rejected");
    Check(result.Vertices.size()==3 && result.Indices.size()==6, "shared corners not deduplicated");
    Check(result.Indices==std::vector<std::uint32_t>({0,1,2,0,2,1}), "triangle order/winding changed");
    Check(result.Vertices[0].PositionBone==0 && result.Vertices[0].NormalBone==1,
          "separate normal bone lost");
    Check(result.Vertices[0].UV[1]==0.25f, "unexpected V flip");
    Check(result.Vertices[2].OriginalVertexId==2, "original wave vertex id lost");

    triangles[1].TexCoordIndex[0]=3;
    triangles[1].NormalIndex[1]=1;
    Check(BuildModernBMDGeometry(mesh,2,result,error), "seam mesh rejected");
    Check(result.Vertices.size()==5, "UV seam or hard-normal split lost");
    Check(result.Indices==std::vector<std::uint32_t>({0,1,2,3,4,1}), "seam corner map wrong");
    Check(result.Vertices[3].OriginalVertexId==0 && result.Vertices[4].OriginalVertexId==2,
          "seam duplication changed original vertex id");

    const auto reject = [&](ModernBMDGeometryError expected, const char* label, int bones=2)
    {
        const auto vertices = result.Vertices.size();
        const auto indices = result.Indices;
        Check(!BuildModernBMDGeometry(mesh,bones,result,error), label);
        Check(error==expected, "incorrect rejection diagnostic");
        Check(result.Vertices.size()==vertices && result.Indices==indices,
              "failed conversion replaced previous output");
    };
    triangles[1].VertexIndex[1]=-1;
    reject(ModernBMDGeometryError::InvalidVertexIndex, "negative index accepted");
    triangles[1].VertexIndex[1]=3;
    reject(ModernBMDGeometryError::InvalidVertexIndex, "out-of-range index accepted");
    triangles[1].VertexIndex[1]=2;
    triangles[0].NormalIndex[0]=2;
    reject(ModernBMDGeometryError::InvalidNormalIndex, "invalid normal index accepted");
    triangles[0].NormalIndex[0]=0;
    triangles[0].TexCoordIndex[0]=4;
    reject(ModernBMDGeometryError::InvalidUVIndex, "invalid UV index accepted");
    triangles[0].TexCoordIndex[0]=0;
    positions[0].Node=-1;
    reject(ModernBMDGeometryError::InvalidPositionBone, "negative bone wrapped");
    positions[0].Node=0;
    normals[0].Node=2;
    reject(ModernBMDGeometryError::InvalidNormalBone, "out-of-range normal bone accepted");
    normals[0].Node=1;
    triangles[1].Polygon=4;
    reject(ModernBMDGeometryError::UnsupportedPolygon, "quad silently triangulated");
    triangles[1].Polygon=3;
    positions[0].Position[1]=(std::numeric_limits<float>::quiet_NaN)();
    reject(ModernBMDGeometryError::NonFiniteData, "NaN position accepted");
    positions[0].Position[1]=0;
    uv[0].TexCoordV=(std::numeric_limits<float>::infinity)();
    reject(ModernBMDGeometryError::NonFiniteData, "infinite UV accepted");
    uv[0].TexCoordV=0.25f;
    mesh.Triangles=nullptr;
    reject(ModernBMDGeometryError::MissingData, "null triangle storage accepted");
    mesh.Triangles=triangles;
    mesh.NumTriangles=0;
    reject(ModernBMDGeometryError::InvalidCounts, "empty mesh accepted");
    mesh.NumTriangles=2;
    reject(ModernBMDGeometryError::InvalidCounts, "bone count > MAX_BONES accepted",201);

    // Capacity is per expanded corner, not per original vertex. Exercise >16-bit
    // output indices while every original index still fits Main's signed short.
    std::vector<VertexInput> manyPositions(15000, VertexInput{0,{1,2,3}});
    std::vector<UVInput> manyUV(32767, UVInput{0.25f,0.75f});
    std::vector<TriangleInput> manyTriangles(24000);
    for (std::size_t i=0;i<manyTriangles.size();++i)
    {
        auto& t=manyTriangles[i]; t.Polygon=3;
        for (int k=0;k<3;++k)
        {
            const auto corner=i*3+static_cast<std::size_t>(k);
            t.VertexIndex[k]=static_cast<short>(corner%15000);
            t.NormalIndex[k]=0;
            t.TexCoordIndex[k]=static_cast<short>(corner/15000);
        }
    }
    MeshInput large{15000,2,32767,24000,manyPositions.data(),normals,manyUV.data(),manyTriangles.data()};
    Check(BuildModernBMDGeometry(large,2,result,error), "large valid mesh rejected");
    Check(result.Vertices.size()==72000 && result.Indices.back()==71999,
          "expanded mesh indices truncated to 16 bits");
    std::cout << "BMD geometry: seams, bones, winding, transactional errors and 32-bit indices PASS\n";
}

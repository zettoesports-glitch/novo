#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

// Main's final per-bone transform is a 3x4 affine matrix. The modern transport
// keeps the same semantics but packs each bone into two float4 texels, matching
// the reference renderer shape: quaternion rotation + position/uniform scale.
// BodyScale and BodyOrigin remain per-instance data and are intentionally not
// baked into this asset-independent pose snapshot.
struct ModernSkeletonBoneTexels
{
    float Rotation[4];      // quaternion xyzw
    float PositionScale[4]; // xyz translation, w = legacy BoneScale
};

static_assert(sizeof(ModernSkeletonBoneTexels) == sizeof(float) * 8,
              "Modern skeleton bone transport must remain two float4 texels");

struct ModernSkeletonPose
{
    std::vector<ModernSkeletonBoneTexels> Bones;
};

enum class ModernSkeletonPoseError
{
    None,
    InvalidBoneCount,
    MissingMatrices,
    NonFiniteData,
    DegenerateRotation,
    AllocationFailed,
};

namespace ModernSkeletonPoseDetail
{
inline bool IsFinite3x4(const float matrix[3][4])
{
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 4; ++column)
            if (!std::isfinite(matrix[row][column]))
                return false;
    return true;
}

inline bool MatrixRotationToQuaternion(const float matrix[3][4], float quaternion[4])
{
    const float m00 = matrix[0][0];
    const float m01 = matrix[0][1];
    const float m02 = matrix[0][2];
    const float m10 = matrix[1][0];
    const float m11 = matrix[1][1];
    const float m12 = matrix[1][2];
    const float m20 = matrix[2][0];
    const float m21 = matrix[2][1];
    const float m22 = matrix[2][2];

    const float trace = m00 + m11 + m22;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    if (trace > 0.0f)
    {
        const float root = std::sqrt(trace + 1.0f);
        if (!(root > 0.0f) || !std::isfinite(root))
            return false;
        const float inv = 0.5f / root;
        w = 0.5f * root;
        x = (m21 - m12) * inv;
        y = (m02 - m20) * inv;
        z = (m10 - m01) * inv;
    }
    else if (m00 >= m11 && m00 >= m22)
    {
        const float root = std::sqrt((1.0f + m00) - m11 - m22);
        if (!(root > 0.0f) || !std::isfinite(root))
            return false;
        const float inv = 0.5f / root;
        x = 0.5f * root;
        y = (m01 + m10) * inv;
        z = (m02 + m20) * inv;
        w = (m21 - m12) * inv;
    }
    else if (m11 >= m22)
    {
        const float root = std::sqrt((1.0f + m11) - m00 - m22);
        if (!(root > 0.0f) || !std::isfinite(root))
            return false;
        const float inv = 0.5f / root;
        x = (m01 + m10) * inv;
        y = 0.5f * root;
        z = (m12 + m21) * inv;
        w = (m02 - m20) * inv;
    }
    else
    {
        const float root = std::sqrt((1.0f + m22) - m00 - m11);
        if (!(root > 0.0f) || !std::isfinite(root))
            return false;
        const float inv = 0.5f / root;
        x = (m02 + m20) * inv;
        y = (m12 + m21) * inv;
        z = 0.5f * root;
        w = (m10 - m01) * inv;
    }

    const float lengthSquared = x * x + y * y + z * z + w * w;
    if (!(lengthSquared > 1.0e-12f) || !std::isfinite(lengthSquared))
        return false;

    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    x *= inverseLength;
    y *= inverseLength;
    z *= inverseLength;
    w *= inverseLength;

    // q and -q encode the same rotation. Canonicalize the sign so snapshots
    // are deterministic and easier to compare/log/cache.
    if (w < 0.0f)
    {
        x = -x;
        y = -y;
        z = -z;
        w = -w;
    }

    quaternion[0] = x;
    quaternion[1] = y;
    quaternion[2] = z;
    quaternion[3] = w;
    return true;
}
} // namespace ModernSkeletonPoseDetail

inline bool BuildModernSkeletonPose(const float (*boneMatrices)[3][4],
                                    int boneCount,
                                    float boneScale,
                                    ModernSkeletonPose& output,
                                    ModernSkeletonPoseError& error)
{
    error = ModernSkeletonPoseError::None;
    if (boneCount <= 0 || boneCount > 200)
    {
        error = ModernSkeletonPoseError::InvalidBoneCount;
        return false;
    }
    if (boneMatrices == nullptr)
    {
        error = ModernSkeletonPoseError::MissingMatrices;
        return false;
    }
    if (!std::isfinite(boneScale))
    {
        error = ModernSkeletonPoseError::NonFiniteData;
        return false;
    }

    try
    {
        ModernSkeletonPose converted;
        converted.Bones.resize(static_cast<std::size_t>(boneCount));

        for (int bone = 0; bone < boneCount; ++bone)
        {
            const float (*matrix)[4] = boneMatrices[bone];
            if (!ModernSkeletonPoseDetail::IsFinite3x4(matrix))
            {
                error = ModernSkeletonPoseError::NonFiniteData;
                return false;
            }

            ModernSkeletonBoneTexels& packed = converted.Bones[static_cast<std::size_t>(bone)];
            if (!ModernSkeletonPoseDetail::MatrixRotationToQuaternion(matrix, packed.Rotation))
            {
                error = ModernSkeletonPoseError::DegenerateRotation;
                return false;
            }

            packed.PositionScale[0] = matrix[0][3];
            packed.PositionScale[1] = matrix[1][3];
            packed.PositionScale[2] = matrix[2][3];
            packed.PositionScale[3] = boneScale;
        }

        // Transactional: failed conversion never replaces a previously valid pose.
        output.Bones.swap(converted.Bones);
        return true;
    }
    catch (const std::bad_alloc&)
    {
        error = ModernSkeletonPoseError::AllocationFailed;
        return false;
    }
}

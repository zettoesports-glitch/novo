#include "../source/ModernSkeletonPose.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
void Check(bool value, const char* message)
{
    if (!value)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

bool Near(float a, float b, float epsilon = 1.0e-5f)
{
    return std::fabs(a - b) <= epsilon;
}

void RotateByQuaternion(const float q[4], const float v[3], float out[3])
{
    const float ux = q[0], uy = q[1], uz = q[2], s = q[3];
    const float dotUV = ux * v[0] + uy * v[1] + uz * v[2];
    const float dotUU = ux * ux + uy * uy + uz * uz;
    const float crossX = uy * v[2] - uz * v[1];
    const float crossY = uz * v[0] - ux * v[2];
    const float crossZ = ux * v[1] - uy * v[0];
    out[0] = 2.0f * dotUV * ux + (s * s - dotUU) * v[0] + 2.0f * s * crossX;
    out[1] = 2.0f * dotUV * uy + (s * s - dotUU) * v[1] + 2.0f * s * crossY;
    out[2] = 2.0f * dotUV * uz + (s * s - dotUU) * v[2] + 2.0f * s * crossZ;
}
}

int main()
{
    constexpr ModernSkeletonPoseSpace SeparateBody = ModernSkeletonPoseSpace::BodyTransformSeparate;

    float bones[2][3][4] = {
        {{1,0,0,10},{0,1,0,20},{0,0,1,30}},
        {{0,-1,0,4},{1,0,0,5},{0,0,1,6}}
    };

    ModernSkeletonPose pose;
    ModernSkeletonPoseError error;
    Check(BuildModernSkeletonPose(bones, 2, 1.5f, SeparateBody, pose, error), "valid pose rejected");
    Check(pose.Space == SeparateBody, "pose space changed");
    Check(pose.Bones.size() == 2, "bone count changed");
    Check(Near(pose.Bones[0].Rotation[0], 0.0f) &&
          Near(pose.Bones[0].Rotation[1], 0.0f) &&
          Near(pose.Bones[0].Rotation[2], 0.0f) &&
          Near(pose.Bones[0].Rotation[3], 1.0f), "identity rotation packed incorrectly");
    Check(Near(pose.Bones[0].PositionScale[0], 10.0f) &&
          Near(pose.Bones[0].PositionScale[1], 20.0f) &&
          Near(pose.Bones[0].PositionScale[2], 30.0f) &&
          Near(pose.Bones[0].PositionScale[3], 1.5f), "translation/BoneScale lost");

    const float xAxis[3] = {1,0,0};
    float rotated[3] = {};
    RotateByQuaternion(pose.Bones[1].Rotation, xAxis, rotated);
    Check(Near(rotated[0], 0.0f) && Near(rotated[1], 1.0f) && Near(rotated[2], 0.0f),
          "90-degree Z rotation changed convention");

    // Prove two snapshots do not alias or share mutable entity state.
    ModernSkeletonPose secondPose;
    bones[0][0][3] = -7.0f;
    Check(BuildModernSkeletonPose(bones, 2, 0.75f, SeparateBody, secondPose, error), "second pose rejected");
    Check(Near(pose.Bones[0].PositionScale[0], 10.0f), "first pose mutated by second instance");
    Check(Near(secondPose.Bones[0].PositionScale[0], -7.0f) &&
          Near(secondPose.Bones[0].PositionScale[3], 0.75f), "second pose state incorrect");

    const auto previous = secondPose.Bones;
    bones[0][1][1] = (std::numeric_limits<float>::quiet_NaN)();
    Check(!BuildModernSkeletonPose(bones, 2, 1.0f, SeparateBody, secondPose, error), "NaN matrix accepted");
    Check(error == ModernSkeletonPoseError::NonFiniteData, "wrong NaN diagnostic");
    Check(secondPose.Bones.size() == previous.size() &&
          Near(secondPose.Bones[0].PositionScale[0], previous[0].PositionScale[0]),
          "failed conversion replaced prior pose");
    bones[0][1][1] = 1.0f;

    Check(!BuildModernSkeletonPose(nullptr, 2, 1.0f, SeparateBody, secondPose, error) &&
          error == ModernSkeletonPoseError::MissingMatrices, "null matrices accepted");
    Check(!BuildModernSkeletonPose(bones, 0, 1.0f, SeparateBody, secondPose, error) &&
          error == ModernSkeletonPoseError::InvalidBoneCount, "zero bones accepted");
    Check(!BuildModernSkeletonPose(bones, 201, 1.0f, SeparateBody, secondPose, error) &&
          error == ModernSkeletonPoseError::InvalidBoneCount, "too many bones accepted");
    Check(!BuildModernSkeletonPose(bones, 2, (std::numeric_limits<float>::infinity)(), SeparateBody, secondPose, error) &&
          error == ModernSkeletonPoseError::NonFiniteData, "infinite BoneScale accepted");

    float degenerate[1][3][4] = {{{0,0,0,0},{0,0,0,0},{0,0,0,0}}};
    Check(!BuildModernSkeletonPose(degenerate, 1, 1.0f, SeparateBody, secondPose, error) &&
          error == ModernSkeletonPoseError::DegenerateRotation, "degenerate rotation accepted");

    // Calc_RenderObject can ask Animation() to bake BodyScale/BodyOrigin into
    // BoneMatrix. The first modern BMD shader applies those values per-instance,
    // so accepting a baked pose would transform it twice. Reject it explicitly
    // until a normalization/dedicated shader path exists.
    const auto preserved = secondPose.Bones;
    Check(!BuildModernSkeletonPose(bones, 2, 1.0f,
                                   ModernSkeletonPoseSpace::BodyTransformBaked,
                                   secondPose, error), "body-baked pose accepted");
    Check(error == ModernSkeletonPoseError::UnsupportedPoseSpace,
          "wrong body-baked pose diagnostic");
    Check(secondPose.Bones.size() == preserved.size(),
          "rejected pose changed previous snapshot");

    std::cout << "Skeleton pose: convention, body-space, isolation and rejection PASS\n";
}

#include "index.hpp"
#include "gl-gizmo.hpp"

#define KINDA_SMALL_NUMBER 0.001f

inline float square(const float value) { return value * value; }

// Find good arbitrary axis vectors to represent U and V axes of a plane, using this vector as the normal of the plane.
void find_best_axis_vectors(const float3 & vector, float3 & Axis1, float3 & Axis2)
{
    const float3 N = abs(vector);

    if (N.z > N.x && N.z > N.y) Axis1 = float3(1, 0, 0);
    else Axis1 = float3(0, 0, 1);

    Axis1 = safe_normalize(Axis1 - vector * dot(Axis1,vector));
    Axis2 = cross(Axis1, vector);
}

//SolveTwoBoneIK(false, 1.f, 1.f);

// FaceJointTarget

/**
* Two Bone IK
*
* This handles two bone chain link excluding root bone. This will solve the solution for joint/end when given root, joint, end position (root->joint->end in the hierarchy)
* based on effector, joint target location
* This only solves for location, if you want to rotate them to face target, this doesn't do it for you
*
* @param	RootPos				Current Root position
* @param	JointPos			Current Joint position
* @param	EndPos				Current End position
* @param	JointTarget			Joint Target position (where joint is facing while creating plane between joint pos, joint target, root pos(rotate-plane ik))
* @param	Effector			Effector position (target position)
* @param	OutJointPos			(out) adjusted joint pos
* @param	OutEndPos			(out) adjusted end pos
* @param	bAllowStretching	whether or not to allow stretching or not
* @param	StartStretchRatio	When should it start stretch -i.e. 1 means its own length without any stretch
* @param	MaxStretchScale		How much it can stretch to in ratio
*/
void SolveTwoBoneIK(
    const float3 & RootPos,
    const float3 & JointPos,
    const float3 & EndPos,
    const float3 & JointTarget,
    const float3 & Effector,
    float3 & OutJointPos,
    float3 & OutEndPos,
    bool bAllowStretching,
    float StartStretchRatio,
    float MaxStretchScale)
{

    float LowerLimbLength = length(EndPos - JointPos);
    float UpperLimbLength = length(JointPos - RootPos);

    // This is our reach goal.
    float3 DesiredPos = Effector;
    float3 DesiredDelta = DesiredPos - RootPos;
    float DesiredLength = length(DesiredDelta);

    // Find lengths of upper and lower limb in the ref skeleton.
    // Use actual sizes instead of ref skeleton, so we take into account translation and scaling from other bone controllers.
    float MaxLimbLength = LowerLimbLength + UpperLimbLength;

    // Check to handle case where DesiredPos is the same as RootPos.
    float3	DesiredDir;
    if (DesiredLength < (float) KINDA_SMALL_NUMBER)
    {
        DesiredLength = (float)KINDA_SMALL_NUMBER;
        DesiredDir = float3(1, 0, 0);
    }
    else
    {
        DesiredDir = safe_normalize(DesiredDelta);
    }

    // Get joint target (used for defining plane that joint should be in).
    float3 JointTargetDelta = JointTarget - RootPos;
    const float JointTargetLengthSqr = length2(JointTargetDelta);

    // Same check as above, to cover case when JointTarget position is the same as RootPos.
    float3 JointPlaneNormal, JointBendDir;
    if (JointTargetLengthSqr < square((float)KINDA_SMALL_NUMBER))
    {
        JointBendDir = float3(0, 1, 0);
        JointPlaneNormal = float3(0, 0, 1);
    }
    else
    {
        JointPlaneNormal = cross(DesiredDir, JointTargetDelta);

        // If we are trying to point the limb in the same direction that we are supposed to displace the joint in, 
        // we have to just pick 2 random vector perp to DesiredDir and each other.
        if (length2(JointPlaneNormal) < square((float)KINDA_SMALL_NUMBER))
        {
            find_best_axis_vectors(DesiredDir, JointPlaneNormal, JointBendDir);
        }
        else
        {
            JointPlaneNormal = normalize(JointPlaneNormal);

            // Find the final member of the reference frame by removing any component of JointTargetDelta along DesiredDir.
            // This should never leave a zero vector, because we've checked DesiredDir and JointTargetDelta are not parallel.
            JointBendDir = normalize(JointTargetDelta - (dot(JointTargetDelta, DesiredDir) * DesiredDir));
        }
    }

    if (bAllowStretching)
    {
        const float ScaleRange = MaxStretchScale - StartStretchRatio;
        if (ScaleRange > KINDA_SMALL_NUMBER && MaxLimbLength > KINDA_SMALL_NUMBER)
        {
            const float ReachRatio = DesiredLength / MaxLimbLength;
            const float ScalingFactor = (MaxStretchScale - 1.f) * clamp((ReachRatio - StartStretchRatio) / ScaleRange, 0.f, 1.f);
            if (ScalingFactor > KINDA_SMALL_NUMBER)
            {
                LowerLimbLength *= (1.f + ScalingFactor);
                UpperLimbLength *= (1.f + ScalingFactor);
                MaxLimbLength   *= (1.f + ScalingFactor);
            }
        }
    }

    OutEndPos = DesiredPos;
    OutJointPos = JointPos;

    // If we are trying to reach a goal beyond the length of the limb, clamp it to something solvable and extend limb fully.
    if (DesiredLength >= MaxLimbLength)
    {
        OutEndPos = RootPos + (MaxLimbLength * DesiredDir);
        OutJointPos = RootPos + (UpperLimbLength * DesiredDir);
    }
    else
    {
        // So we have a triangle we know the side lengths of. We can work out the angle between DesiredDir and the direction of the upper limb
        // using the sin rule:
        const float TwoAB = 2.f * UpperLimbLength * DesiredLength;

        const float CosAngle = (TwoAB != 0.f) ? ((UpperLimbLength*UpperLimbLength) + (DesiredLength*DesiredLength) - (LowerLimbLength*LowerLimbLength)) / TwoAB : 0.f;

        // If CosAngle is less than 0, the upper arm actually points the opposite way to DesiredDir, so we handle that.
        const bool bReverseUpperBone = (CosAngle < 0.f);

        // Angle between upper limb and DesiredDir
        // ACos clamps internally so we dont need to worry about out-of-range values here.
        const float Angle = std::acos(CosAngle);

        // Now we calculate the distance of the joint from the root -> effector line.
        // This forms a right-angle triangle, with the upper limb as the hypotenuse.
        const float JointLineDist = UpperLimbLength * std::sin(Angle);

        // And the final side of that triangle - distance along DesiredDir of perpendicular.
        // ProjJointDistSqr can't be neg, because JointLineDist must be <= UpperLimbLength because appSin(Angle) is <= 1.
        const float ProjJointDistSqr = (UpperLimbLength*UpperLimbLength) - (JointLineDist*JointLineDist);

        // although this shouldn't be ever negative, sometimes Xbox release produces -0.f, causing ProjJointDist to be NaN
        // so now I branch it. 						
        float ProjJointDist = (ProjJointDistSqr > 0.f) ? std::sqrt(ProjJointDistSqr) : 0.f;
        if (bReverseUpperBone) ProjJointDist *= -1.f;

        // So now we can work out where to put the joint!
        OutJointPos = RootPos + (ProjJointDist * DesiredDir) + (JointLineDist * JointBendDir);
    }
}

struct shader_workbench : public GLFWApp
{
    GlCamera cam;
    FlyCameraController flycam;
    ShaderMonitor shaderMonitor{ "../assets/" };
    std::unique_ptr<gui::ImGuiManager> igm;
    GlGpuTimer gpuTimer;

    std::unique_ptr<GlGizmo> gizmo;

    float elapsedTime{ 0 };

    std::shared_ptr<GlShader> normalDebug;

    GlMesh sphere_mesh, cylinder_mesh;

    shader_workbench();
    ~shader_workbench();

    virtual void on_window_resize(int2 size) override;
    virtual void on_input(const InputEvent & event) override;
    virtual void on_update(const UpdateEvent & e) override;
    virtual void on_draw() override;
};
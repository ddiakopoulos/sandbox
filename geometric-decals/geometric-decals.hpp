#include "index.hpp"
#include "gl-gizmo.hpp"
#include "scene.hpp"
#include "assets.hpp"

enum DecalProjectionType
{
    PROJECTION_TYPE_CAMERA,
    PROJECTION_TYPE_NORMAL
};

struct DecalVertex
{
    float3 v, n;
    DecalVertex(float3 v, float3 n) : v(v), n(n) {}
    DecalVertex() {}
};

inline std::vector<DecalVertex> clip_face(const std::vector<DecalVertex> & inVertices, float3 dimensions, float3 plane)
{
    std::vector<DecalVertex> outVertices;

    float size = 0.5f * std::abs(dot(dimensions, plane));

    auto clip = [&](const DecalVertex v0, const DecalVertex v1, const float3 p)
    {
        float d0 = dot(v0.v, p) - size;
        float d1 = dot(v1.v, p) - size;
        float s = d0 / (d0 - d1);

        DecalVertex vert = {
            float3(v0.v.x + s * (v1.v.x - v0.v.x), v0.v.y + s * (v1.v.y - v0.v.y), v0.v.z + s * (v1.v.z - v0.v.z)),
            float3(v0.n.x + s * (v1.n.x - v0.n.x), v0.n.y + s * (v1.n.y - v0.n.y), v0.n.z + s * (v1.n.z - v0.n.z))
        };

        return vert;
    };

    for (int j = 0; j < inVertices.size(); j += 3)
    {
        float d1 = dot(inVertices[j + 0].v, plane) - size;
        float d2 = dot(inVertices[j + 1].v, plane) - size;
        float d3 = dot(inVertices[j + 2].v, plane) - size;

        int v1Out = d1 > 0.f;
        int v2Out = d2 > 0.f;
        int v3Out = d3 > 0.f;

        // How many verts are on this side of the plane?
        int total = v1Out + v2Out + v3Out;

        DecalVertex nV1, nV2, nV3, nV4;

        switch (total)
        {

        // None - don't clip the geometry
        case 0:
        {
            outVertices.push_back(inVertices[j + 0]);
            outVertices.push_back(inVertices[j + 1]);
            outVertices.push_back(inVertices[j + 2]);
            break;
        }

        // One vert
        case 1:
        {
            // v1 has been marked
            if (v1Out)
            {
                nV1 = inVertices[j + 1];
                nV2 = inVertices[j + 2];
                nV3 = clip(inVertices[j + 0], nV1, plane);
                nV4 = clip(inVertices[j + 0], nV2, plane);

                outVertices.push_back(nV1);
                outVertices.push_back(nV2);
                outVertices.push_back(nV3);

                outVertices.push_back(nV4);
                outVertices.push_back(nV3);
                outVertices.push_back(nV2);
            }

            // v1 has been marked
            if (v2Out)
            {
                nV1 = inVertices[j + 0];
                nV2 = inVertices[j + 2];
                nV3 = clip(inVertices[j + 1], nV1, plane);
                nV4 = clip(inVertices[j + 1], nV2, plane);

                outVertices.push_back(nV3);
                outVertices.push_back(nV2);
                outVertices.push_back(nV1);

                outVertices.push_back(nV2);
                outVertices.push_back(nV3);
                outVertices.push_back(nV4);
            }

            // v3 has been marked
            if (v3Out)
            {
                nV1 = inVertices[j + 0];
                nV2 = inVertices[j + 1];
                nV3 = clip(inVertices[j + 2], nV1, plane);
                nV4 = clip(inVertices[j + 2], nV2, plane);

                outVertices.push_back(nV1);
                outVertices.push_back(nV2);
                outVertices.push_back(nV3);

                outVertices.push_back(nV4);
                outVertices.push_back(nV3);
                outVertices.push_back(nV2);
            }

            break;
        }

        // Two verts
        case 2:
        {
            if (!v1Out)
            {
                nV1 = inVertices[j + 0];
                nV2 = clip(nV1, inVertices[j + 1], plane);
                nV3 = clip(nV1, inVertices[j + 2], plane);
                outVertices.push_back(nV1);
                outVertices.push_back(nV2);
                outVertices.push_back(nV3);
            }

            if (!v2Out)
            {
                nV1 = inVertices[j + 1];
                nV2 = clip(nV1, inVertices[j + 2], plane);
                nV3 = clip(nV1, inVertices[j + 0], plane);
                outVertices.push_back(nV1);
                outVertices.push_back(nV2);
                outVertices.push_back(nV3);
            }

            if (!v3Out)
            {
                nV1 = inVertices[j + 2];
                nV2 = clip(nV1, inVertices[j + 0], plane);
                nV3 = clip(nV1, inVertices[j + 1], plane);
                outVertices.push_back(nV1);
                outVertices.push_back(nV2);
                outVertices.push_back(nV3);
            }

            break;
        }

        // All outside
        case 3:
        {
            break;
        }
        }

    }

    return outVertices;
}

// http://blog.wolfire.com/2009/06/how-to-project-decals/
inline Geometry make_decal_geometry(Geometry & mesh, const Pose & pose, const Pose & cubePose, const float3 & dimensions)
{
    Geometry decal;
    std::vector<DecalVertex> finalVertices;

    assert(mesh.normals.size() > 0);

    for (int i = 0; i < mesh.faces.size(); i++)
    {
        uint3 f = mesh.faces[i];
        std::vector<DecalVertex> clippedVertices;

        for (int j = 0; j < 3; j++)
        {
            float3 v = mesh.vertices[f[j]];
            float3 n = mesh.normals[f[j]];
            v = transform_coord(pose.matrix(), v); // local into world
            v = transform_coord(cubePose.inverse().matrix(), v); // with the box
            clippedVertices.emplace_back(v, n);
        }

        // Clip X faces
        clippedVertices = clip_face(clippedVertices, dimensions, float3(1, 0, 0));
        clippedVertices = clip_face(clippedVertices, dimensions, float3(-1, 0, 0));

        // Clip Y faces
        clippedVertices = clip_face(clippedVertices, dimensions, float3(0, 1, 0));
        clippedVertices = clip_face(clippedVertices, dimensions, float3(0, -1, 0));

        // Clip Z faces
        clippedVertices = clip_face(clippedVertices, dimensions, float3(0, 0, 1));
        clippedVertices = clip_face(clippedVertices, dimensions, float3(0, 0, -1));

        // Projected coordinates are the texture coordinates
        for (int v = 0; v < clippedVertices.size(); v++)
        {
            auto & a = clippedVertices[v];
            decal.texcoord0.push_back(float2(0.5f + (a.v.x / dimensions.x), 0.5f + (a.v.y / dimensions.y)));
            a.v = transform_coord(cubePose.matrix(), a.v); // back to local
        }

        if (clippedVertices.size() == 0)
        {
            continue;
        }

        finalVertices.insert(finalVertices.end(), clippedVertices.begin(), clippedVertices.end());
    }

    for (int k = 0; k < finalVertices.size(); k += 3)
    {
        decal.faces.emplace_back(k, k + 1, k + 2);

        decal.vertices.push_back(finalVertices[k + 0].v);
        decal.vertices.push_back(finalVertices[k + 1].v);
        decal.vertices.push_back(finalVertices[k + 2].v);

        decal.normals.push_back(finalVertices[k + 0].n);
        decal.normals.push_back(finalVertices[k + 1].n);
        decal.normals.push_back(finalVertices[k + 2].n);
    }

    return decal;
}

struct shader_workbench : public GLFWApp
{
    GlCamera cam;
    FlyCameraController flycam;
    ShaderMonitor shaderMonitor{ "../assets/" };

    std::unique_ptr<gui::imgui_wrapper> igm;
    std::unique_ptr<GlGizmo> gizmo;

    GlShader litShader;
    GlTexture2D decalTex, emptyTex;
    DecalProjectionType projType = PROJECTION_TYPE_CAMERA;
    std::vector<StaticMesh> meshes;
    std::vector<GlMesh> decals;

    shader_workbench();
    ~shader_workbench();

    virtual void on_window_resize(int2 size) override;
    virtual void on_input(const InputEvent & event) override;
    virtual void on_update(const UpdateEvent & e) override;
    virtual void on_draw() override;
};
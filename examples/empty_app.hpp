#include "index.hpp"
#include <iterator>
#include "svd.hpp"
#include "gl-gizmo.hpp"

constexpr const char basic_wireframe_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    layout(location = 2) in vec3 inColor;
    uniform mat4 u_mvp;
    out vec3 color;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
        color = inColor;
    }
)";

constexpr const char basic_wireframe_frag[] = R"(#version 330
    in vec3 color;
    out vec4 f_color;
    uniform vec3 u_color;
    void main()
    {
        f_color = vec4(u_color, 1);
    }
)";

struct DebugSphere
{
    Pose p;
    float radius;

    Bounds3D get_bounds() const
    {
        const float3 rad3 = float3(radius, radius, radius);
        return { p.transform_coord(-rad3), p.transform_coord(rad3) };
    }
};

/*
 * An octree is a tree data structure in which each internal node has exactly
 * eight children. Octrees are most often used to partition a three
 * dimensional space by recursively subdividing it into eight octants. 
 * This implementation stores 8 pointers per node, instead of the other common
 * approach, which is to use a flat array with an offset. The `inside` method
 * defines the comparison function.
 * https://www.gamedev.net/resources/_/technical/game-programming/introduction-to-octrees-r3529
 * https://cs.brown.edu/courses/csci1230/lectures/CS123_17_Acceleration_Data_Structures_11.3.16.pptx.
 * http://www.piko3d.net/tutorials/space-partitioning-tutorial-piko3ds-dynamic-octree/
 */

// Instead of a strict bounds check which might force an object into a parent cell, this function
// checks centers, aka a "loose" octree. 
inline bool inside(const Bounds3D & node, const Bounds3D & other)
{
    // Compare centers
    if (!(linalg::all(greater(other.max(), node.center())) && linalg::all(less(other.min(), node.center())))) return false;

    // Otherwise ensure we shouldn't move to parent
    return linalg::all(less(node.size(), other.size()));
}

template<typename T>
struct SceneNodeContainer
{
    T & object;
    Bounds3D worldspaceBounds;
    SceneNodeContainer(T & obj, const Bounds3D & bounds) : object(obj), worldspaceBounds(bounds) {}
};

template<typename T>
struct SceneOctree
{
    template<typename T>
    struct Octant
    {
        std::list<SceneNodeContainer<T>> objects;

        Octant<T> * parent;
        Octant(Octant<T> * parent) : parent(parent) {}

        Bounds3D box;
        VoxelArray<std::unique_ptr<Octant<T>>> arr = { { 2, 2, 2 } };
        uint32_t occupancy{ 0 };

        int3 get_indices(const Bounds3D & other) const
        {
            const float3 a = other.center();
            const float3 b = box.center();
            int3 index;
            index.x = (a.x > b.x) ? 1 : 0;
            index.y = (a.y > b.y) ? 1 : 0;
            index.z = (a.z > b.z) ? 1 : 0;
            return index;
        }

        void increase_occupancy(Octant<T> * n) const
        {
            if (n != nullptr)
            {
                n->occupancy++;
                increase_occupancy(n->parent);
            }
        }

        void decrease_occupancy(Octant<T> * n) const
        {
            if (n != nullptr)
            {
                n->occupancy--;
                decrease_occupancy(n->parent);
            }
        }

        // Returns true if the other is less than half the size of myself
        bool check_fit(const Bounds3D & other) const
        {
            return all(lequal(other.size(), box.size() * 0.5f));
        }

    };

    enum CullStatus
    {
        INSIDE,
        INTERSECT,
        OUTSIDE
    };

    std::unique_ptr<Octant<T>> root;
    uint32_t maxDepth { 8 };

    SceneOctree(const uint32_t maxDepth = 8, const Bounds3D rootBounds = { { -4, -4, -4 }, { +4, +4, +4 } })
    {
        root.reset(new Octant<T>(nullptr));
        root->box = rootBounds;
    }

    ~SceneOctree() { }

    float3 get_resolution()
    {
        return root->box.size() / (float) maxDepth;
    }

    void add(SceneNodeContainer<T> & sceneNode, Octant<T> * child, int depth = 0)
    {
        if (!child) child = root.get();

        const Bounds3D bounds = sceneNode.worldspaceBounds;

        if (depth < maxDepth && child->check_fit(bounds))
        {
            int3 lookup = child->get_indices(bounds);

            // No child for this octant
            if (child->arr[lookup] == nullptr)
            {
                child->arr[lookup].reset(new Octant<T>(child));

                const float3 octantMin = child->box.min();
                const float3 octantMax = child->box.max();
                const float3 octantCenter = child->box.center();

                float3 min, max;
                for (int axis : { 0, 1, 2 })
                {
                    if (lookup[axis] == 0)
                    {
                        min[axis] = octantMin[axis];
                        max[axis] = octantCenter[axis];
                    }
                    else
                    {
                        min[axis] = octantCenter[axis];
                        max[axis] = octantMax[axis];
                    }
                }

                child->arr[lookup]->box = Bounds3D(min, max);
            }

            // Recurse into a new depth
            add(sceneNode, child->arr[lookup].get(), ++depth);
        }
        // The current octant fits this 
        else
        {
            child->increase_occupancy(child);
            child->objects.push_back(sceneNode);
        }
    }

    void create(SceneNodeContainer<T> && sceneNode)
    {
        if (!inside(sceneNode.worldspaceBounds, root->box))
        {
            throw std::invalid_argument("object is not in the bounding volume of the root node");
        }
        else
        {
            add(sceneNode, root.get());
        }
    }

    void cull(Frustum & camera, std::vector<Octant<T> *> & visibleNodeList, Octant<T> * node, bool alreadyVisible)
    {
        if (!node) node = root.get();
        if (node->occupancy == 0) return;

        CullStatus status = OUTSIDE;

        if (alreadyVisible)
        {
            status = INSIDE;
        }
        else if (node == root.get())
        {
            status = INTERSECT;
        }
        else
        {
            if (camera.contains(node->box.center()))
            {
                status = INSIDE;
            }
        }

        alreadyVisible = (status == INSIDE);

        if (alreadyVisible)
        {
            visibleNodeList.push_back(node);
        }

        // Recurse into children
        Octant<T> * child;
        if ((child = node->arr[{0, 0, 0}].get()) != nullptr) cull(camera, visibleNodeList, child, alreadyVisible);
        if ((child = node->arr[{0, 0, 1}].get()) != nullptr) cull(camera, visibleNodeList, child, alreadyVisible);
        if ((child = node->arr[{0, 1, 0}].get()) != nullptr) cull(camera, visibleNodeList, child, alreadyVisible);
        if ((child = node->arr[{0, 1, 1}].get()) != nullptr) cull(camera, visibleNodeList, child, alreadyVisible);
        if ((child = node->arr[{1, 0, 0}].get()) != nullptr) cull(camera, visibleNodeList, child, alreadyVisible);
        if ((child = node->arr[{1, 0, 1}].get()) != nullptr) cull(camera, visibleNodeList, child, alreadyVisible);
        if ((child = node->arr[{1, 1, 0}].get()) != nullptr) cull(camera, visibleNodeList, child, alreadyVisible);
        if ((child = node->arr[{1, 1, 1}].get()) != nullptr) cull(camera, visibleNodeList, child, alreadyVisible);
    }
};

template<typename T>
inline void octree_debug_draw(
    const SceneOctree<T> & octree, 
    GlShader * shader, 
    GlMesh * boxMesh, 
    GlMesh * sphereMesh, 
    const float4x4 & viewProj, 
    typename SceneOctree<T>::Octant<T> * node, // rumble rumble something about dependent types
    float3 octantColor)
{
    if (!node) node = octree.root.get();

    shader->bind();

    const auto boxModel = mul(make_translation_matrix(node->box.center()), make_scaling_matrix(node->box.size() / 2.f));
    shader->uniform("u_color", octantColor);
    shader->uniform("u_mvp", mul(viewProj, boxModel));
    boxMesh->draw_elements();

    for (auto obj : node->objects)
    {
        const auto & object = obj.object;
        const auto sphereModel = mul(object.p.matrix(), make_scaling_matrix(object.radius));
        shader->uniform("u_color", octantColor);
        shader->uniform("u_mvp", mul(viewProj, sphereModel));
        sphereMesh->draw_elements();
    }

    shader->unbind();

    // Recurse into children
    SceneOctree<T>::Octant<T> * child;
    if ((child = node->arr[{0, 0, 0}].get()) != nullptr) octree_debug_draw<T>(octree, shader, boxMesh, sphereMesh, viewProj, child, { 0, 0, 0 });
    if ((child = node->arr[{0, 0, 1}].get()) != nullptr) octree_debug_draw<T>(octree, shader, boxMesh, sphereMesh, viewProj, child, { 0, 0, 1 });
    if ((child = node->arr[{0, 1, 0}].get()) != nullptr) octree_debug_draw<T>(octree, shader, boxMesh, sphereMesh, viewProj, child, { 0, 1, 0 });
    if ((child = node->arr[{0, 1, 1}].get()) != nullptr) octree_debug_draw<T>(octree, shader, boxMesh, sphereMesh, viewProj, child, { 0, 1, 1 });
    if ((child = node->arr[{1, 0, 0}].get()) != nullptr) octree_debug_draw<T>(octree, shader, boxMesh, sphereMesh, viewProj, child, { 1, 0, 0 });
    if ((child = node->arr[{1, 0, 1}].get()) != nullptr) octree_debug_draw<T>(octree, shader, boxMesh, sphereMesh, viewProj, child, { 1, 0, 1 });
    if ((child = node->arr[{1, 1, 0}].get()) != nullptr) octree_debug_draw<T>(octree, shader, boxMesh, sphereMesh, viewProj, child, { 1, 1, 0 });
    if ((child = node->arr[{1, 1, 1}].get()) != nullptr) octree_debug_draw<T>(octree, shader, boxMesh, sphereMesh, viewProj, child, { 1, 1, 1 });
}

bool toggleDebug = false;

struct ExperimentalApp : public GLFWApp
{
    std::unique_ptr<GlShader> wireframeShader;

    GlCamera debugCamera;
    FlyCameraController cameraController;

    UniformRandomGenerator rand;

    std::vector<DebugSphere> meshes;

    GlMesh sphere, box, frustum;

    SceneOctree<DebugSphere> octree;

    std::unique_ptr<GlGizmo> gizmo;
    tinygizmo::rigid_transform xform;

    Pose externalCam;

    ExperimentalApp() : GLFWApp(1280, 800, "Nearly Empty App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        gl_check_error(__FILE__, __LINE__);

        gizmo.reset(new GlGizmo());
        xform.position = { 0.1f, 0.1f, 0.1f };

        wireframeShader.reset(new GlShader(basic_wireframe_vert, basic_wireframe_frag));

        debugCamera.look_at({0, 3.0, -3.5}, {0, 2.0, 0});
        cameraController.set_camera(&debugCamera);

        externalCam = look_at_pose_rh({ 0, 3.0, 5.0f }, { 0, 2.0, -0.001f });

        frustum = {};

        sphere = make_sphere_mesh(1.f);
        box = make_cube_mesh();
        box.set_non_indexed(GL_LINES);

        for (int i = 0; i < 32; ++i)
        {
            const float3 position = { rand.random_float(8.f) - 4.f, rand.random_float(8.f) - 4.f, rand.random_float(8.f) - 4.f };
            const float radius = rand.random_float(0.25f);

            DebugSphere s; 
            s.p = Pose(float4(0, 0, 0, 1), position);
            s.radius = radius;

            meshes.push_back(std::move(s));
        }

        {
            scoped_timer create("octree create");
            for (auto & sph : meshes)
            {
                octree.create(std::move(SceneNodeContainer<DebugSphere>(sph, sph.get_bounds())));
            }
        }

    }
    
    void on_window_resize(int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        cameraController.handle_input(event);
        if (gizmo) gizmo->handle_input(event);

        if (event.type == InputEvent::KEY && event.value[0] == GLFW_KEY_SPACE && event.action == GLFW_RELEASE)
        {
            toggleDebug = !toggleDebug;
        }
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
    }
    
    void on_draw() override
    {
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
        
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
     
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

        if (gizmo) gizmo->update(debugCamera, float2(width, height));
        tinygizmo::transform_gizmo("destination", gizmo->gizmo_ctx, xform);

        const float4x4 proj = debugCamera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = debugCamera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);

        if (toggleDebug)
        {
            octree_debug_draw<DebugSphere>(octree, wireframeShader.get(), &box, &sphere, viewProj, nullptr, float3());
        }

        float3 xformPosition = { xform.position.x, xform.position.y, xform.position.z };
        //Bounds3D worldspaceCameraVolume = { xformPosition - float3(0.5f), xformPosition + float3(0.5f) };

        /*
        wireframeShader->bind();
        const auto model = mul(make_translation_matrix(xformPosition), make_scaling_matrix(0.5f));
        wireframeShader->uniform("u_color", float3(1, 1, 1));
        wireframeShader->uniform("u_mvp", mul(viewProj, model));
        box.draw_elements();
        wireframeShader->unbind();
        */

        /*
        const float4x4 visualizeViewProj = mul(make_perspective_matrix(to_radians(45.f), 1.0f, 0.1f, 10.f), inverse(externalCam.matrix()));

        Frustum f(visualizeViewProj);
        auto generated_frustum_corners = make_frustum_corners(f);

        float3 ftl = generated_frustum_corners[0]; float3 fbr = generated_frustum_corners[1];
        float3 fbl = generated_frustum_corners[2]; float3 ftr = generated_frustum_corners[3];
        float3 ntl = generated_frustum_corners[4]; float3 nbr = generated_frustum_corners[5];
        float3 nbl = generated_frustum_corners[6]; float3 ntr = generated_frustum_corners[7];

        std::vector<float3> frustum_coords = {
            ntl, ntr, ntr, nbr, nbr, nbl, nbl, ntl, // near quad
            ntl, ftl, ntr, ftr, nbr, fbr, nbl, fbl, // between
            ftl, ftr, ftr, fbr, fbr, fbl, fbl, ftl, // far quad
        };

        Geometry g;
        for (auto & v : frustum_coords) g.vertices.push_back(v);
        frustum = make_mesh_from_geometry(g);
        glLineWidth(2.f);
        frustum.set_non_indexed(GL_LINES);

        // Visualize the frustum
        wireframeShader->bind();
        wireframeShader->uniform("u_color", float3(1, 0, 0));
        wireframeShader->uniform("u_mvp", mul(viewProj, Identity4x4));
        frustum.draw_elements();

        */


        std::vector<SceneOctree<DebugSphere>::Octant<DebugSphere> *> visibleNodes;
        Frustum camFrustum(viewProj);

        wireframeShader->bind();

        for (auto & sph : meshes)
        {
            const auto model = mul(sph.p.matrix(), make_scaling_matrix(sph.radius));
            wireframeShader->uniform("u_color", camFrustum.contains(sph.p.position) ? float3(1, 1, 1) : float3(0, 0, 0));
            wireframeShader->uniform("u_mvp", mul(viewProj, model));
            sphere.draw_elements();
        }

        wireframeShader->unbind();

        octree.cull(camFrustum, visibleNodes, nullptr, false);
        uint32_t visibleObjects = 0;
        for (auto node : visibleNodes)
        {
            float4x4 boxModel = mul(make_translation_matrix(node->box.center()), make_scaling_matrix(node->box.size() / 2.f));
            wireframeShader->bind();
            wireframeShader->uniform("u_mvp", mul(viewProj, boxModel));
            box.draw_elements();

            for (auto obj : node->objects)
            {
                const auto & object = obj.object;
                const auto sphereModel = mul(object.p.matrix(), make_scaling_matrix(object.radius));
                wireframeShader->uniform("u_mvp", mul(viewProj, sphereModel));
                sphere.draw_elements();
            }

            visibleObjects += node->objects.size();

            wireframeShader->unbind();
        }
 
        std::cout << "Visible Objects: " << visibleObjects << std::endl;


        if (gizmo) gizmo->draw();

        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
    }
    
};
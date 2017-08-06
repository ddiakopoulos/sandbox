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
        Bounds3D b(p.transform_coord(-rad3), p.transform_coord(rad3)), result;

        for (auto x : { b.min().x,b.max().x })
        {
            for (auto y : { b.min().y,b.max().y })
            {
                for (auto z : { b.min().z,b.max().z })
                {
                    result.surround(p.transform_coord({ x,y,z }));
                }
            }
        }
        return b;
    }

};


/*
* An octree is a tree data structure in which each internal node has exactly
* eight children. Octrees are most often used to partition a three
* dimensional space by recursively subdividing it into eight octants.
*/

// http://thomasdiewald.com/blog/?p=1488
// https://www.gamedev.net/resources/_/technical/game-programming/introduction-to-octrees-r3529
// https://cs.brown.edu/courses/csci1230/lectures/CS123_17_Acceleration_Data_Structures_11.3.16.pptx.
// http://www.piko3d.net/tutorials/space-partitioning-tutorial-piko3ds-dynamic-octree/

inline bool inside(const Bounds3D & node, const Bounds3D & other)
{
    // Compare centers
    if (!(linalg::all(greater(other.max(), node.center())) && linalg::all(less(other.min(), node.center())))) return false;

    // Otherwise ensure we shouldn't move to parent
    return linalg::all(less(node.size(), other.size()));
}

struct SceneOctree
{

    struct Node
    {
        std::list<DebugSphere *> spheres;

        std::unique_ptr<Node> parent;
        Node(Node * parent) : parent(parent) {}
        Bounds3D box;
        VoxelArray<Node *> arr = { { 2, 2, 2 } };
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

        void increase_occupancy()
        {
            occupancy++;
            if (parent) parent->occupancy++;
        }

        void decrease_occupancy()
        {
            occupancy--;
            if (parent) parent->occupancy--;
        }

        // Returns true if the other is less than half the size of myself
        bool check_fit(const Bounds3D & other) const
        {
            return all(lequal(other.size(), box.size() * 0.5f));
        }

    };

    /////////////////////////////

    Node * root;
    uint32_t maxDepth = { 8 };

    SceneOctree()
    {
        root = new Node(nullptr);
        root->box = Bounds3D({ -4, -4, -4 }, { +4, +4, +4 });
    }

    ~SceneOctree() { }

    float3 get_resolution()
    {
        return root->box.size() / (float)maxDepth;
    }

    void add(DebugSphere * sphere, Node * child, int depth = 0)
    {
        if (!child) child = root;

        //std::cout << "Current Depth: " << depth << std::endl;

        const Bounds3D bounds = sphere->get_bounds();
        //std::cout << "Sphere bounds: " << sphere->get_bounds() << std::endl;

        if (depth < maxDepth && child->check_fit(bounds))
        {
            int3 lookup = child->get_indices(bounds);

            // Bounds here is always [0 to something] or [something to 0]
            //std::cout << "lookup: " << bounds << ", " << lookup << std::endl;

            // No child for this octant
            if (child->arr[lookup] == nullptr)
            {
                //std::cout << "Creating Child For: " << lookup << std::endl;

                child->arr[lookup] = new Node(child);

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
               // std::cout << "... child has bounds: " << child->arr[lookup]->box << std::endl;
            }

            //std::cout << "Calling Add Recursively..." << std::endl;

            // Recurse into a new depth
            add(sphere, child->arr[lookup], ++depth);
        }
        else
        {
            std::cout << "Else increase_occupancy(-------------------)" << std::endl;

            child->increase_occupancy();
            child->spheres.push_back(sphere);
        }
    }

    void create(DebugSphere * sphere)
    {
        // todo: valid box / root check
        //if (!node) return;

        const Bounds3D bounds = sphere->get_bounds();

        if (!inside(bounds, root->box))
        {
            std::cout << "Node: " << bounds << " not inside " << root->box << std::endl;
            //root->increase_occupancy();
        }
        else
        {
            std::cout << "+++++++++++ Adding Node: " << sphere << std::endl;
            add(sphere, root); // start at depth 0 again
        }
    }

    void remove(Renderable * node)
    {

    }

    // Debugging Only
    void debug_draw(GlShader * shader, GlMesh * mesh, GlMesh * sphereMesh, const float4x4 & viewProj, Node * node, float3 coordinate)
    {
        if (!node) node = root;

        //if (node->occupancy == 0)
        //{
            //std::cout << "RETURNING ON NODE " << node << std::endl;
        //    return;
        //}

        //debugRenderer.draw_box(node->box, coordinate);

        float4x4 model = mul(make_translation_matrix(node->box.center()), make_scaling_matrix(node->box.size() / 2.f));
        shader->bind();
        shader->uniform("u_color", coordinate);
        shader->uniform("u_mvp", mul(viewProj, model));
        mesh->draw_elements();

        for (auto s : node->spheres)
        {
            const auto sphereModel = mul(s->p.matrix(), make_scaling_matrix(s->radius));
            shader->uniform("u_color", coordinate);
            shader->uniform("u_mvp", mul(viewProj, sphereModel));
            sphereMesh->draw_elements();
        }

        shader->unbind();
   
        //std::cout << "Drawing: " << node->box << std::endl;
        //std::cout << "Occupancy: " << node->occupancy << std::endl;

        // Recurse into children
        Node * child;
        if ((child = node->arr[{0, 0, 0}]) != nullptr) debug_draw(shader, mesh, sphereMesh, viewProj, child, { 0, 0, 0 });
        if ((child = node->arr[{0, 0, 1}]) != nullptr) debug_draw(shader, mesh, sphereMesh, viewProj, child, { 0, 0, 1 });
        if ((child = node->arr[{0, 1, 0}]) != nullptr) debug_draw(shader, mesh, sphereMesh, viewProj, child, { 0, 1, 0 });
        if ((child = node->arr[{0, 1, 1}]) != nullptr) debug_draw(shader, mesh, sphereMesh, viewProj, child, { 0, 1, 1 });
        if ((child = node->arr[{1, 0, 0}]) != nullptr) debug_draw(shader, mesh, sphereMesh, viewProj, child, { 1, 0, 0 });
        if ((child = node->arr[{1, 0, 1}]) != nullptr) debug_draw(shader, mesh, sphereMesh, viewProj, child, { 1, 0, 1 });
        if ((child = node->arr[{1, 1, 0}]) != nullptr) debug_draw(shader, mesh, sphereMesh, viewProj, child, { 1, 1, 0 });
        if ((child = node->arr[{1, 1, 1}]) != nullptr) debug_draw(shader, mesh, sphereMesh, viewProj, child, { 1, 1, 1 });
    }

    enum Position
    {
        INSIDE,
        INTERSECT,
        OUTSIDE
    };

    // Debugging Only
    void cull(Bounds3D & camera, GlShader * shader, GlMesh * mesh, GlMesh * sphereMesh, const float4x4 & viewProj, Node * node, float3 coordinate, bool alreadyVisible)
    {
        if (!node) node = root;

        /*
        // Don't traverse empty parts
        if (node->occupancy == 0)
        {
            if (node != root)
            {
                //std::cout << "Returning..." << std::endl;
                return;
            }
        }
        */

        std::cout << "occupancy..." << node->occupancy << std::endl;

        const Bounds3D box = node->box;

        Position position = OUTSIDE;

        if (alreadyVisible)
        {
            position = INSIDE;
        }
        else if (node == root)
        {
            position = INTERSECT;
        }
        else
        {
            // Then we can assume all children of this node also intersect 
            if (box.contains(camera.center())) // full intersection
            {
                position = INSIDE; 
            }
        }

        alreadyVisible = (position == INSIDE);

        if (alreadyVisible)
        {
            float4x4 boxModel = mul(make_translation_matrix(node->box.center()), make_scaling_matrix(node->box.size() / 2.f));
            shader->bind();
            shader->uniform("u_color", coordinate);
            shader->uniform("u_mvp", mul(viewProj, boxModel));
            mesh->draw_elements();

            for (auto s : node->spheres)
            {
                const auto sphereModel = mul(s->p.matrix(), make_scaling_matrix(s->radius));
                shader->uniform("u_color", coordinate);
                shader->uniform("u_mvp", mul(viewProj, sphereModel));
                sphereMesh->draw_elements();
            }

            shader->unbind();
        }

        // Recurse into children
        Node * child;
        if ((child = node->arr[{0, 0, 0}]) != nullptr) cull(camera, shader, mesh, sphereMesh, viewProj, child, { 0, 0, 0 }, alreadyVisible);
        if ((child = node->arr[{0, 0, 1}]) != nullptr) cull(camera, shader, mesh, sphereMesh, viewProj, child, { 0, 0, 1 }, alreadyVisible);
        if ((child = node->arr[{0, 1, 0}]) != nullptr) cull(camera, shader, mesh, sphereMesh, viewProj, child, { 0, 1, 0 }, alreadyVisible);
        if ((child = node->arr[{0, 1, 1}]) != nullptr) cull(camera, shader, mesh, sphereMesh, viewProj, child, { 0, 1, 1 }, alreadyVisible);
        if ((child = node->arr[{1, 0, 0}]) != nullptr) cull(camera, shader, mesh, sphereMesh, viewProj, child, { 1, 0, 0 }, alreadyVisible);
        if ((child = node->arr[{1, 0, 1}]) != nullptr) cull(camera, shader, mesh, sphereMesh, viewProj, child, { 1, 0, 1 }, alreadyVisible);
        if ((child = node->arr[{1, 1, 0}]) != nullptr) cull(camera, shader, mesh, sphereMesh, viewProj, child, { 1, 1, 0 }, alreadyVisible);
        if ((child = node->arr[{1, 1, 1}]) != nullptr) cull(camera, shader, mesh, sphereMesh, viewProj, child, { 1, 1, 1 }, alreadyVisible);
    }
};


inline Geometry coordinate_system_geometry()
{
    coord_system opengl_coords { coord_axis::right, coord_axis::up, coord_axis::back }; // traditional rh opengl

    Geometry axis;

    for (auto a : { opengl_coords.get_right(), opengl_coords.get_up(), opengl_coords.get_forward() })
    {
        axis.vertices.emplace_back(0.f, 0.f, 0.f);
        axis.vertices.emplace_back(a);

        axis.colors.emplace_back(abs(a), 1.f);
        axis.colors.emplace_back(abs(a), 1.f);
    }

    return axis;
}

inline GlMesh make_coordinate_system_mesh()
{
    auto m = make_mesh_from_geometry(coordinate_system_geometry());
    m.set_non_indexed(GL_LINES);
    return m;
}

bool toggleDebug = false;

struct ExperimentalApp : public GLFWApp
{
    std::unique_ptr<GlShader> wireframeShader;

    GlCamera debugCamera;
    FlyCameraController cameraController;

    UniformRandomGenerator rand;

    std::vector<DebugSphere> meshes;

    GlMesh sphere, box;

    SceneOctree octree;

    std::unique_ptr<GlGizmo> gizmo;
    tinygizmo::rigid_transform xform;

    ExperimentalApp() : GLFWApp(1280, 800, "Nearly Empty App")
    {
        svd_tests::execute();

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        gl_check_error(__FILE__, __LINE__);

        gizmo.reset(new GlGizmo());

        wireframeShader.reset(new GlShader(basic_wireframe_vert, basic_wireframe_frag));

        debugCamera.look_at({0, 3.0, -3.5}, {0, 2.0, 0});
        cameraController.set_camera(&debugCamera);

        sphere = make_sphere_mesh(1.f);
        box = make_cube_mesh();
        box.set_non_indexed(GL_LINES);

        for (int i = 0; i < 16; ++i)
        {
            // Generate a random position between [-3, 3]
            float3 position = { rand.random_float(8.f) - 4.f, rand.random_float(8.f) - 4.f, rand.random_float(8.f) - 4.f };

            float radius = 0.05f; // rand.random_float(0.1);

            std::cout << "Position: " << position << ", radius: " << radius << std::endl;

            DebugSphere s;
            s.p = Pose(float4(0, 0, 0, 1), position);
            s.radius = radius;

            meshes.push_back(s);
        }

        for (auto & sph : meshes)
        {
            std::cout << "Create: " << &sph << std::endl;
            octree.create(&sph);
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

        const auto proj = debugCamera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = debugCamera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);

        wireframeShader->bind();

        /*
        for (auto & sph : meshes)
        {
            const auto model = mul(sph.p.matrix(), make_scaling_matrix(sph.radius));
            wireframeShader->uniform("u_color", float3(0, 0, 0));
            wireframeShader->uniform("u_mvp", mul(viewProj, model));
            sphere.draw_elements();
        }
        */

        if (toggleDebug) octree.debug_draw(wireframeShader.get(), &box, &sphere, viewProj, nullptr, float3());

        float3 xformPosition = { xform.position.x, xform.position.y, xform.position.z };
        Bounds3D worldspaceCameraVolume = { xformPosition - float3(0.5f), xformPosition + float3(0.5f) };

        wireframeShader->bind();
        const auto model = mul(make_translation_matrix(xformPosition), make_scaling_matrix(0.5f));
        wireframeShader->uniform("u_color", float3(1, 1, 1));
        wireframeShader->uniform("u_mvp", mul(viewProj, model));
        box.draw_elements();
        wireframeShader->unbind();

        octree.cull(worldspaceCameraVolume, wireframeShader.get(), &box, &sphere, viewProj, nullptr, float3(), false);

        if (gizmo) gizmo->draw();

        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
    }
    
};
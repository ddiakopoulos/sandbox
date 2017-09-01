#include "index.hpp"
#include "../virtual-reality/assets.hpp"
#include "gl-scene.hpp"

#include "cereal/cereal.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/polymorphic.hpp"
#include <cereal/types/base_class.hpp>
#include "cereal/archives/json.hpp"

using namespace avl;

struct BaseClass
{
    virtual void t() = 0;
    Pose pose{ { -1, -1, -1 } };
    std::string id = "base";
    float scale = -1;
};

struct DerivedClassA : public BaseClass
{
    virtual void t() override {}
    bool field = true;

};

struct DerivedClassB : public BaseClass
{
    virtual void t() override {}
};

CEREAL_REGISTER_TYPE(DerivedClassA);
CEREAL_REGISTER_TYPE(DerivedClassB);
CEREAL_REGISTER_POLYMORPHIC_RELATION(BaseClass, DerivedClassA)
CEREAL_REGISTER_POLYMORPHIC_RELATION(BaseClass, DerivedClassB)

namespace cereal
{
    template<class Archive> void serialize(Archive & archive, float2 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y)); }
    template<class Archive> void serialize(Archive & archive, float3 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y), cereal::make_nvp("z", m.z)); }
    template<class Archive> void serialize(Archive & archive, float4 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y), cereal::make_nvp("z", m.z), cereal::make_nvp("w", m.w)); }

    template<class Archive> void serialize(Archive & archive, float2x2 & m) { archive(cereal::make_size_tag(2), m[0], m[1]); }
    template<class Archive> void serialize(Archive & archive, float3x3 & m) { archive(cereal::make_size_tag(3), m[0], m[1], m[2]); }
    template<class Archive> void serialize(Archive & archive, float4x4 & m) { archive(cereal::make_size_tag(4), m[0], m[1], m[2], m[3]); }
    template<class Archive> void serialize(Archive & archive, Frustum & m) { archive(cereal::make_size_tag(6)); for (auto const & p : m.planes) archive(p); }

    template<class Archive> void serialize(Archive & archive, Pose & m) { archive(cereal::make_nvp("position", m.position), cereal::make_nvp("orientation", m.orientation)); }
    template<class Archive> void serialize(Archive & archive, Bounds2D & m) { archive(cereal::make_nvp("min", m._min), cereal::make_nvp("max", m._max)); }
    template<class Archive> void serialize(Archive & archive, Bounds3D & m) { archive(cereal::make_nvp("min", m._min), cereal::make_nvp("max", m._max)); }
    template<class Archive> void serialize(Archive & archive, Ray & m) { archive(cereal::make_nvp("origin", m.origin), cereal::make_nvp("direction", m.direction)); }
    template<class Archive> void serialize(Archive & archive, Plane & m) { archive(cereal::make_nvp("equation", m.equation)); }
    template<class Archive> void serialize(Archive & archive, Line & m) { archive(cereal::make_nvp("origin", m.origin), cereal::make_nvp("direction", m.direction)); }
    template<class Archive> void serialize(Archive & archive, Segment & m) { archive(cereal::make_nvp("a", m.a), cereal::make_nvp("b", m.b)); }
    template<class Archive> void serialize(Archive & archive, Sphere & m) { archive(cereal::make_nvp("center", m.center), cereal::make_nvp("radius", m.radius)); }

    template<class Archive> void serialize(Archive & archive, BaseClass & m)
    { 
        archive(cereal::make_nvp("pose", m.pose));
        archive(cereal::make_nvp("scale", m.scale));
        archive(cereal::make_nvp("id", m.id));
    }

    template<class Archive> void serialize(Archive & archive, DerivedClassA & m)
    {
        archive(cereal::make_nvp("abse", cereal::base_class<BaseClass>(&m))); // also virtual_base_class
        archive(cereal::make_nvp("override", m.field));
    }

    template<class Archive> void serialize(Archive & archive, DerivedClassB & m)
    {
        archive(cereal::make_nvp("id", m.id));
    }
}

template <typename T>
std::string ToJson(T e) 
{
    
    std::ostringstream oss;
    {
        cereal::JSONOutputArchive json(oss);

        // CEREAL_NVP
        //json(e);

        //serialize(json, e);
        json(e);
    }


    return oss.str();
}

struct ExperimentalApp : public GLFWApp
{

    ExperimentalApp() : GLFWApp(600, 600, "Asset Test")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);

        GlShader wireframe = GlShader(
            read_file_text("../assets/shaders/wireframe_vert.glsl"), 
            read_file_text("../assets/shaders/wireframe_frag.glsl"),
            read_file_text("../assets/shaders/wireframe_geom.glsl"));

        std::cout << "Created: " << wireframe.handle() << std::endl;

        global_register_asset("wireframe-shader", std::move(wireframe));

        {
            AssetHandle<GlShader> shader("wireframe-shader");
            std::cout << "Got: " << shader.get().handle() << std::endl;
        }

        for (auto & shader : AssetHandle<GlShader>::list())
        {
            std::cout << "List (Before): " << shader.get().handle() << std::endl;

            GlShader wireframe = GlShader(
                read_file_text("../assets/shaders/wireframe_vert.glsl"),
                read_file_text("../assets/shaders/wireframe_frag.glsl"),
                read_file_text("../assets/shaders/wireframe_geom.glsl"));

            std::cout << "A new asset: " << wireframe.handle() << std::endl;

            shader.assign(std::move(wireframe));
        }

        for (auto a : AssetHandle<GlShader>::list())
        {
            std::cout << "List (After): " << a.get().handle() << std::endl;
        }

        {
            AssetHandle<GlShader> shader("wireframe-shader");
            std::cout << "Got: " << shader.get().handle() << std::endl;
        }


        std::cout << "Exiting..." << std::endl;

        //std::cout << ToJson(float2(1, 2)) << std::endl;
        //std::cout << ToJson(float3(1, 2, 3)) << std::endl;
        //std::cout << ToJson(float4(1, 2, 3, 4)) << std::endl;
        //std::cout << ToJson(Identity3x3) << std::endl;
        //std::cout << ToJson(Identity4x4) << std::endl;
        //std::cout << ToJson(Pose()) << std::endl;
        //std::cout << ToJson(Ray()) << std::endl;
        //std::cout << ToJson(Frustum()) << std::endl;

        /*
        auto w = std::make_shared<DerivedClassA>();
        w->pose.position = float3(10, 20, 30);

        std::string derived = ToJson(w);

        std::istringstream instr(derived); 

        cereal::JSONInputArchive inJson(instr);
        std::shared_ptr<DerivedClassA> derivedPtr;
        inJson(derivedPtr);
        std::cout << derivedPtr->pose << std::endl;
        */

        /*
        std::string jsonObjects;
            
        {
            std::vector<std::shared_ptr<BaseClass>> sceneObjects;
            for (int i = 0; i < 6; ++i)
            {
                auto someNewObjectA = std::make_shared<DerivedClassA>();
                someNewObjectA->id = "class a";

                auto someNewObjectB = std::make_shared<DerivedClassB>();
                someNewObjectB->id = "class b";

                sceneObjects.push_back(someNewObjectA);
                sceneObjects.push_back(someNewObjectB);
            }

            jsonObjects = ToJson(sceneObjects);
            std::cout << jsonObjects << std::endl;
        }

        {
            std::vector<std::shared_ptr<BaseClass>> sceneObjects;

            std::istringstream inStringStream(jsonObjects);
            cereal::JSONInputArchive inJson(inStringStream);

            inJson(sceneObjects);
            
            for (auto & obj : sceneObjects)
            {
                std::cout << obj->id << std::endl;
            }
        }

        */

        /*
        {
            AssetDatabase<GlTexture2D> textures;

            auto loadTexEmptyTex = []() -> GlTexture2D
            {
                GlTexture2D newTex;
                std::cout << "Generated Handle: " << newTex << std::endl;
                return newTex;
            };

            textures.register_asset("empty-tex", loadTexEmptyTex());

            {
                auto & tex = textures.get_asset("empty-tex");
                std::cout << "Got: " << tex << std::endl;
            }

            auto list = textures.list();
            for (auto tex : list)
            {
                std::cout << "List: " << tex->name << std::endl;

                GlTexture2D someNewHandle;
                std::cout << "A new asset: " << someNewHandle << std::endl;

                tex->asset = std::move(someNewHandle);
            }

        }
        */

    }
    
    void on_window_resize(int2 size) override {}

    void on_input(const InputEvent & event) override {}

    void on_update(const UpdateEvent & e) override {}
    
    void on_draw() override
    {
        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);
    }
  
};
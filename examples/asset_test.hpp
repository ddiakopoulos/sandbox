#include "index.hpp"
#include "../virtual-reality/assets.hpp"

#include "cereal/cereal.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/valarray.hpp"
#include "cereal/archives/json.hpp"

using namespace avl;

namespace cereal
{
    template<class Archive> void serialize(Archive & archive, float2 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y)); }
    template<class Archive> void serialize(Archive & archive, float3 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y), cereal::make_nvp("z", m.z)); }
    template<class Archive> void serialize(Archive & archive, float4 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y), cereal::make_nvp("z", m.z), cereal::make_nvp("w", m.w)); }


    template<class Archive> void serialize(Archive & archive, Pose & m) { archive(cereal::make_nvp("position", m.position), cereal::make_nvp("orientation", m.orientation)); }

    template<class Archive> void serialize(Archive & archive, Bounds2D & m) { archive(cereal::make_nvp("min", m._min), cereal::make_nvp("max", m._max)); }
    template<class Archive> void serialize(Archive & archive, Bounds3D & m) { archive(cereal::make_nvp("min", m._min), cereal::make_nvp("max", m._max)); }
    template<class Archive> void serialize(Archive & archive, Ray & m) { archive(cereal::make_nvp("origin", m.origin), cereal::make_nvp("direction", m.direction)); }
    template<class Archive> void serialize(Archive & archive, Plane & m) { archive(cereal::make_nvp("equation", m.equation)); }
    template<class Archive> void serialize(Archive & archive, Line & m) { archive(cereal::make_nvp("origin", m.origin), cereal::make_nvp("direction", m.direction)); }
    template<class Archive> void serialize(Archive & archive, Segment & m) { archive(cereal::make_nvp("a", m.a), cereal::make_nvp("b", m.b)); }
    template<class Archive> void serialize(Archive & archive, Sphere & m) { archive(cereal::make_nvp("center", m.center), cereal::make_nvp("radius", m.radius)); }

    template<class Archive> void serialize(Archive & archive, float2x2 & m) { archive(cereal::make_size_tag(2), m[0], m[1]); }
    template<class Archive> void serialize(Archive & archive, float3x3 & m) { archive(cereal::make_size_tag(3), m[0], m[1], m[2]); }
    template<class Archive> void serialize(Archive & archive, float4x4 & m) { archive(cereal::make_size_tag(4), m[0], m[1], m[2], m[3]); }
    template<class Archive> void serialize(Archive & archive, Frustum & m) { archive(cereal::make_size_tag(6)); for (auto const & p : m.planes) archive(p); }
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

       // std::cout << ToJson(float2(1, 2)) << std::endl;
        //std::cout << ToJson(float3(1, 2, 3)) << std::endl;
        //std::cout << ToJson(float4(1, 2, 3, 4)) << std::endl;
        //std::cout << ToJson(Identity3x3) << std::endl;
        std::cout << ToJson(Identity4x4) << std::endl;
        //std::cout << ToJson(Pose()) << std::endl;
        //std::cout << ToJson(Ray()) << std::endl;

        //std::cout << ToJson(Frustum()) << std::endl;

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

            std::cout << "Exiting..." << std::endl;
        }
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
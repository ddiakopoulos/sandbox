#include "index.hpp"
#include "../virtual-reality/assets.hpp"

struct ExperimentalApp : public GLFWApp
{

    ExperimentalApp() : GLFWApp(600, 600, "Asset Test")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);

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
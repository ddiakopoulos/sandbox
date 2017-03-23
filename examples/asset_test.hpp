#include "index.hpp"
#include "../virtual-reality/assets.hpp"

struct ExperimentalApp : public GLFWApp
{
    AssetDatabase<GlTexture2D> textures;

    ExperimentalApp() : GLFWApp(600, 600, "Lens Dev")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);

        auto loadTexEmptyTex = []() -> GlTexture2D
        {
            GlTexture2D newTex;
            std::cout << "Generated Tex Handle:" << newTex << std::endl;
            return newTex;
        };

        textures.register_asset("empty-tex", loadTexEmptyTex());
        
        auto & tex = textures.get_asset("empty-tex");

        std::cout << "Got: " << tex << std::endl;
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
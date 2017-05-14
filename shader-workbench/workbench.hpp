#include "index.hpp"

struct shader_workbench : public GLFWApp
{
    GlCamera cam;
    FlyCameraController flycam;
    shader_workbench();
    ~shader_workbench();
    virtual void on_window_resize(int2 size) override;
    virtual void on_input(const InputEvent & event) override;
    virtual void on_update(const UpdateEvent & e) override;
    virtual void on_draw() override;
};
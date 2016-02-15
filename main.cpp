#include <iostream>
#include <sstream>

#include "index.hpp"

using namespace avl;

//#include "examples/empty_app.hpp"
//#include "examples/arcball_app.hpp"
//#include "examples/ssao_app.hpp"
//#include "examples/skydome_app.hpp"
//#include "examples/terrain_app.hpp"
//#include "examples/camera_app.hpp"
//#include "examples/euclidean_app.hpp" // Borked on windows
//#include "examples/meshline_app.hpp"
//#include "examples/reaction_app.hpp"
//#include "examples/forward_lighting_app.hpp"
//#include "examples/decal_app.hpp"
//#include "examples/vision_app.hpp"
#include "examples/sandbox_app.hpp"

IMPLEMENT_MAIN(int argc, char * argv[])
{
    ExperimentalApp app;
    app.main_loop();
    return 0;
}

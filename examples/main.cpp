
#include <iostream>
#include <sstream>

#include "index.hpp"

using namespace avl;

//#include "examples/empty_app.hpp"
//#include "examples/textured_model_app.hpp"
//#include "examples/ssao_app.hpp"
//#include "examples/camera_app.hpp"
//#include "examples/euclidean_app.hpp" // Borked on windows
//#include "examples/meshline_app.hpp"
//#include "examples/reaction_app.hpp"
//#include "examples/decal_app.hpp"
//#include "examples/vision_app.hpp"
//#include "examples/sandbox_app.hpp"
//#include "examples/hdr_skydome.hpp"
//#include "examples/instance_app.hpp"
//#include "examples/manipulation_app.hpp"
//#include "examples/shadow_app.hpp"
//#include "examples/simplex_noise_app.hpp"
//#include "examples/render_experiments.hpp"
//#include "examples/glass_material_app.hpp"
#include "examples/geometric_algo_dev.hpp"

IMPLEMENT_MAIN(int argc, char * argv[])
{
	ExperimentalApp app;
	app.main_loop();
	return 0;
}
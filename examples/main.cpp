
#include <iostream>
#include <sstream>

#include "index.hpp"
#include "index.hpp"

using namespace avl;

//#include "examples/empty_app.hpp"
//#include "examples/textured_model_app.hpp"
//#include "examples/ssao_app.hpp"
//#include "examples/camera_app.hpp"
//#include "examples/meshline_app.hpp"
//#include "examples/reaction_app.hpp"
//#include "examples/decal_app.hpp"
//#include "examples/vision_app.hpp"
//#include "examples/sandbox_app.hpp"
//#include "examples/hdr_skydome.hpp"
//#include "examples/instance_app.hpp"
//#include "examples/shadow_app.hpp"
//#include "examples/simplex_noise_app.hpp"
//#include "examples/render_experiments.hpp"
//#include "examples/glass_material_app.hpp"
//#include "examples/geometric_algo_dev.hpp"
//#include "examples/lens_dev.hpp"
//#include "examples/asset_test.hpp"
#include "examples/octree_test_app.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb/stb_image.h"

IMPLEMENT_MAIN(int argc, char * argv[])
{
	ExperimentalApp app;
	app.main_loop();
	return 0;
}
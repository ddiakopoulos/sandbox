#include "index.hpp"

using namespace avl;

struct VirtualRealityApp : public GLFWApp
{
	VirtualRealityApp() : GLFWApp(100, 100, "VR") {}
	~VirtualRealityApp() {}
};

int main(int argc, char * argv[])
{
	VirtualRealityApp app;
	app.main_loop();
	return EXIT_SUCCESS;
}
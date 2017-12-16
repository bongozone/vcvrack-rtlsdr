#include "rack.hpp"
#include "dsp/digital.hpp"

using namespace rack;


extern Plugin *plugin;

////////////////////
// module widgets
////////////////////

struct MyModuleWidget : ModuleWidget {
	MyModuleWidget();
};

struct PanAndShiftWidget : ModuleWidget {
	PanAndShiftWidget();
};

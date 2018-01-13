#include "../PQ.hpp"
#include "rtl-sdr.h"

struct SDR : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	RtlSdr radio;

	SDR() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
  }
	void step() override;
};


void SDR::step() {
}

SDRWidget::SDRWidget() {
	SDR *module = new SDR();
	setModule(module);
	box.size = Vec(6 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

  Panel *panel = new Panel();
  panel->box.size = box.size;
  panel->backgroundColor=COLOR_BLACK;
  addChild(panel);

	addChild(createScrew<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	addChild(createScrew<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}

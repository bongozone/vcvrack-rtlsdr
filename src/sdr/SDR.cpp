#include "../PQ.hpp"
#include "rtl-sdr.h"
#include <stdio.h>
#include <limits.h>

struct SDR : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		AUDIO_OUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	RtlSdr radio;
	FILE* file;

	SDR() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
  }
	~SDR() {
		RtlSdr_end(&radio);
	}
	void onSampleRateChange() override;
	void step() override;
	void openFile();
};


void SDR::step() {
	// play -r 32k -t raw -e s -b 16 -c 1 -V1 -
	static int16_t sample = 0;
	int result = fread(&sample, sizeof(sample), 1, file);
	if (result != 1) {
		printf ("Reading error; count was %d\n",result);
		fclose(file);
		openFile();
	}
	float value = 5.0*float(sample)/(float)SHRT_MAX;
	outputs[SDR::AUDIO_OUT].value = value;
}

void SDR::onSampleRateChange() {
	RtlSdr_init(&radio, (int)engineGetSampleRate());
	openFile();
}

void SDR::openFile() {
	char* filename = "/tmp/vcvrack-rtlsdr.raw";
	file = fopen(filename, "r");
	if (!file) {
		fprintf(stderr, "Failed to open %s\n", filename);
		exit(1);
	}
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

	addOutput(createOutput<PJ301MPort>(Vec(50,50), module, SDR::AUDIO_OUT));
}

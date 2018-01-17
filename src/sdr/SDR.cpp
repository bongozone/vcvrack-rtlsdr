#include "../PQ.hpp"
#include "rtl-sdr.h"
#include <stdio.h>
#include <limits.h>

struct SDR : Module {
	enum ParamIds {
		TUNE_PARAM,
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
	FILE* file = NULL;
	long currentFreq;

	SDR() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		radio.filename[0]='\0';
  }
	~SDR() {
		RtlSdr_end(&radio);
	}
	void onSampleRateChange() override;
	void step() override;
	void openFile();
	long getFreq(float);
};


void SDR::step() {
	if(!strlen(radio.filename)) {
		RtlSdr_init(&radio, (int)engineGetSampleRate());
	}
	if(!file) {
		openFile();
	}
	// play -r 32k -t raw -e s -b 16 -c 1 -V1 -
	static int16_t sample = 0;
	int result = fread(&sample, sizeof(sample), 1, file);
	if (result != 1) {
		printf ("Reading error; count was %d\n",result);
		fclose(file);
		openFile();
		return; // hold off doing anything else until data is flowing (for now)
	}
	long freq = getFreq(params[TUNE_PARAM].value);

	if (abs(freq - currentFreq)>10000) {
			RtlSdr_tune(&radio, freq);
			currentFreq = freq;
	}

	float value = 5.0*float(sample)/(float)SHRT_MAX;
	outputs[SDR::AUDIO_OUT].value = value;
}

void SDR::onSampleRateChange() {
}

void SDR::openFile() {
	if(!strlen(radio.filename)) {
		return;
	}
	file = fopen(radio.filename, "r");
	if (!file) {
		fprintf(stderr, "Failed to open %s\n", radio.filename);
		//exit(1);
	}
}

long SDR::getFreq(float knob) {
	return int(knob*10000000);
}

SDRWidget::SDRWidget() {
	SDR *module = new SDR();
	setModule(module);
	box.size = Vec(6 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

  Panel *panel = new DarkPanel();
  panel->box.size = box.size;
  addChild(panel);

	addChild(createScrew<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	addChild(createScrew<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	addParam(createParam<RoundHugeBlackKnob>(Vec(box.size.x/4, 30), module, SDR::TUNE_PARAM, 80.0, 110.0, 90.0));
	addOutput(createOutput<PJ301MPort>(Vec(50,box.size.y-30), module, SDR::AUDIO_OUT));
}

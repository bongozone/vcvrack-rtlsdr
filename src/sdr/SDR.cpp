#include "../PQ.hpp"
#include "rtl-sdr.h"
#include <stdio.h>
#include <limits.h>
#include <cstring>
#include <iomanip> // setprecision
#include <sstream> // stringstream
#include "dsp/ringbuffer.hpp"
#define HZ_CEIL 110.0
#define HZ_FLOOR 80.0
#define HZ_SPAN (HZ_CEIL-HZ_FLOOR)
#define HZ_CENTER (HZ_FLOOR+0.5*HZ_SPAN)
#define MAX_VOLTAGE 5.0

// cribbed from JW-modules - remove later
struct CenteredLabel : Widget {
	std::string text;
	int fontSize;
	CenteredLabel(int _fontSize = 18) {
		fontSize = _fontSize;
	}
	void draw(NVGcontext *vg) override {
		nvgTextAlign(vg, NVG_ALIGN_CENTER);
		nvgFillColor(vg, nvgRGB(252,120,111));
		nvgFontSize(vg, fontSize);
		nvgText(vg, box.pos.x, box.pos.y, text.c_str(), NULL);
	}
};

struct SDR : Module {
	enum ParamIds {
		TUNE_PARAM,
		TUNE_ATT,
		NUM_PARAMS
	};
	enum InputIds {
		TUNE_INPUT,
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
	rack::RingBuffer<int16_t, 16384> buffer;
	long currentFreq;

	SDR() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		buffer.clear();
		RtlSdr_init(&radio, (int)engineGetSampleRate());

  }
	~SDR() {
		RtlSdr_end(&radio);
	}
	void onSampleRateChange() override;
	void step() override;
	void openFile();
	long getFreq(float);
	CenteredLabel* linkedLabel;
};

void SDR::step() {

	if(buffer.size() < 10 ) { // This seems reasonable
		//printf("📻 ring buffer is getting low (%ld), try mutex\n", buffer.size());
		int error = pthread_mutex_trylock(radio.rack_mutex);
		if (error != 0) {
			if(error==EBUSY) {
				printf("📻 mutex busy\n");
			} else {
				printf("📻 mutex error\n");
			}
		} else {
			if(*(radio.rack_buffer_pos) != 0) {
				for(int i = 0; i < *(radio.rack_buffer_pos); i++) {
					if(buffer.full()) {
						printf("📻 sdr buffer overrun\n");
						break;
					}
					buffer.push(radio.rack_buffer[i]);
				}
				//printf("📻 ring buffer consumed %ld, size is now %d\n", *(radio.rack_buffer_pos), buffer.size());
				*(radio.rack_buffer_pos) = 0;
			}
			pthread_mutex_unlock(radio.rack_mutex);
		}
	}

	float freq = params[TUNE_PARAM].value;
	float freqOff = params[TUNE_ATT].value*inputs[TUNE_INPUT].value/MAX_VOLTAGE;
	float freqComputed = freq + freqOff;
	long longFreq = getFreq(freqComputed) ; // lots of zeros

	if (longFreq - currentFreq) {
			RtlSdr_tune(&radio, longFreq);
			currentFreq = longFreq;
			std::stringstream stream;
			stream << std::fixed << std::setprecision(3) << freqComputed;
			linkedLabel->text = stream.str()+ "M";
	}

	if(!buffer.empty()) {
		int16_t sample = buffer.shift();
		float value = MAX_VOLTAGE*float(sample)/(float)SHRT_MAX;
		outputs[SDR::AUDIO_OUT].value 	= value;
	} else {
		//printf("📻 awaiting buffer\n");
	}
}

void SDR::onSampleRateChange() {
}

long SDR::getFreq(float knob) {
	return int(knob*1000000); // float quantities are in millions so this is a million
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

	{
		CenteredLabel* const freqLabel = new CenteredLabel;
		freqLabel->box.pos = Vec(20, 20);
		freqLabel->text = "0";
		module->linkedLabel  = freqLabel;
		addChild(freqLabel);
	}

	addParam(createParam<RoundHugeBlackKnob>(Vec(box.size.x/4, 100), module, SDR::TUNE_PARAM, HZ_FLOOR, HZ_CEIL, HZ_CENTER));
	addParam(createParam<RoundSmallBlackKnob>(Vec(box.size.x/4, 170), module, SDR::TUNE_ATT, -HZ_SPAN/2.0, +HZ_SPAN/2.0, 0.0));
	addInput(createInput<PJ301MPort>(Vec(box.size.x/4, 200), module, SDR::TUNE_INPUT));
	addOutput(createOutput<PJ301MPort>(Vec(50,box.size.y-50), module, SDR::AUDIO_OUT));
}

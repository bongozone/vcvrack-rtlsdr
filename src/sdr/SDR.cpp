#include "../PQ.hpp"
#include "rtl-sdr.h"
#include <stdio.h>
#include <limits.h>
#include <cstring>
#include <iostream>
#include <iomanip> // setprecision
#include <sstream> // stringstream
#define HZ_CEIL 110.0
#define HZ_FLOOR 80.0
#define HZ_SPAN (HZ_CEIL-HZ_FLOOR)
#define HZ_CENTER (HZ_FLOOR+0.5*HZ_SPAN)
#define MAX_VOLTAGE 5.0

struct MyLabel : Widget {
	std::string text;
	int fontSize;
	NVGcolor color = nvgRGB(255,20,20);
	MyLabel(int _fontSize = 18) {
		fontSize = _fontSize;
	}
	void draw(const DrawArgs& args) override {
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER|NVG_ALIGN_BASELINE);
		nvgFillColor(args.vg, color);
		nvgFontSize(args.vg, fontSize);
		nvgText(args.vg, box.pos.x, box.pos.y, text.c_str(), NULL);
	}
};

struct SDR : Module {
	enum ParamIds {
		TUNE_PARAM,
		TUNE_ATT,
		QUANT_PARAM,
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
	rack::dsp::RingBuffer<int16_t, 16384> buffer;
	long currentFreq;

	SDR() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		buffer.clear();
		RtlSdr_init(&radio, (int)APP->engine->getSampleRate());
    configParam(SDR::TUNE_PARAM, HZ_FLOOR, HZ_CEIL, HZ_CENTER, "");
    configParam(SDR::TUNE_ATT, -HZ_SPAN/2.0, +HZ_SPAN/2.0, 0.0, "");
    configParam(SDR::QUANT_PARAM, 0.0, 2.0, 0.0, "");
  }
	~SDR() {
		RtlSdr_end(&radio);
	}
	void onSampleRateChange() override;
	void process(const ProcessArgs& args) override;
	void openFile();
	long getFreq(float);
	float getMegaFreq(long);
	MyLabel* linkedLabel;
	long stepCount;
};
void SDR::process(const ProcessArgs& args) {

	if (radio.rack_buffer==NULL && stepCount++ % 100000 == 0) {
		RtlSdr_init(&radio, (int)args.sampleRate);
		return;
	}

	if (radio.rack_buffer==NULL) {
		return;
	}

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

	float freq = params[TUNE_PARAM].getValue();
	float freqOff = params[TUNE_ATT].getValue()*inputs[TUNE_INPUT].getVoltage()/MAX_VOLTAGE;
	float freqComputed = freq + freqOff;
	long longFreq = getFreq(freqComputed) ; // lots of zeros

	enum Quantization {HUNDREDK, TENK, NONE};
  Quantization scale = static_cast<Quantization>(roundf(params[QUANT_PARAM].getValue()));
	long modulo;
	switch(scale) {
		case HUNDREDK:
			modulo = 100000;
			break;
		case TENK:
			modulo = 10000;
			break;
		case NONE:
			modulo = 1;
	}
	long modulus = longFreq%modulo;
	longFreq -= modulus;
	if(modulus>=modulo/2) {
		longFreq+=modulo;
	}


	if (longFreq - currentFreq) {
			RtlSdr_tune(&radio, longFreq);
			currentFreq = longFreq;
			std::stringstream stream;
			stream << std::fixed << std::setprecision(3) << std::setw(7) << getMegaFreq(longFreq);
			std::cout << std::setw(7) << std::fixed << std::setprecision(3) << getMegaFreq(longFreq) << "\n";
			linkedLabel->text = stream.str();
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
	return int(knob*1000000.f); // float quantities are in millions so this is a million
}

float SDR::getMegaFreq(long longFreq) {
	return float(longFreq)/ 1000000.f; // float quantities are in millions so this is a million
}

struct SDRWidget : ModuleWidget {
	SDRWidget(SDR *module);
};

SDRWidget::SDRWidget(SDR *module) : ModuleWidget(module) {

	box.size = Vec(4 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

  // Panel *panel = new LightPanel();
  SVGPanel *panel = new SVGPanel();
  panel->box.size = box.size;
  panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/sdr.svg")));
  addChild(panel);

	addChild(createWidget<ScrewSilver>(Vec(0, 0)));
	addChild(createWidget<ScrewSilver>(Vec(0, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	{
		MyLabel* const freqLabel = new MyLabel;
		freqLabel->box.pos = Vec(box.size.x/4,RACK_GRID_WIDTH*3);  // coordinate system is broken FIXME
		freqLabel->text = "0";
    if(module)
      module->linkedLabel = freqLabel;
		addChild(freqLabel);
	}

	{
		MyLabel* const cLabel = new MyLabel(10);
		cLabel->box.pos = Vec(19,5);  // coordinate system is broken FIXME
		cLabel->color = nvgRGB(0,0,0);
		cLabel->text = "rtl-sdr FM";
		addChild(cLabel);
	}

	{
		MyLabel* const cLabel = new MyLabel(9);
		cLabel->box.pos = Vec(14.5,(RACK_GRID_HEIGHT - RACK_GRID_WIDTH/1.5)/2); // coordinate system is broken FIXME
		cLabel->color = nvgRGB(0,0,0);
		cLabel->text = "pulsum";
		addChild(cLabel);
	}
	{
		MyLabel* const cLabel = new MyLabel(9);
		cLabel->box.pos = Vec(18,(RACK_GRID_HEIGHT - RACK_GRID_WIDTH/5)/2); // coordinate system is broken FIXME
		cLabel->color = nvgRGB(0,0,0);
		cLabel->text = "quadratum";
		addChild(cLabel);
	}

  SVGKnob *knob = dynamic_cast<SVGKnob*>(createParam<RoundHugeBlackKnob>(Vec(RACK_GRID_WIDTH/6, 100), module, SDR::TUNE_PARAM));
	knob->maxAngle += 10*2*M_PI;
	knob->speed /= 20.f;
	addParam(knob);
	addParam(createParam<RoundSmallBlackKnob>(Vec(RACK_GRID_WIDTH, 170), module, SDR::TUNE_ATT));
	addInput(createInput<PJ301MPort>(Vec(RACK_GRID_WIDTH, 200), module, SDR::TUNE_INPUT));
	addParam(createParam<CKSSThree>(Vec(RACK_GRID_WIDTH/2, 240), module, SDR::QUANT_PARAM));
	{
		MyLabel* const cLabel = new MyLabel(12);
		cLabel->box.pos = Vec(16,236/2); // coordinate system is broken FIXME
		cLabel->color = nvgRGB(0,0,0);
		cLabel->text = "Stepping";
		addChild(cLabel);
	}
	{
		MyLabel* const cLabel = new MyLabel(12);
		cLabel->box.pos = Vec(20,240/2+4); // coordinate system is broken FIXME
		cLabel->color = nvgRGB(0,0,0);
		cLabel->text = "none ";
		addChild(cLabel);
	}
	{
		MyLabel* const cLabel = new MyLabel(12);
		cLabel->box.pos = Vec(20,240/2+9); // coordinate system is broken FIXME
		cLabel->color = nvgRGB(0,0,0);
		cLabel->text = "10 k ";
		addChild(cLabel);
	}
	{
		MyLabel* const cLabel = new MyLabel(12);
		cLabel->box.pos = Vec(20,240/2+14); // coordinate system is broken FIXME
		cLabel->color = nvgRGB(0,0,0);
		cLabel->text = "100k";
		addChild(cLabel);
	}


	addOutput(createOutput<PJ301MPort>(Vec(RACK_GRID_WIDTH, box.size.y-3*RACK_GRID_WIDTH), module, SDR::AUDIO_OUT));
}


Model *sdrModule = createModel<SDR, SDRWidget>("SDRWidget");

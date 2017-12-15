#include "Tutorial.hpp"

// I know this is lame http://blog.natekohl.net/making-countof-suck-less/
#define COUNTOF(arr) (sizeof(arr) / sizeof(arr[0]))


struct MyModule : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		CV_INPUT,
    TRIGGER_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CV1_OUTPUT,
		CV2_OUTPUT,
		CV3_OUTPUT,
		CV4_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
    TRIGGER_LIGHT,
		LIGHT1,
		LIGHT2,
		LIGHT3,
		LIGHT4,
		NUM_LIGHTS
	};

	float phase = 0.0;
	float blinkPhase = 0.0;
	float triggerPhase = 0.0;
  SchmittTrigger trigger;
  float inputValue = 0.0;
  float shift[4] = {0.0, 0.0, 0.0, 0.0};
  int shiftPosition = -1;

	MyModule() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
    trigger.setThresholds(0.0, 2.0);
  }
	void step() override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - reset, randomize: implements special behavior when user clicks these from the context menu
};


void MyModule::step() {
	// Implement a simple sine oscillator
	float deltaTime = 1.0 / engineGetSampleRate();

	// Compute the sine output
	float sine = sinf(2 * M_PI * phase);
	outputs[CV1_OUTPUT].value = 5.0 * sine;

	if (trigger.process(inputs[TRIGGER_INPUT].value)) {
    triggerPhase = 0.0;
    shiftPosition++;
    shiftPosition%=COUNTOF(shift);
    shift[shiftPosition] = inputValue;
    printf("shiftPosition=%d last is=%d value is = %f size is %d\n", shiftPosition,(shiftPosition+3)%(int)COUNTOF(shift),shift[shiftPosition+3%COUNTOF(shift)],(int)COUNTOF(shift));
	}

  inputValue = inputs[CV_INPUT].value;

  lights[TRIGGER_LIGHT].value = (triggerPhase < 0.25) ? 1.0 : 0.0; // trigger led is 250ms
  triggerPhase += deltaTime;

	outputs[CV1_OUTPUT].value = shift[(shiftPosition+0)%COUNTOF(shift)];
	outputs[CV2_OUTPUT].value = shift[(shiftPosition+1)%COUNTOF(shift)];
	outputs[CV3_OUTPUT].value = shift[(shiftPosition+2)%COUNTOF(shift)];
	outputs[CV4_OUTPUT].value = shift[(shiftPosition+3)%COUNTOF(shift)];

  lights[LIGHT1].value = outputs[CV1_OUTPUT].value/10.0;
  lights[LIGHT2].value = outputs[CV2_OUTPUT].value/10.0;
  lights[LIGHT3].value = outputs[CV3_OUTPUT].value/10.0;
  lights[LIGHT4].value = outputs[CV4_OUTPUT].value/10.0;
}

MyModuleWidget::MyModuleWidget() {
	MyModule *module = new MyModule();
	setModule(module);
	box.size = Vec(6 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/MyModule.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	addInput(createInput<PJ301MPort>(Vec(33, 140), module, MyModule::TRIGGER_INPUT));
	addInput(createInput<PJ301MPort>(Vec(33, 186), module, MyModule::CV_INPUT));

	addChild(createLight<MediumLight<RedLight>>(Vec(66, 140), module, MyModule::TRIGGER_LIGHT));

	addOutput(createOutput<PJ301MPort>(Vec(33, 250), module, MyModule::CV1_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(33, 275), module, MyModule::CV2_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(33, 300), module, MyModule::CV3_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(33, 325), module, MyModule::CV4_OUTPUT));

  // TODO make lights follow pitch?
	addChild(createLight<MediumLight<RedLight>>(Vec(66, 250), module, MyModule::LIGHT1));
	addChild(createLight<MediumLight<RedLight>>(Vec(66, 275), module, MyModule::LIGHT2));
	addChild(createLight<MediumLight<RedLight>>(Vec(66, 300), module, MyModule::LIGHT3));
	addChild(createLight<MediumLight<RedLight>>(Vec(66, 325), module, MyModule::LIGHT4));

}

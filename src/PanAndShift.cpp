#include "Tutorial.hpp"

// I know this is lame http://blog.natekohl.net/making-countof-suck-less/
#define COUNTOF(arr) (sizeof(arr) / sizeof(arr[0]))

// TODO:
// channel leds turn green when they get a new note and (generation color) when they get an old note
// generation colors
// add multiple ranges for BUFF_SIZE for audio rate stuff
// add log scale for buff size
// add option to make knob scale by powers of two only?
// figure out resize behavior

#define LANES 4
#define STRIPE_COUNT 16
#define BUFF_SIZE 16
#define LED_TIME 0.1

struct PanAndShift : Module {
	enum ParamIds {
    CENTER,
    WIDTH,
    CENTER_ATT,
    WIDTH_ATT,
    LANE_LENGTH,
		NUM_PARAMS = LANE_LENGTH + LANES
	};
	enum InputIds {
		CV_INPUT,
    TRIGGER_INPUT,
    WIDTH_INPUT,
    CENTER_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
    LANE_OUTPUT,
		NUM_OUTPUTS = LANE_OUTPUT + LANES
	};
	enum LightIds {
    TRIGGER_LIGHT,
    LENGTH_LIGHTS,
    STATE_LIGHTS = LENGTH_LIGHTS+LANES*STRIPE_COUNT,
    PAN_LIGHTS = STATE_LIGHTS+LANES*STRIPE_COUNT,
		NUM_LIGHTS = PAN_LIGHTS + LANES
	};

	float triggerPhase = 0.0;
  SchmittTrigger trigger;
  float inputValue = 0.0;
  long shiftPositions[LANES];
  float shift[LANES][BUFF_SIZE];

  long laneLengths[LANES];
  bool laneActive[LANES];

	PanAndShift() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
    trigger.setThresholds(0.0, 2.0);
    for(int lane = 0; lane < LANES; lane++) {
      shiftPositions[lane] = 0;
      laneLengths[lane] = 1;
    }
  }
	void step() override;

};


void PanAndShift::step() {
	// Implement a simple sine oscillator
	float deltaTime = 1.0 / engineGetSampleRate();

  for(int lane = 0; lane < LANES; lane++) {
    laneLengths[lane] = 1+long(params[PanAndShift::LANE_LENGTH + lane].value*(BUFF_SIZE-1));
    for(int stripe = 0; stripe < STRIPE_COUNT ; stripe++) {
      int ceiling = stripe*BUFF_SIZE/STRIPE_COUNT + 1;
      lights[LENGTH_LIGHTS + lane*STRIPE_COUNT + stripe ].value = laneLengths[lane]>=ceiling;
    }
  }

  bool debug = false;
  bool triggered = false;
	if (trigger.process(inputs[TRIGGER_INPUT].value)) {
    triggerPhase = 0.0;
    //printf("shiftPosition=%d last is=%d value is = %f size is %d\n", shiftPosition,(shiftPosition+3)%(int)COUNTOF(shift),shift[shiftPosition+3%COUNTOF(shift)],(int)COUNTOF(shift));
    debug = true;
    triggered = true;
	}

  inputValue = inputs[CV_INPUT].value;

  lights[TRIGGER_LIGHT].value = (triggerPhase < LED_TIME) ? 1.0 : 0.0;
  triggerPhase += deltaTime;

  float center = params[CENTER].value + params[CENTER_ATT].value * inputs[CENTER_INPUT].value;
  float width = params[WIDTH].value + params[WIDTH_ATT].value * inputs[WIDTH_INPUT].value;
  for(int lane = 0; lane < LANES; lane++) {
    float laneVoltage = 2.5*lane-5.0;
    laneActive[lane] = abs(center - laneVoltage) < width;
    lights[PAN_LIGHTS + lane].value = laneActive[lane];
  }

  for(int lane = 0; lane < LANES; lane++) {
    outputs[LANE_OUTPUT+lane].value = shift[lane][shiftPositions[lane]];
    if (/*laneActive[lane]==true &&*/ triggered) {
      shift[lane][shiftPositions[lane]] = laneActive[lane]==true ? inputValue : outputs[LANE_OUTPUT+lane].value;
      shiftPositions[lane]%=laneLengths[lane];
      shiftPositions[lane]++;
    }
  }
}

PanAndShiftWidget::PanAndShiftWidget() {
	PanAndShift *module = new PanAndShift();
	setModule(module);
	box.size = Vec(20 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

  Panel *panel = new LightPanel();
  panel->box.size = box.size;
  addChild(panel);

	addChild(createScrew<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	addChild(createScrew<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	addInput(createInput<PJ301MPort>(Vec(20, 20), module, PanAndShift::TRIGGER_INPUT));
	addInput(createInput<PJ301MPort>(Vec(20, 50), module, PanAndShift::CV_INPUT));

	addChild(createLight<LargeLight<RedLight>>(Vec(50, 28), module, PanAndShift::TRIGGER_LIGHT));

  addParam(createParam<RoundHugeBlackKnob>(Vec(80, 20), module, PanAndShift::CENTER, -5.1, 5.1, 0.0));
	addInput(createInput<PJ301MPort>(Vec(140, 50), module, PanAndShift::CENTER_INPUT));
  addParam(createParam<RoundSmallBlackKnob>(Vec(140, 20), module, PanAndShift::CENTER_ATT, -1.0, 1.0, 0.0));

  addParam(createParam<RoundHugeBlackKnob>(Vec(200, 20), module, PanAndShift::WIDTH, 0.0, 5.1, 0.0));
	addInput(createInput<PJ301MPort>(Vec(260, 50), module, PanAndShift::WIDTH_INPUT));
  addParam(createParam<RoundSmallBlackKnob>(Vec(260, 20), module, PanAndShift::WIDTH_ATT, -1.0, 1.0, 0.0));

  int laneHead = 150;
  for(int lane = 0; lane < LANES; lane++) {
    int xOffset = 10+75*lane;
    int x2Offset = 30+70*lane;
    addChild(createLight<MediumLight<RedLight>>(Vec(xOffset, laneHead), module, PanAndShift::PAN_LIGHTS + lane));
    addParam(createParam<RoundLargeBlackKnob>(Vec(xOffset+10, laneHead), module, PanAndShift::LANE_LENGTH + lane, 0.0, 1.0, 0.0));

    for(int stripe = 0; stripe < STRIPE_COUNT; stripe++) {
      addChild(createLight<TinyLight<GreenLight>>(Vec(xOffset+20, laneHead + 50 + stripe*8), module, PanAndShift::LENGTH_LIGHTS + (STRIPE_COUNT * lane) + stripe));
      addChild(createLight<TinyLight<BlueLight>>(Vec(xOffset+40, laneHead + 50 + stripe*8), module, PanAndShift::STATE_LIGHTS + (STRIPE_COUNT * lane) + stripe));
    }

    addOutput(createOutput<PJ301MPort>(Vec(x2Offset, laneHead+200), module, PanAndShift::LANE_OUTPUT + lane));
  }
}

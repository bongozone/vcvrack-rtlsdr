#include "Tutorial.hpp"

// TODO:
// generation color strip
// channel leds turn green when they get a new note and (generation color) when they get an old note
// add option to make length scale by: n, n^2, 2^n
// figure out resize behavior
// empty state on init?
// Add trigger input, normalled to step input with led
// smooth leds
// add channel trigger and step outs
// blacken active step on runway
// add pan and scan for active channels

#define LANES 4
#define STRIPE_COUNT 16
#define BUFF_SIZE 16
#define LED_TIME 0.1
#define GENERATION_COUNT 1

struct PanAndShift : Module {
  struct State {
    float value;
    NVGcolor color;
  };
	enum ParamIds {
    CENTER,
    WIDTH = CENTER+2,
    CENTER_ATT = WIDTH+2,
    WIDTH_ATT = CENTER_ATT+2,
    LANE_LENGTH,
		NUM_PARAMS = LANE_LENGTH + LANES
	};
	enum InputIds {
		CV_INPUT,
    STEP_INPUT,
    WIDTH_INPUT,
    CENTER_INPUT = WIDTH_INPUT+2,
		NUM_INPUTS
	};
	enum OutputIds {
    LANE_OUTPUT,
		NUM_OUTPUTS = LANE_OUTPUT + LANES
	};
	enum LightIds {
    STEP_LIGHT,
    LENGTH_LIGHTS,
    PAN_LIGHTS = LENGTH_LIGHTS+LANES*STRIPE_COUNT,
		NUM_LIGHTS = PAN_LIGHTS + LANES
	};

	float stepPhase = 0.0;
  SchmittTrigger stepTrigger;
  float inputValue = 0.0;
  long shiftPositions[LANES];
  State shift[LANES][BUFF_SIZE];

  long laneLengths[LANES];
  bool laneActive[LANES];
  long stepCount;

	PanAndShift() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
    stepTrigger.setThresholds(0.0, 2.0);
    for(int lane = 0; lane < LANES; lane++) {
      shiftPositions[lane] = 0;
      laneLengths[lane] = 1;
    }
  }
	void step() override;
	float wheel();


};


float PanAndShift::wheel() {
  float w = stepCount%(GENERATION_COUNT*BUFF_SIZE)/double(GENERATION_COUNT*BUFF_SIZE);
  return w;
}

void PanAndShift::step() {
	float deltaTime = 1.0 / engineGetSampleRate();

  bool debug = false;
  bool stepTriggered = false;
	if (stepTrigger.process(inputs[STEP_INPUT].value)) {
    stepPhase = 0.0;
    debug = true;
    stepTriggered = true;
    stepCount++;
	}


  for(int lane = 0; lane < LANES; lane++) {
    laneLengths[lane] = 1+long(params[PanAndShift::LANE_LENGTH + lane].value*(BUFF_SIZE-1));
    for(int stripe = 0; stripe < STRIPE_COUNT ; stripe++) {
      int ceiling = stripe*BUFF_SIZE/STRIPE_COUNT + 1;
      bool isActive = stripe == STRIPE_COUNT*shiftPositions[lane]/BUFF_SIZE && (stepPhase < LED_TIME);
      lights[LENGTH_LIGHTS + lane*STRIPE_COUNT + stripe ].value = laneLengths[lane]>=ceiling && !isActive;
    }
  }

  inputValue = inputs[CV_INPUT].value;

  lights[STEP_LIGHT].value = (stepPhase < LED_TIME) ? 1.0 : 0.0;
  stepPhase += deltaTime;

  /* begin pan and scan */
  float center = params[CENTER].value + params[CENTER_ATT].value * inputs[CENTER_INPUT].value;
  float width = params[WIDTH].value + params[WIDTH_ATT].value * inputs[WIDTH_INPUT].value;
  for(int lane = 0; lane < LANES; lane++) {
    float laneVoltage = 2.5*lane-5.0;
    laneActive[lane] = abs(center - laneVoltage) < width;
    lights[PAN_LIGHTS + lane].value = laneActive[lane];
  }
  /* end pan and scan */

  for(int lane = 0; lane < LANES; lane++) {
    outputs[LANE_OUTPUT+lane].value = shift[lane][shiftPositions[lane]].value;
    if (/*laneActive[lane]==true &&*/ stepTriggered) {
      shift[lane][shiftPositions[lane]].value = laneActive[lane]==true ? inputValue : outputs[LANE_OUTPUT+lane].value;
      if(laneActive[lane]==true) {
        shift[lane][shiftPositions[lane]].color = nvgHSL(wheel(), 1.0, 0.4);
      }
      shiftPositions[lane]%=laneLengths[lane];
      shiftPositions[lane]++;
    }
  }

}

PanAndShiftWidget::PanAndShiftWidget() {
	PanAndShift *module = new PanAndShift();
	setModule(module);
	box.size = Vec(20 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

  Panel *panel = new DarkPanel();
  panel->box.size = box.size;
  addChild(panel);

	addChild(createScrew<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	addChild(createScrew<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	addInput(createInput<PJ301MPort>(Vec(20, 20), module, PanAndShift::STEP_INPUT));
	addInput(createInput<PJ301MPort>(Vec(20, 50), module, PanAndShift::CV_INPUT));

	addChild(createLight<LargeLight<RedLight>>(Vec(50, 28), module, PanAndShift::STEP_LIGHT));

  // Begin top pan-and-scan
  int panAndScanOriginY = 20;
  addParam(createParam<RoundHugeBlackKnob>(Vec(80, panAndScanOriginY), module, PanAndShift::CENTER, -5.1, 5.1, 0.0));
	addInput(createInput<PJ301MPort>(Vec(140, panAndScanOriginY+30), module, PanAndShift::CENTER_INPUT));
  addParam(createParam<RoundSmallBlackKnob>(Vec(140, panAndScanOriginY), module, PanAndShift::CENTER_ATT, -1.0, 1.0, 0.0));

  addParam(createParam<RoundHugeBlackKnob>(Vec(200, panAndScanOriginY), module, PanAndShift::WIDTH, 0.0, 5.1, 0.0));
	addInput(createInput<PJ301MPort>(Vec(260, panAndScanOriginY+30), module, PanAndShift::WIDTH_INPUT));
  addParam(createParam<RoundSmallBlackKnob>(Vec(260, panAndScanOriginY), module, PanAndShift::WIDTH_ATT, -1.0, 1.0, 0.0));
  // End top pan-and-scan

  // Begin bottom pan-and-scan
  panAndScanOriginY+=60;
  addParam(createParam<RoundHugeBlackKnob>(Vec(80, panAndScanOriginY), module, PanAndShift::CENTER+1, -5.1, 5.1, 0.0));
	addInput(createInput<PJ301MPort>(Vec(140, panAndScanOriginY+30), module, PanAndShift::CENTER_INPUT+1));
  addParam(createParam<RoundSmallBlackKnob>(Vec(140, panAndScanOriginY), module, PanAndShift::CENTER_ATT+1, -1.0, 1.0, 0.0));

  addParam(createParam<RoundHugeBlackKnob>(Vec(200, panAndScanOriginY), module, PanAndShift::WIDTH+1, 0.0, 5.1, 0.0));
	addInput(createInput<PJ301MPort>(Vec(260, panAndScanOriginY+30), module, PanAndShift::WIDTH_INPUT+1));
  addParam(createParam<RoundSmallBlackKnob>(Vec(260, panAndScanOriginY), module, PanAndShift::WIDTH_ATT+1, -1.0, 1.0, 0.0));
  // End bottom pan-and-scan

  int laneHead = 150;
  for(int lane = 0; lane < LANES; lane++) {
    int xOffset = 10+75*lane;
    int x2Offset = 30+70*lane;
    addChild(createLight<MediumLight<RedLight>>(Vec(xOffset, laneHead), module, PanAndShift::PAN_LIGHTS + lane));
    addParam(createParam<RoundLargeBlackKnob>(Vec(xOffset+10, laneHead), module, PanAndShift::LANE_LENGTH + lane, 0.0, 1.0, 0.0));

    for(int stripe = 0; stripe < STRIPE_COUNT; stripe++) {
      addChild(createLight<TinyLight<GreenLight>>(Vec(xOffset+20, laneHead + 50 + stripe*8), module, PanAndShift::LENGTH_LIGHTS + (STRIPE_COUNT * lane) + stripe));
    }

    addOutput(createOutput<PJ301MPort>(Vec(x2Offset, laneHead+200), module, PanAndShift::LANE_OUTPUT + lane));
  }
}

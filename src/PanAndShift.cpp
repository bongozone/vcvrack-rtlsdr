#include "Tutorial.hpp"

// TODO:
// generation color strip
// channel leds turn green when they get a new note and (generation color) when they get an old note
// add option to make length scale by: n, n^2, 2^n, fibbonacci
// figure out resize behavior
// empty state on init?
// Add trigger input, normalled to clock input with led
// smooth leds
// add channel trigger and clock outs
// blacken active step on runway
// add pan and scan for active channels
// untriggered steps don't change the CV? [trigger mode]
// trigger / CV mode toggle for channel 2
// continue sampling CV as long as CLOCK is high?
// CV values for right channel don't make sense when scale is non linear
// trigger mode needs to only trigger for a short pulse
// bugs in right hand lane display

#define LANES 4
#define STRIPE_COUNT 16
#define BUFF_SIZE 16
#define LED_TIME 0.1
#define GENERATION_COUNT 1

struct PanAndShift : Module {
  struct State {
    float value;
    float value2; // generally gate!
    NVGcolor color; // generation color? CV-color?
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
    CLOCK_INPUT,
    TRIGGER_INPUT,
    WIDTH_INPUT,
    CENTER_INPUT = WIDTH_INPUT+2,
		NUM_INPUTS
	};
	enum OutputIds {
    LANE_OUTPUT,
    LANE_TRIGGER_OUTPUT = LANE_OUTPUT + LANES,
		NUM_OUTPUTS = LANE_TRIGGER_OUTPUT + LANES
	};
	enum LightIds {
    CLOCK_LIGHT,
    TRIGGER_LIGHT,
    LENGTH_LIGHTS,
    GATE_LIGHTS = LENGTH_LIGHTS+LANES*STRIPE_COUNT,
    PAN_IN_LIGHTS = GATE_LIGHTS+LANES*STRIPE_COUNT,
    PAN_OUT_LIGHTS = PAN_IN_LIGHTS + LANES,
		NUM_LIGHTS = PAN_OUT_LIGHTS + LANES
	};

	float clockPhase = 0.0;
  SchmittTrigger clockTrigger;
  float inputValue = 0.0;
  float inputValue2 = 0.0;
  long shiftPositions[LANES];
  float almostOuts[LANES];
  float almostOuts2[LANES];
  float outs[LANES];
  float outs2[LANES];
  State shift[LANES][BUFF_SIZE];

  long laneLengths[LANES];
  bool laneInputActive[LANES];
  bool laneOutputActive[LANES];
  long clockCount;

	PanAndShift() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
    clockTrigger.setThresholds(0.0, 2.0);
    for(int lane = 0; lane < LANES; lane++) {
      shiftPositions[lane] = 0;
      laneLengths[lane] = 1;
    }
  }
	void step() override;
	float wheel();

  void panAndScan(bool lanes[], float center, float width);
};


float PanAndShift::wheel() {
  float w = clockCount%(GENERATION_COUNT*BUFF_SIZE)/double(GENERATION_COUNT*BUFF_SIZE);
  return w;
}

void PanAndShift::panAndScan(bool lanes[], float center, float width) {
  for(int lane = 0; lane < LANES; lane++) {
    float laneVoltage = 2.5*lane-5.0;
    lanes[lane] = abs(center - laneVoltage) < width;
  }
}

void PanAndShift::step() {
	float deltaTime = 1.0 / engineGetSampleRate();

  bool debug = false;
  bool clockTriggered = false;
	if (clockTrigger.process(inputs[CLOCK_INPUT].value)) {
    clockPhase = 0.0;
    debug = true;
    clockTriggered = true;
    clockCount++;
	}

  // lane length stripes + memory activity
  for(int lane = 0; lane < LANES; lane++) {
    laneLengths[lane] = 1+long(params[PanAndShift::LANE_LENGTH + lane].value*(BUFF_SIZE-1));
    for(int stripe = 0; stripe < STRIPE_COUNT ; stripe++) {
      int ceiling = stripe*BUFF_SIZE/STRIPE_COUNT + 1;
      bool isActive = stripe == STRIPE_COUNT*shiftPositions[lane]/BUFF_SIZE && (clockPhase < LED_TIME);
      lights[LENGTH_LIGHTS + lane*STRIPE_COUNT + stripe ].value = laneLengths[lane]>=ceiling && !isActive;
      lights[GATE_LIGHTS + lane*STRIPE_COUNT + stripe ].value = shift[lane][stripe*(BUFF_SIZE/STRIPE_COUNT)].value2 * (laneLengths[lane]>=ceiling);
    }
  }

  inputValue = inputs[CV_INPUT].value;
  inputValue2 = inputs[TRIGGER_INPUT].normalize(inputs[CLOCK_INPUT].value); // TODO: needs to be a trigger in gate mode

  lights[CLOCK_LIGHT].value = (clockPhase < LED_TIME) ? 1.0 : 0.0;
  // when second channel is in CV mode maybe show polarity / otherwise use gate channel SchmittTrigger value? This should also show the sampled value rather than the current value
  lights[TRIGGER_LIGHT].value = (clockPhase < LED_TIME) ? inputValue2 : 0.0;
  clockPhase += deltaTime;

  /* begin pan and scan */
  float center, width;
  center = params[CENTER].value + params[CENTER_ATT].value * inputs[CENTER_INPUT].value;
  width = params[WIDTH].value + params[WIDTH_ATT].value * inputs[WIDTH_INPUT].value;
  panAndScan(laneInputActive, center, width);
  center = params[CENTER+1].value + params[CENTER_ATT+1].value * inputs[CENTER_INPUT+1].value;
  width = params[WIDTH+1].value + params[WIDTH_ATT+1].value * inputs[WIDTH_INPUT+1].value;
  panAndScan(laneOutputActive, center, width);
  for(int lane = 0; lane < LANES; lane++) {
    lights[PAN_IN_LIGHTS + lane].value = laneInputActive[lane];
    lights[PAN_OUT_LIGHTS + lane].value = laneOutputActive[lane];
  }
  /* end pan and scan */

  // sample CV and step clock
  for(int lane = 0; lane < LANES; lane++) {
    almostOuts[lane] = shift[lane][shiftPositions[lane]].value;
    almostOuts2[lane] = shift[lane][shiftPositions[lane]].value2;
    if (clockTriggered) {
      if(laneInputActive[lane]==true) {
        shift[lane][shiftPositions[lane]].value = inputValue;
        shift[lane][shiftPositions[lane]].value2 = inputValue2;
        shift[lane][shiftPositions[lane]].color = nvgHSL(wheel(), 1.0, 0.4);
      } else {
        // FIXME preserve generation color
        shift[lane][shiftPositions[lane]].value = almostOuts[lane]; // FIXME: when we use pan and scan for outputs, this won't work
        shift[lane][shiftPositions[lane]].value2 = almostOuts2[lane]; // FIXME: when we use pan and scan for outputs, this won't work
      }
      shiftPositions[lane]%=laneLengths[lane];
      shiftPositions[lane]++;
    }
  }

  // final I/O
  for(int lane = 0; lane < LANES; lane++) {
    if(laneOutputActive[lane]) {
      outs[lane] = almostOuts[lane];
      outs2[lane] = almostOuts2[lane];
    }
    outputs[LANE_OUTPUT+lane].value = outs[lane];
    outputs[LANE_TRIGGER_OUTPUT+lane].value = outs2[lane];
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

	addChild(createLight<LargeLight<GreenLight>>(Vec(24, 28), module, PanAndShift::CLOCK_LIGHT));
	addChild(createLight<LargeLight<RedLight>>(Vec(54, 28), module, PanAndShift::TRIGGER_LIGHT));

	addInput(createInput<PJ301MPort>(Vec(20, 50), module, PanAndShift::CLOCK_INPUT));
	addInput(createInput<PJ301MPort>(Vec(50, 50), module, PanAndShift::TRIGGER_INPUT));

	addInput(createInput<PJ301MPort>(Vec(20, 80), module, PanAndShift::CV_INPUT));


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
    addChild(createLight<MediumLight<GreenLight>>(Vec(xOffset, laneHead), module, PanAndShift::PAN_IN_LIGHTS + lane));
    addChild(createLight<MediumLight<BlueLight>>(Vec(xOffset, laneHead+15), module, PanAndShift::PAN_OUT_LIGHTS + lane));
    addParam(createParam<RoundLargeBlackKnob>(Vec(xOffset+10, laneHead), module, PanAndShift::LANE_LENGTH + lane, 0.0, 1.0, 0.0));

    for(int stripe = 0; stripe < STRIPE_COUNT; stripe++) {
      addChild(createLight<TinyLight<GreenLight>>(Vec(xOffset+20, laneHead + 50 + stripe*8), module, PanAndShift::LENGTH_LIGHTS + (STRIPE_COUNT * lane) + stripe));
      addChild(createLight<TinyLight<RedLight>>(Vec(xOffset+40, laneHead + 50 + stripe*8), module, PanAndShift::GATE_LIGHTS + (STRIPE_COUNT * lane) + stripe));
    }

    addOutput(createOutput<PJ301MPort>(Vec(x2Offset, laneHead+200), module, PanAndShift::LANE_OUTPUT + lane));
    addOutput(createOutput<PJ301MPort>(Vec(x2Offset+30, laneHead+200), module, PanAndShift::LANE_TRIGGER_OUTPUT + lane));
  }
}

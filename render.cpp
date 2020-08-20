/*
 ____  _____ _        _    
| __ )| ____| |      / \   
|  _ \|  _| | |     / _ \  
| |_) | |___| |___ / ___ \ 
|____/|_____|_____/_/   \_\

The platform for ultra-low latency audio and sensor processing

http://bela.io

A project of the Augmented Instruments Laboratory within the
Centre for Digital Music at Queen Mary University of London.
http://www.eecs.qmul.ac.uk/~andrewm

(c) 2016 Augmented Instruments Laboratory: Andrew McPherson,
	Astrid Bin, Liam Donovan, Christian Heinrichs, Robert Jack,
	Giulio Moro, Laurel Pardue, Victor Zappi. All rights reserved.

The Bela software is distributed under the GNU Lesser General Public License
(LGPL 3.0), available here: https://www.gnu.org/licenses/lgpl-3.0.txt
*/

#include <Bela.h>
#include <algorithm>
#include <libraries/Gui/Gui.h>
#include <libraries/math_neon/math_neon.h>
#include <libraries/GuiController/GuiController.h>


Gui gui;
GuiController controller;

const int SAMPLERATE = 44100;
const int maxDelayTime = 1; //in seconds
const int bufferSize = SAMPLERATE * maxDelayTime + 1;

int gAudioChannelNum; // number of audio channels to iterate over
int gAnalogChannelNum; // number of analog channels to iterate over
int bufferIdx = 0;

unsigned int gDelaySliderIdx;
unsigned int gdelayMixingSliderIdx;
unsigned int gfeedbackSliderIdx;
unsigned int gflangerSpeedSliderIdx;
unsigned int gflangerIntensitySliderIdx;
unsigned int gflangerBaseDelaySliderIdx;

float delayTime = 0.5f;
float delayMixing = 0.0f;

float buffer[bufferSize] = { 0.0f };

float delayFilter = 0.005f;
float oldDelayTime = 0.0f;
float feedback = 0.0f;
float baseDelay = 0.001f;

float phase = 0.0f;
float flangerSpeed = 0.0f;
float flangerIntensity = 0.0f;


bool setup(BelaContext *context, void *userData)
{

		// Set up the GUI
	gui.setup(context->projectName);
	// and attach to it
	controller.setup(&gui, "Controls");

	// Arguments: name, default value, minimum, maximum, increment
	// store the return value to read from the slider later on
	//Name, Default Value, Min Value, Max Vlaue, Step Size
	gDelaySliderIdx = controller.addSlider("Delay (in sec.)", 0.5, 0, 2, 0.00001);
	gdelayMixingSliderIdx = controller.addSlider("Mixing", 0, 0, 1, 0.0001);
	gfeedbackSliderIdx = controller.addSlider("Feedback", 0, 0, 1, 0.0001);
	gflangerSpeedSliderIdx = controller.addSlider("Flanger Speed", 0, 0, 5, 0.0001);
	gflangerIntensitySliderIdx = controller.addSlider("Flanger Intensity", 0, 0, 1, 0.001);
	gflangerBaseDelaySliderIdx = controller.addSlider("Flanger BaseDelay", 0, 0, 0.1, 0.0001);

	// Check that we have the same number of inputs and outputs.
	if(context->audioInChannels != context->audioOutChannels ||
			context->analogInChannels != context-> analogOutChannels){
		printf("Different number of outputs and inputs available. Working with what we have.\n");
	}

	// If the amout of audio and analog input and output channels is not the same
	// we will use the minimum between input and output
	gAudioChannelNum = std::min(context->audioInChannels, context->audioOutChannels);
	gAnalogChannelNum = std::min(context->analogInChannels, context->analogOutChannels);

	return true;
}

void render(BelaContext *context, void *userData)
{	

	delayTime = (1 - delayFilter) * oldDelayTime + delayFilter * controller.getSliderValue(gDelaySliderIdx);
	oldDelayTime = delayTime;
	delayMixing = controller.getSliderValue(gdelayMixingSliderIdx);
	feedback = controller.getSliderValue(gfeedbackSliderIdx);
	flangerSpeed = controller.getSliderValue(gflangerSpeedSliderIdx);
	flangerIntensity = controller.getSliderValue(gflangerIntensitySliderIdx);
	baseDelay = controller.getSliderValue(gflangerBaseDelaySliderIdx);

	phase += 2*M_PI/SAMPLERATE*flangerSpeed;

	if (phase >= 2*M_PI){
		phase -= 2*M_PI;
	}
	float lfoValue = sinf_neon(phase);
	
	if ( flangerSpeed >= 0) {
		delayTime = baseDelay + lfoValue * flangerIntensity * baseDelay;
	}
	

	// Simplest possible case: pass inputs through to outputs
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		float insample = 0.0f;
		for(unsigned int ch = 0; ch < gAudioChannelNum; ch++){
			insample += audioRead(context, n, ch);
		}

		int delayTimeInSamples = SAMPLERATE * delayTime;
		int wrappedIdx = (bufferIdx + bufferSize - delayTimeInSamples) % bufferSize;
		
		float out = (1 - delayMixing) * insample + delayMixing * buffer[wrappedIdx];

		bufferIdx++;
		buffer[bufferIdx] = insample - feedback * buffer[wrappedIdx];
		bufferIdx %= bufferSize;

		for(unsigned int ch = 0; ch < gAudioChannelNum; ch++){
			audioWrite(context, n, ch, out);
		}

	}



	// // Same with analog channels
	// for(unsigned int n = 0; n < context->analogFrames; n++) {
	// 	for(unsigned int ch = 0; ch < gAnalogChannelNum; ch++) {
	// 		analogWriteOnce(context, n, ch, analogRead(context, n, ch));
	// 	}
	// }
}

void cleanup(BelaContext *context, void *userData)
{

}


/**
\example passthrough/render.cpp

Audio and analog passthrough: input to output
-----------------------------------------

This sketch demonstrates how to read from and write to the audio and analog input and output buffers.

In `render()` you'll see a nested for loop structure. You'll see this in all Bela projects. 
The first for loop cycles through `audioFrames`, the second through 
`audioInChannels` (in this case left 0 and right 1).

You can access any information about current audio and sensor settings like this: 
`context->name_of_item`. For example `context->audioInChannels` returns current number of input channels,
`context->audioFrames` returns the current number of audio frames, 
`context->audioSampleRate` returns the audio sample rate.

You can look at all the information you can access in ::BelaContext.

Reading and writing from the audio buffers
------------------------------------------

The simplest way to read samples from the audio input buffer is with
`audioRead()` which we pass three arguments: context, current audio 
frame and current channel. In this example we have 
`audioRead(context, n, ch)` where both `n` and `ch` are provided by 
the nested for loop structure.

We can write samples to the audio output buffer in a similar way using 
`audioWrite()`. This has a fourth argument which is the value of to output.
For example `audioWrite(context, n, ch, value_to_output)`.

Reading and writing from the analog buffers
-------------------------------------------

The same is true for `analogRead()` and `analogWriteOnce()`.

Note that for the analog channels we write to and read from the buffers in a separate set 
of nested for loops. This is because they are sampled at half audio rate by default.
The first of these for loops cycles through `analogFrames`, the second through
`analogInChannels`.

By setting `audioWrite(context, n, ch, audioRead(context, n, ch))` and
`analogWriteOnce(context, n, ch, analogRead(context, n, ch))` we have a simple 
passthrough of audio input to output and analog input to output.


It is also possible to address the buffers directly, for example: 
`context->audioOut[n * context->audioOutChannels + ch]`.
*/

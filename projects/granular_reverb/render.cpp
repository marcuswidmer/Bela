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

// Simple Delay on Audio Input with Low Pass Filter

#include <Bela.h>
#include <Resample.h>

#define MAX_GRAIN_SIZE 44100 // 100 ms

#define MAX_REPS_PER_GRAIN 120
#define NUM_GRAINS 300

#define GRAIN_CANVAS_SIZE 88200 // 2 seconds

//
float grain_canvas_l[GRAIN_CANVAS_SIZE] = {0};
float grain_canvas_r[GRAIN_CANVAS_SIZE] = {0};

// Buffer containing random delays
int random_delays_l[NUM_GRAINS][MAX_REPS_PER_GRAIN] = {0};
int random_delays_r[NUM_GRAINS][MAX_REPS_PER_GRAIN] = {0};

// Buffer containing random amps
float random_amps_l[NUM_GRAINS][MAX_REPS_PER_GRAIN] = {0};
float random_amps_r[NUM_GRAINS][MAX_REPS_PER_GRAIN] = {0};

float grain_array[NUM_GRAINS][MAX_GRAIN_SIZE] = {0};

float out_l = 0;
float out_r = 0;
float input_sample = 0;
float canvas_input = 0;
float out_l_prev = 0;
float grain_sample = 0;
float amplitude = 0;
float feedback = 0;
float freeze_dial = 0;
float = pitch;

unsigned int grain_index = 0;
unsigned int delay_samples_l = 0;
unsigned int delay_samples_r = 0;
unsigned int grain_size;
unsigned int grain_write_ptr = 0;
unsigned int freeze_fade_ptr = 0;
unsigned int grain_number = 0;
unsigned int gain_reps = 100;

bool freeze = false;
bool true_freeze = false;

int gSensorInputVolume = 0;
int gSensorInputGrainSize = 1;
int gSensorInputFeedback = 2;
int gSensorInputGainReps = 3;
int gSensorInputFreeze = 4;
int gSensorInputPitch = 5;

int gAudioFramesPerAnalogFrame = 0;
int canvas_write_ptr = -1;

// Butterworth coefficients for low-pass filter @ 8000Hz
float gDel_a0 = 0.1772443606634904;
float gDel_a1 = 0.3544887213269808;
float gDel_a2 = 0.1772443606634904;
float gDel_a3 = -0.5087156198145868;
float gDel_a4 = 0.2176930624685485;

// Previous two input and output values for each channel (required for applying the filter)
float gDel_x1_l = 0;
float gDel_x2_l = 0;
float gDel_y1_l = 0;
float gDel_y2_l = 0;
float gDel_x1_r = 0;
float gDel_x2_r = 0;
float gDel_y1_r = 0;
float gDel_y2_r = 0;

static void initialize_delay_array(int random_delays[NUM_GRAINS][MAX_REPS_PER_GRAIN])
{
    for (unsigned int k = 0; k < NUM_GRAINS; k++) {
        for (unsigned int l = 0; l < MAX_REPS_PER_GRAIN; l++) {
            random_delays[k][l] = (int)(rand() % GRAIN_CANVAS_SIZE);
        }
    }
}

static void initialize_amp_array(float random_amps[NUM_GRAINS][MAX_REPS_PER_GRAIN])
{
    for (unsigned int k = 0; k < NUM_GRAINS; k++) {
        for (unsigned int l = 0; l < MAX_REPS_PER_GRAIN; l++) {
            random_amps[k][l] = (float)(rand() % GRAIN_CANVAS_SIZE) / GRAIN_CANVAS_SIZE;
        }
    }
}

static float hann_function(int n, int total_length)
{
    return ((cos((float(n) / total_length) * 2 * M_PI - M_PI) + 1) / 2);
}

bool setup(BelaContext *context, void *userData)
{
    initialize_delay_array(random_delays_l);
    initialize_delay_array(random_delays_r);
    initialize_amp_array(random_amps_l);
    initialize_amp_array(random_amps_r);

    grain_size = 4410;

    // Useful calculations
    if(context->analogFrames)
        gAudioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;

    return true;
}

static void pitch_grain(float * grain_array, int grain_len, float pitch)
{
    float resampled_grain_array[grain_len] = {0};
    int n_resamp = (int)grain_len * exp(-log(2) / 2 * pitch);

    Resample(grain_array, grain_len, resampled_grain_array, n_resamp);

    grain_array = resampled_grain_array;
}

void render(BelaContext *context, void *userData)
{
    for(unsigned int n = 0; n < context->audioFrames; n++) {
        float out_l = 0;
        float out_r = 0;

        if(gAudioFramesPerAnalogFrame && !(n % gAudioFramesPerAnalogFrame)) {
            amplitude = analogRead(context, n/gAudioFramesPerAnalogFrame, gSensorInputVolume);

            if (!true_freeze)
                grain_size = map(analogRead(context, n/gAudioFramesPerAnalogFrame, gSensorInputGrainSize), 0, 1, 500, MAX_GRAIN_SIZE);

            feedback = map(analogRead(context, n/gAudioFramesPerAnalogFrame, gSensorInputFeedback), 0, 1, 0.5, 1.5);
            gain_reps = map(analogRead(context, n/gAudioFramesPerAnalogFrame, gSensorInputGainReps), 0, 1, 2, MAX_REPS_PER_GRAIN);
            freeze_dial = map(analogRead(context, n/gAudioFramesPerAnalogFrame, gSensorInputFreeze), 0, 1, 0, 100);
            pitch = map(analogRead(context, n/gAudioFramesPerAnalogFrame, gSensorInputPitch), 0, 1, -4, 4);
            freeze = freeze_dial > 50;

            if (freeze)
                feedback = 0.95;

        }

        // Increment grain write pointer and grain_number
        grain_write_ptr++;
        if (grain_write_ptr >= grain_size) {
            true_freeze = freeze;

            grain_write_ptr = 0;
            pitch_grain(grain_array[grain_number], grain_size, pitch);
            grain_number++;

            if (grain_number >= NUM_GRAINS) {
                grain_number = 0;
            }
        }

        // Incement main canvar writer pointer
        canvas_write_ptr++;
        if (canvas_write_ptr >= GRAIN_CANVAS_SIZE) {
            //printf("Max sample lately: %f\n", out_l_prev);
            out_l_prev = 0;
            canvas_write_ptr = 0;
        }

        // Read audio inputs
        if (!true_freeze) {
            input_sample = audioRead(context,n,0);
            grain_sample = hann_function(grain_write_ptr, grain_size) * input_sample;
            grain_array[grain_number][grain_write_ptr] = grain_sample;
        }

        // grain_array_buffer[grain_write_ptr][grain_number] = hann_function(grain_write_ptr, grain_size) * input_sample;

        for (unsigned int i = 0; i < gain_reps; i++) {
            delay_samples_l = random_delays_l[grain_index][i];
            grain_canvas_l[(canvas_write_ptr + delay_samples_l) % GRAIN_CANVAS_SIZE] *= feedback;
            grain_canvas_l[(canvas_write_ptr + delay_samples_l) % GRAIN_CANVAS_SIZE] += random_amps_l[grain_number][i] * grain_array[grain_number][grain_write_ptr];

            delay_samples_r = random_delays_r[grain_index][i];
            grain_canvas_r[(canvas_write_ptr + delay_samples_r) % GRAIN_CANVAS_SIZE] *= feedback;
            grain_canvas_r[(canvas_write_ptr + delay_samples_r) % GRAIN_CANVAS_SIZE] += random_amps_r[grain_number][i] * grain_array[grain_number][grain_write_ptr];
        }

        out_l = amplitude * (grain_canvas_l[canvas_write_ptr] + input_sample);
        out_r = amplitude * (grain_canvas_r[canvas_write_ptr] + input_sample);

        if (abs(out_l) > 0.8)
            abort();

        audioWrite(context, n, 0, out_l);
        audioWrite(context, n, 1, out_r);
    }
}



void cleanup(BelaContext *context, void *userData)
{

}

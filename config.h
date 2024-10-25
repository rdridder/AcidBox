#define PROG_NAME       "ESP32 AcidBox"
#define VERSION         "v.1.3.3"

#define BOARD_HAS_UART_CHIP

#define JUKEBOX                 // real-time endless auto-compose acid tunes
#define JUKEBOX_PLAY_ON_START   // should it play on power on, or should it wait for "boot" button to be pressed
//#define MIDI_RAMPS              // this is what makes automated Cutoff-Reso-FX turn
//#define TEST_POTS               // experimental interactivity with potentiometers connected to POT_PINS[] defined below

//#define USE_INTERNAL_DAC      // use this for testing, SOUND QUALITY SACRIFICED: NOISY 8BIT STEREO
//#define NO_PSRAM              // if you don't have PSRAM on your board, then use this define, but REVERB TO BE SACRIFICED, ONE SMALL DRUM KIT SAMPLES USED 

//#define LOLIN_RGB               // Flashes the LOLIN S3 built-in RGB-LED

//#define DEBUG_ON              // note that debugging eats ticks initially belonging to real-time tasks, so sound output will be spoild in most cases, turn it off for production build
//#define DEBUG_MASTER_OUT      // serial monitor plotter will draw the output waveform
//#define DEBUG_SAMPLER
//#define DEBUG_SYNTH
//#define DEBUG_JUKEBOX
//#define DEBUG_FX
//#define DEBUG_TIMING
//#define DEBUG_MIDI

#define MIDI_VIA_SERIAL       // use this option to enable Hairless MIDI on Serial port @115200 baud (USB connector), THIS WILL BLOCK SERIAL DEBUGGING as well
//#define MIDI_VIA_SERIAL2        // use this option if you want to operate by standard MIDI @31250baud, UART2 (Serial2), 
#define MIDIRX_PIN      4       // this pin is used for input when MIDI_VIA_SERIAL2 defined (note that default pin 17 won't work with PSRAM)
#define MIDITX_PIN      15      // this pin will be used for output (not implemented yet) when MIDI_VIA_SERIAL2 defined

#define POT_NUM 3
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define I2S_BCLK_PIN    5       // I2S BIT CLOCK pin (BCL BCK CLK)
#define I2S_WCLK_PIN    7       // I2S WORD CLOCK pin (WCK WCL LCK)
#define I2S_DOUT_PIN    6       // to I2S DATA IN pin (DIN D DAT)
const uint8_t POT_PINS[POT_NUM] = {40, 41, 42};
#elif defined(CONFIG_IDF_TARGET_ESP32)
#define I2S_BCLK_PIN    5       // I2S BIT CLOCK pin (BCL BCK CLK)
#define I2S_WCLK_PIN    19      // I2S WORD CLOCK pin (WCK WCL LCK)
#define I2S_DOUT_PIN    18      // to I2S DATA IN pin (DIN D DAT)
const uint8_t POT_PINS[POT_NUM] = {34, 35, 36};
#endif


float bpm = 130.0f;

#define MAX_CUTOFF_FREQ 4000.0f
#define MIN_CUTOFF_FREQ 250.0f

#ifdef USE_INTERNAL_DAC
#define SAMPLE_RATE     22050   // price for increasing this value having NO_PSRAM is less delay time, you won't hear the difference at 8bit/sample
#else
#define SAMPLE_RATE     44100   // 44100 seems to be the right value, 48000 is also OK. Other values haven't been tested.
#endif

const float DIV_SAMPLE_RATE = 1.0f / (float)SAMPLE_RATE;
const float DIV_2SAMPLE_RATE = 0.5f / (float)SAMPLE_RATE;
const float TWO_DIV_16383 = 1.22077763e-04f;

#define TABLE_BIT  		        10UL				// bits per index of lookup tables for waveforms, exp(), sin(), cos() etc. 10 bit means 2^10 = 1024 samples
#define TABLE_SIZE            (1<<TABLE_BIT)        // samples used for lookup tables (it works pretty well down to 32 samples due to linear approximation, so listen and free some memory at your choice)
#define TABLE_MASK  	        (TABLE_SIZE-1)        // strip MSB's and remain within our desired range of TABLE_SIZE
#define CICLE_INDEX(i)        (((int32_t)(i)) & TABLE_MASK ) // this way we can operate with periodic functions or waveforms without phase-reset ("if's" are pretty costly in the matter of time)

const float DIV_TABLE_SIZE =  1.0f / (float)TABLE_SIZE;

// illinear shaper, choose preferred parameters basing on your audial experience
//#define SHAPER_USE_TANH             // use tanh() function to introduce illeniarity into the filter and compressor, it won't impact performance as this will be pre-calculated 
#define SHAPER_USE_CUBIC              // use the cubic curve to introduce illeniarity into the filter and compressor, it won't impact performance as this will be pre-calculated

// curve will be pre-calculated within -X..X range, outside this interval the function is assumed to be flat
#define SHAPER_LOOKUP_MAX 5.0f        // maximum X argument value for tanh(X) lookup table, tanh(X)~=1 if X>4 
const float SHAPER_LOOKUP_COEF = (float)TABLE_SIZE / SHAPER_LOOKUP_MAX;
#define DMA_BUF_LEN     32          // there should be no problems with low values, down to 32 samples, 64 seems to be OK with some extra
#define DMA_NUM_BUF     2           // I see no reasom to set more than 2 DMA buffers, but...

const uint32_t DMA_BUF_TIME = (uint32_t)(1000000.0f / (float)SAMPLE_RATE * (float)DMA_BUF_LEN); // microseconds per buffer, used for debugging output of time-slots

#define SYNTH1_MIDI_CHAN        1
#define SYNTH2_MIDI_CHAN        2

#define DRUM_MIDI_CHAN          10

const float TWOPI = PI*2.0f;
const float MIDI_NORM = 1.0f/127.0f;
const float ONE_DIV_PI = 1.0f/PI;
const float ONE_DIV_TWOPI = 1.0f/TWOPI;
#define FORMAT_LITTLEFS_IF_FAILED true

#define GROUP_HATS  // if so, instruments CH_NUMBER and OH_NUMBER will terminate each other (sampler module)
#define CH_NUMBER  6 // closed hat instrument number in kit (for groupping, zero-based)
#define OH_NUMBER  7 // open hat instrument number in kit (for groupping, zero-based)

#ifdef NO_PSRAM
  #define RAM_SAMPLER_CACHE  40000    // bytes, compact sample set is 132kB, first 8 samples is ~38kB
  #define DEFAULT_DRUMKIT 4           // /data/4/ folder
  #define SAMPLECNT       8           // how many samples we prepare (here just 8)
#else
//  #define PRELOAD_ALL                 // allows operating all the samples in realtime
  #define PSRAM_SAMPLER_CACHE 3145728 // bytes, we are going to preload ALL the samples from FLASH to PSRAM
                                      // we divide samples by octaves to use modifiers to particular instruments, not just note numbers
                                      // i.e. we know that all the "C" notes in all octaves are bass drums, and CC_808_BD_TONE affects all BD's
  #define SAMPLECNT       (7 * 12)    // how many samples we prepare (8 octaves by 12 samples)
  #define DEFAULT_DRUMKIT 0           // in my /data /0 has a massive bassdrum , /6 = 808 samples
#endif

#define TINY 1e-32;

#ifndef LED_BUILTIN
#define LED_BUILTIN 0
#endif

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

#if (defined ARDUINO_LOLIN_S3_PRO)
#undef BOARD_HAS_UART_CHIP
#endif

#if (defined BOARD_HAS_UART_CHIP)
  #define MIDI_PORT_TYPE HardwareSerial
  #define MIDI_PORT Serial
  #define DEBUG_PORT Serial
#else
  #if (ESP_ARDUINO_VERSION_MAJOR < 3)
    #define MIDI_PORT_TYPE HWCDC
    #define MIDI_PORT USBSerial
    #define DEBUG_PORT Serial
  #else
    #define MIDI_PORT_TYPE HardwareSerial
    #define MIDI_PORT Serial
    #define DEBUG_PORT Serial
  #endif
#endif

#ifdef MIDI_VIA_SERIAL
  #undef DEBUG_ON
#endif

// debug macros
#ifdef DEBUG_ON
  #define DEB(...)    DEBUG_PORT.print(__VA_ARGS__) 
  #define DEBF(...)   DEBUG_PORT.printf(__VA_ARGS__)
  #define DEBUG(...)  DEBUG_PORT.println(__VA_ARGS__)
#else
  #define DEB(...)
  #define DEBF(...)
  #define DEBUG(...)
#endif


// normalizing matrices for TB filter and distortion/overdrive pairs
#define NORM1_DEPTH 1.0f 
#define NORM2_DEPTH 1.0f

/* 
const float cutoff_reso[16][16] = { // flat EQ linear amplitude
{4.8385, 4.88747, 4.92047, 4.76436, 4.8962, 5.01568, 4.98983, 5.01559, 5.10654, 5.03281, 5.01903, 4.95362, 4.81538, 4.8074, 4.74791, 4.54329},
{3.87221, 3.90254, 3.82171, 3.75677, 3.7406, 3.67065, 3.69104, 3.56865, 3.54271, 3.67638, 3.65262, 3.57059, 3.58953, 3.51315, 3.45127, 3.37038},
{3.1284, 3.14124, 3.14524, 3.09576, 3.02473, 3.06767, 3.04812, 3.06489, 3.0655, 2.99194, 2.95566, 2.83795, 2.68933, 2.75138, 2.64402, 2.47201},
{2.92398, 2.76649, 2.72365, 2.67662, 2.59435, 2.57171, 2.63079, 2.59806, 2.50945, 2.50648, 2.49537, 2.44069, 2.38885, 2.29797, 2.17515, 2.04705},
{2.69233, 2.58547, 2.62899, 2.56338, 2.56841, 2.48645, 2.40444, 2.36171, 2.26753, 2.17892, 2.22557, 2.13955, 2.05197, 1.9635, 1.877, 1.77883},
{2.49518, 2.41459, 2.35943, 2.48924, 2.4879, 2.30845, 2.28157, 2.2319, 2.19218, 2.12374, 2.04543, 1.96772, 1.87544, 1.72046, 1.71919, 1.58504},
{2.56721, 2.4846, 2.46367, 2.38851, 2.34092, 2.21967, 2.10531, 2.17264, 2.08794, 1.98999, 1.93158, 1.86221, 1.79165, 1.68922, 1.61215, 1.49858},
{2.44912, 2.41703, 2.38924, 2.60044, 2.47826, 2.26369, 2.26848, 2.08206, 2.01731, 1.95231, 1.78664, 1.81617, 1.68899, 1.58835, 1.48491, 1.39299},
{2.42586, 2.44112, 2.3695, 2.38807, 2.42516, 2.21484, 2.30564, 2.09487, 2.14824, 1.97235, 1.88533, 1.7607, 1.67901, 1.56326, 1.41019, 1.35799},
{2.51576, 2.53396, 2.47729, 2.66741, 2.33181, 2.21481, 2.31478, 1.98828, 2.12556, 1.97937, 1.8806, 1.76699, 1.70553, 1.57373, 1.48169, 1.35733},
{2.25554, 2.45139, 2.38947, 2.78224, 2.49177, 2.3555, 2.46807, 2.16115, 2.14116, 1.98996, 1.89712, 1.7226, 1.68581, 1.60263, 1.49875, 1.35428},
{2.51794, 2.50148, 2.46082, 2.69584, 2.30117, 2.27949, 2.5582, 2.17867, 2.31131, 2.21329, 2.02697, 1.9131, 1.75641, 1.61857, 1.52771, 1.35415},
{2.46675, 2.60235, 2.55729, 2.84951, 2.49979, 2.33553, 2.49566, 2.21991, 2.20328, 2.13907, 2.08274, 1.94865, 1.87065, 1.78613, 1.61446, 1.47914},
{2.40512, 2.40291, 2.40859, 2.97096, 2.52717, 2.39973, 2.88218, 2.37344, 2.43893, 2.30513, 2.12342, 1.99408, 1.90687, 1.7411, 1.70994, 1.5803},
{2.54366, 2.64905, 2.52548, 2.75611, 2.52512, 2.28283, 2.65487, 2.36714, 2.51868, 2.44883, 2.36448, 2.20553, 2.13651, 1.99002, 1.77779, 1.66373},
{2.43544, 2.58627, 2.48965, 3.20733, 2.63355, 2.50921, 2.83243, 2.43752, 2.50693, 2.39616, 2.26776, 2.28478, 2.22265, 2.13063, 2.08305, 2.01791}
};
const float cutoff_reso_avg = 2.506875f;


const float wfolder_overdrive[16][16] = { // flat EQ linear amplitude
{1.81553, 2.92774, 4.06149, 5.07091, 6.26805, 7.34979, 8.18204, 8.78785, 9.45271, 10.05631, 10.65088, 11.09989, 11.4481, 11.92296, 12.23205, 12.29738},
{2.6708, 4.41444, 5.95289, 7.47505, 8.39286, 9.33202, 10.16724, 10.7217, 11.29222, 12.31747, 12.93994, 13.2008, 13.66097, 14.02695, 14.27628, 14.42418},
{3.38126, 5.6452, 7.62784, 9.13709, 10.18128, 11.301, 12.05577, 12.97395, 13.60086, 14.02923, 14.41857, 14.53039, 14.12202, 14.99198, 14.77959, 14.68183},
{4.37828, 7.03035, 9.03751, 10.54589, 11.27255, 12.1775, 13.68013, 14.3049, 14.23863, 14.48288, 14.76953, 14.95656, 14.62193, 14.64976, 14.68791, 14.60861},
{6.15714, 9.96185, 13.47305, 14.8845, 14.73223, 14.25685, 14.59641, 15.15803, 15.03315, 13.94896, 14.08422, 13.62272, 13.85014, 14.56888, 15.05049, 14.73128},
{10.18297, 16.0993, 20.0271, 17.58701, 13.204, 9.45962, 11.62678, 15.84615, 18.96596, 17.04844, 14.1457, 11.65228, 12.7452, 15.39917, 18.01944, 16.55685},
{12.50609, 20.47274, 23.53324, 16.68068, 9.79378, 6.98393, 12.2715, 20.1654, 22.88357, 16.51044, 10.072, 7.87354, 13.03754, 20.20183, 22.35985, 16.68395},
{13.70303, 22.44605, 24.62353, 16.09332, 8.1773, 5.98764, 13.66198, 22.47247, 24.37917, 16.06878, 7.99532, 6.49434, 13.79997, 22.42913, 23.63804, 15.76187},
{14.30539, 23.34099, 25.17157, 15.41343, 7.29526, 5.57348, 13.81831, 23.37496, 25.28368, 15.90457, 7.36298, 6.03334, 14.22995, 23.69467, 23.89587, 15.23157},
{14.54421, 23.93977, 25.9227, 15.9401, 6.95805, 5.33766, 14.02165, 23.19047, 25.30687, 15.63357, 6.78814, 5.57623, 14.38434, 24.30402, 25.2735, 15.50198},
{14.32658, 24.08095, 26.12392, 15.84031, 6.72403, 5.14575, 14.19205, 24.49798, 25.84027, 15.76886, 6.62274, 5.35413, 13.98917, 24.50435, 25.33603, 15.37113},
{15.00247, 24.4431, 26.46044, 15.87536, 6.35671, 4.90051, 14.08632, 24.45546, 25.82815, 15.72268, 6.39308, 5.25604, 14.5012, 24.96537, 25.79327, 15.12743},
{14.97389, 24.50721, 26.50383, 15.8626, 6.40081, 4.9022, 14.26976, 24.82402, 25.15142, 15.14676, 6.22857, 5.13013, 14.37914, 24.82676, 25.89133, 15.38875},
{15.2131, 23.8389, 26.23546, 15.77076, 6.33121, 4.79957, 14.31421, 25.04027, 26.28032, 15.6678, 6.1222, 5.11645, 14.19984, 24.30534, 25.7695, 15.35306},
{15.17793, 24.84065, 26.53333, 15.91002, 6.31883, 4.62659, 13.85447, 24.78054, 25.97502, 15.46608, 6.03461, 5.06064, 14.63126, 25.18358, 26.04326, 15.42947},
{15.16892, 24.67783, 26.40122, 15.80733, 6.27843, 4.76886, 14.31771, 25.04004, 26.3323, 15.22194, 5.83839, 4.99684, 14.46052, 25.16456, 25.8343, 15.38862}
};
const float wfolder_overdrive_avg = 14.70303f;
*/
/*
const float cutoff_reso[16][16] = { // D-weighting curve linear amplitude
{5.088443, 4.748775, 4.213981, 3.806754, 3.814063, 3.891116, 3.607284, 3.705055, 4.047698, 3.973349, 4.032753, 4.297999, 4.399147, 4.164255, 4.265074, 4.560594},
{5.480893, 4.635213, 4.270601, 4.014409, 3.723213, 3.648567, 3.852473, 3.880512, 3.952992, 4.258348, 4.468791, 4.430753, 4.682340, 4.780585, 4.870942, 4.982014},
{5.801386, 4.982126, 4.173073, 3.927025, 3.821813, 3.830817, 3.813940, 4.052980, 4.182085, 4.149244, 4.385879, 4.558488, 4.619930, 4.595871, 4.747104, 5.007563},
{5.914363, 4.792323, 4.326102, 4.013857, 3.716824, 3.755425, 4.281695, 4.412211, 4.448431, 4.943832, 5.413869, 5.404469, 5.278687, 6.044177, 6.174179, 5.860119},
{6.225181, 5.118595, 4.179591, 3.819639, 3.778664, 3.988809, 3.826071, 4.089345, 4.387860, 4.304595, 4.444678, 4.724508, 4.909694, 4.960629, 5.133572, 5.380276},
{6.104742, 4.897842, 4.145612, 3.776814, 3.430487, 3.493173, 3.673556, 3.560862, 3.767686, 4.088268, 4.325448, 4.266427, 4.534279, 5.001377, 4.838728, 5.116366},
{6.388631, 5.000104, 3.923562, 3.373810, 3.552555, 3.384681, 3.475560, 3.943514, 4.346936, 4.514819, 4.676305, 5.487291, 5.773126, 5.864404, 6.348352, 6.885350},
{6.318010, 4.543581, 3.810727, 3.485013, 3.260142, 3.493744, 3.691319, 3.750343, 4.004967, 4.416778, 4.711856, 5.045936, 5.606477, 6.243788, 6.224719, 6.857349},
{6.351570, 4.719615, 3.677153, 3.361043, 3.569208, 3.525641, 3.911758, 4.485856, 4.996965, 5.380759, 5.652077, 6.842835, 7.078077, 7.665046, 8.502805, 9.156425},
{6.273955, 4.355492, 3.707555, 3.483126, 3.521939, 3.849478, 4.470348, 4.593672, 5.145930, 6.276892, 6.837686, 7.431805, 8.649154, 9.819227, 9.916071, 10.709123},
{6.466042, 4.654976, 3.505829, 3.753159, 3.846766, 4.059497, 4.547558, 5.086736, 5.859083, 6.392162, 7.265354, 8.778674, 8.991282, 10.449076, 11.504684, 12.867598},
{6.271001, 4.340880, 3.825479, 3.526718, 3.804931, 4.228046, 4.863353, 5.226099, 5.693886, 7.190523, 7.487588, 8.638903, 10.065550, 11.563904, 12.393667, 13.181049},
{6.692892, 4.329665, 3.611883, 3.737395, 3.935531, 4.151097, 4.419905, 5.393781, 5.887227, 6.507381, 7.729293, 9.264614, 10.000851, 11.397189, 13.108201, 14.891965},
{6.165781, 4.303358, 3.679214, 3.498906, 3.569861, 3.808628, 4.288381, 4.573395, 5.305214, 6.277582, 6.924616, 8.224044, 9.240870, 11.207167, 12.074885, 13.946956},
{6.507094, 4.104322, 3.525125, 3.297218, 3.403118, 3.408006, 3.558969, 4.307840, 4.580469, 5.175146, 5.903279, 7.325158, 7.869888, 8.849345, 11.131460, 12.524242},
{6.205086, 4.138503, 3.464386, 2.883092, 2.711778, 3.014128, 3.131652, 3.280128, 3.670246, 4.381391, 4.794806, 5.374605, 6.395000, 7.877948, 8.369201, 10.447331}
};
const float cutoff_reso_avg = 5.4167266f;
*/

const float wfolder_overdrive[16][16] = { // D-weighting curve linear amplitude
{4.321596, 6.677420, 9.351027, 12.337818, 15.274008, 17.178272, 20.258532, 22.640339, 23.268341, 25.133560, 25.689850, 27.329815, 26.931023, 27.971588, 28.773928, 27.811522},
{6.072484, 10.221110, 14.169627, 17.745028, 20.698469, 24.349220, 25.056787, 26.135130, 27.644402, 29.212200, 28.163837, 28.859060, 30.591475, 30.274736, 30.926729, 32.161110},
{8.636417, 13.038910, 17.829748, 23.371164, 25.444685, 26.327213, 28.255512, 29.594391, 29.098421, 29.936840, 31.134794, 32.169270, 32.045223, 32.963749, 32.976822, 32.455048},
{10.054599, 16.592342, 22.589405, 25.882214, 28.521395, 29.180340, 29.164886, 30.660505, 31.349100, 32.824554, 32.293663, 33.904400, 33.067791, 32.399799, 33.414825, 33.234741},
{15.011400, 23.227974, 31.648169, 35.058456, 32.518459, 30.586367, 30.851610, 33.365906, 34.632706, 35.548817, 35.342865, 33.191872, 33.392647, 35.636063, 37.346981, 36.876877},
{23.186680, 38.978333, 48.210861, 40.115742, 31.781157, 24.790371, 31.183672, 40.898441, 47.043743, 41.951683, 33.172577, 28.403805, 32.496960, 41.487301, 46.415443, 41.383484},
{29.932003, 47.438541, 56.828445, 41.501617, 26.867907, 20.738359, 32.649918, 49.848667, 54.199947, 42.713993, 27.205708, 22.805727, 34.331593, 49.077122, 54.067410, 40.862373},
{33.698540, 54.806038, 60.328083, 41.728024, 22.957489, 17.200394, 33.867798, 54.327579, 60.292271, 40.527672, 22.979492, 19.049171, 35.523216, 54.576229, 58.380924, 40.722004},
{35.641998, 57.294373, 64.910187, 41.493423, 20.014410, 14.921355, 34.554081, 58.559727, 62.141014, 41.429733, 19.964781, 16.598537, 35.357327, 58.005657, 62.420906, 40.056889},
{37.487209, 59.607437, 65.128052, 41.079487, 18.249022, 13.841405, 35.108574, 61.368694, 64.131561, 39.774368, 18.559950, 14.785675, 36.562847, 60.325825, 64.121376, 39.700741},
{37.041679, 60.903545, 66.883095, 41.005798, 17.416222, 13.260203, 36.083569, 60.961777, 64.818130, 40.642086, 17.360371, 14.153852, 36.164448, 62.356262, 64.683060, 39.244789},
{38.588360, 62.251549, 66.640533, 40.079689, 16.864496, 13.063670, 35.713974, 63.281395, 65.684242, 40.100368, 16.357441, 13.569439, 36.963696, 62.407402, 66.269562, 38.965614},
{38.136345, 62.866852, 66.513802, 40.857845, 16.487530, 12.740461, 35.939171, 62.047729, 66.705620, 39.801647, 16.169365, 13.296426, 37.040180, 63.597023, 64.751793, 38.943562},
{38.979004, 63.323586, 66.772980, 40.269608, 16.517096, 12.285855, 35.989716, 64.168343, 67.227440, 39.479458, 15.763931, 13.077268, 36.944458, 63.458538, 66.844772, 39.394936},
{38.541519, 62.366325, 66.909378, 40.739624, 16.021036, 12.396644, 36.157871, 63.634758, 66.528000, 39.438278, 15.723740, 12.900184, 37.455818, 63.688789, 65.662186, 39.418522},
{38.486912, 63.623055, 67.025940, 40.878666, 15.853469, 12.048793, 36.591278, 63.678566, 67.363892, 39.386097, 15.598418, 12.830324, 36.486755, 63.888874, 67.060234, 39.503799}
};
const float wfolder_overdrive_avg = 36.59637f;

float cutoff_reso[16][16] = { // k-weigted mean quad
{6.434804, 4.714645, 3.947374, 2.694166, 2.351397, 2.500912, 2.929582, 2.654394, 2.284407, 1.838856, 2.644853, 2.766961, 2.814959, 2.350692, 1.996572, 2.199751},
{6.612917, 5.302139, 3.893952, 2.907703, 2.001597, 2.857677, 2.988061, 3.030843, 2.442840, 2.147922, 2.721878, 2.790063, 2.963842, 2.972988, 2.489201, 2.509848},
{7.350744, 5.508491, 4.067600, 2.890480, 2.176190, 2.566737, 2.546578, 2.596522, 2.278280, 1.956051, 2.581862, 2.548600, 3.036592, 2.538826, 2.439919, 1.898532},
{8.183912, 5.427793, 4.194474, 2.903769, 2.359180, 2.242266, 2.286241, 3.119573, 3.544204, 3.191301, 2.518959, 2.913494, 4.572999, 5.861769, 4.722514, 3.665691},
{7.280022, 4.675759, 3.778736, 3.111181, 2.431638, 2.358587, 2.270216, 2.901183, 3.002531, 2.652288, 1.998837, 2.844437, 3.059708, 3.341244, 2.923321, 2.497309},
{6.960493, 4.442485, 3.891997, 3.066457, 2.455318, 1.855044, 2.421693, 2.687097, 2.769465, 2.300333, 2.023683, 2.392432, 2.613516, 2.961050, 2.950050, 2.489161},
{6.600060, 4.763449, 4.023485, 3.143195, 2.520189, 1.899933, 2.254261, 2.552054, 2.955580, 3.250866, 2.693074, 2.982439, 2.930238, 4.417704, 5.190308, 4.485150},
{5.806700, 5.164190, 3.994584, 3.393530, 2.460938, 2.048136, 2.080137, 2.109953, 2.448718, 2.548533, 2.194179, 2.070688, 3.086098, 4.052840, 4.070950, 3.569471},
{6.268085, 4.558399, 3.507618, 2.848771, 2.446060, 1.948153, 2.073992, 2.101269, 2.966322, 3.262701, 3.049813, 2.411024, 3.578253, 4.676841, 5.917085, 5.060090},
{6.759351, 4.216856, 3.118927, 2.793170, 2.484806, 2.031502, 1.679651, 2.196030, 2.861528, 3.392502, 3.039659, 2.682046, 3.355549, 3.709728, 4.872293, 5.756225},
{6.830225, 4.422283, 3.082938, 2.982032, 2.476420, 2.165540, 1.598023, 2.144882, 2.456352, 2.995122, 2.919489, 2.632485, 3.150194, 3.483125, 5.059742, 6.296432},
{7.835934, 3.491278, 3.291606, 2.806850, 2.564085, 2.023950, 1.767282, 1.784284, 1.954406, 2.394825, 2.952084, 2.615514, 2.811204, 3.472478, 5.163306, 6.343526},
{8.392389, 3.504093, 3.044849, 2.386751, 2.225955, 1.887471, 1.583561, 1.604031, 1.729157, 2.363731, 2.771977, 2.687573, 2.257057, 3.599182, 5.251308, 6.766300},
{8.595600, 3.864176, 2.557690, 1.968429, 1.866560, 1.764059, 1.478550, 1.335737, 1.606975, 2.199224, 2.646084, 2.557677, 2.242978, 3.037829, 3.686971, 5.391070},
{9.063289, 3.624348, 2.342058, 1.823246, 1.851653, 1.604091, 1.497541, 1.124157, 1.500818, 1.905059, 2.288400, 2.217282, 2.097532, 2.477182, 2.971320, 4.915813},
{9.551075, 3.699403, 1.972151, 1.721147, 1.624085, 1.514310, 1.259587, 1.062701, 1.170411, 1.331594, 1.649718, 2.004076, 1.807243, 2.236462, 2.674345, 4.867586},
};
const float cutoff_reso_avg = 3.19f;

/*
 float wfolder_overdrive[16][16] = { // k-weighted mean quad
{1.100069, 2.612626, 3.790118, 8.684330, 13.651937, 19.295862, 20.443464, 21.131121, 24.541002, 26.373375, 32.965790, 30.926220, 32.139011, 29.744080, 35.700516, 37.820103},
{2.634791, 5.464583, 9.096105, 15.762427, 21.788717, 26.992195, 29.971214, 28.517365, 30.403839, 33.226055, 39.615711, 34.655502, 39.475487, 34.761803, 40.932682, 41.794361},
{4.302970, 9.989371, 15.071154, 23.144535, 26.899090, 35.547665, 31.950285, 36.962143, 33.009865, 41.157677, 42.040821, 42.360878, 39.941982, 40.038044, 41.003162, 41.025238},
{5.831883, 14.910394, 21.974388, 28.007730, 33.020317, 39.290241, 35.500706, 39.943031, 36.429066, 42.511127, 42.573090, 41.172798, 38.672600, 38.115959, 38.328674, 38.419930},
{10.745278, 24.957157, 44.335899, 38.639091, 45.861202, 38.845505, 35.971352, 38.600151, 42.700455, 43.917099, 40.667526, 38.212841, 35.262058, 40.316406, 40.559158, 44.777973},
{21.580822, 52.749134, 76.591423, 48.345505, 33.019783, 23.045059, 26.508516, 49.446281, 60.178902, 46.360680, 31.648138, 27.352730, 26.257122, 51.410534, 54.258270, 49.397896},
{26.786173, 69.479034, 85.775749, 41.720451, 19.458248, 12.441231, 26.969410, 62.399029, 80.884926, 39.512096, 20.850580, 14.713130, 28.300602, 66.324745, 75.790932, 43.306309},
{29.055773, 74.197891, 87.453911, 35.326111, 11.756197, 7.153258, 29.866051, 70.803955, 92.299400, 34.215282, 12.947213, 9.325157, 29.023405, 75.806847, 81.992989, 37.460163},
{29.329697, 84.530838, 86.208633, 36.999393, 9.605332, 6.178853, 29.674660, 82.772247, 92.242180, 35.412563, 10.025507, 7.610517, 28.521023, 77.890633, 84.200706, 34.046375},
{30.291025, 88.884071, 89.562881, 36.682079, 7.239823, 5.217485, 29.093407, 82.466888, 90.862190, 33.188721, 8.243276, 5.953953, 31.746099, 77.590744, 95.853188, 32.116184},
{33.520702, 87.417305, 97.071663, 35.481602, 7.136596, 4.568522, 28.280918, 83.915421, 88.742973, 33.012093, 7.212764, 4.921861, 31.499189, 83.405777, 94.219452, 32.122295},
{32.871964, 85.562317, 94.751083, 34.303524, 6.551445, 4.023323, 27.467133, 92.656708, 86.027946, 35.518806, 6.443895, 4.631652, 30.416872, 87.334679, 92.183884, 32.532410},
{32.032509, 83.331741, 98.296570, 33.268421, 5.883899, 3.771136, 29.050014, 91.149338, 94.362953, 34.575657, 5.897426, 4.319282, 29.828932, 85.467506, 89.734940, 31.316317},
{31.227125, 81.005997, 104.792648, 31.702234, 6.083809, 3.492942, 29.658930, 89.070290, 94.944389, 33.290352, 5.556711, 4.071751, 28.984886, 91.882561, 86.876961, 33.439419},
{30.342409, 89.584282, 102.275230, 35.133881, 5.690284, 3.515618, 28.778267, 86.460449, 92.302498, 32.302811, 5.006345, 3.840481, 28.476063, 94.554810, 88.098221, 33.928406},
{30.110008, 88.525185, 99.403809, 34.329544, 5.596230, 3.220591, 28.182953, 84.471687, 102.308319, 31.231888, 5.534990, 3.560953, 31.054146, 92.313148, 95.137627, 32.929070},
};
const float wfolder_overdrive_avg = 41.225f;
*/

static const float tuning[128] = {
  0.500000f, 0.500000f, 0.500000f, 0.500000f, 0.500000f, 0.529732f, 0.529732f, 0.529732f, 
  0.529732f, 0.529732f, 0.561231f, 0.561231f, 0.561231f, 0.561231f, 0.561231f, 0.594604f, 
  0.594604f, 0.594604f, 0.594604f, 0.594604f, 0.594604f, 0.629961f, 0.629961f, 0.629961f, 
  0.629961f, 0.629961f, 0.667420f, 0.667420f, 0.667420f, 0.667420f, 0.667420f, 0.707107f, 
  0.707107f, 0.707107f, 0.707107f, 0.707107f, 0.749154f, 0.749154f, 0.749154f, 0.749154f, 
  0.749154f, 0.793701f, 0.793701f, 0.793701f, 0.793701f, 0.793701f, 0.840896f, 0.840896f, 
  0.840896f, 0.840896f, 0.840896f, 0.890899f, 0.890899f, 0.890899f, 0.890899f, 0.890899f, 
  0.943874f, 0.943874f, 0.943874f, 0.943874f, 0.943874f, 1.000000f, 1.000000f, 1.000000f, 
  1.000000f, 1.000000f, 1.000000f, 1.059463f, 1.059463f, 1.059463f, 1.059463f, 1.059463f, 
  1.122462f, 1.122462f, 1.122462f, 1.122462f, 1.122462f, 1.189207f, 1.189207f, 1.189207f, 
  1.189207f, 1.189207f, 1.259921f, 1.259921f, 1.259921f, 1.259921f, 1.259921f, 1.334840f, 
  1.334840f, 1.334840f, 1.334840f, 1.334840f, 1.414214f, 1.414214f, 1.414214f, 1.414214f, 
  1.414214f, 1.498307f, 1.498307f, 1.498307f, 1.498307f, 1.498307f, 1.587401f, 1.587401f, 
  1.587401f, 1.587401f, 1.587401f, 1.681793f, 1.681793f, 1.681793f, 1.681793f, 1.681793f, 
  1.681793f, 1.781797f, 1.781797f, 1.781797f, 1.781797f, 1.781797f, 1.887749f, 1.887749f, 
  1.887749f, 1.887749f, 1.887749f, 2.000000f, 2.000000f, 2.000000f, 2.000000f, 2.000000f
};


inline float fast_shape(float x);
static __attribute__((always_inline)) inline float one_div(float a) ;

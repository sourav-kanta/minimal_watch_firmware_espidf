/*
 * stage1_generate_test_vectors.c
 *
 * Generates deterministic raw int16_t XYZ accelerometer fixtures for
 * motion_detection_engine_stage1 host tests.
 *
 * Accelerometer configuration:
 *     ±8 g
 *     4096 LSB/g
 *
 * Production scaling:
 *     SAMPLE_SCALING_FACTOR = 0.0002441f ~= 1 / 4096
 *
 * Therefore:
 *     acceleration_g = raw * 0.0002441f
 *     raw = round(acceleration_g * 4096)
 *
 * Output format:
 *
 *     # scenario_name sample_count
 *     raw_x raw_y raw_z
 *     raw_x raw_y raw_z
 *     ...
 *
 *     <blank line>
 *
 * The generated data represents physical 3-axis acceleration rather than
 * precomputed vertical acceleration. The production Stage 1 pipeline is
 * therefore exercised from raw XYZ data onward.
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define OUTPUT_FILE "stage1_motion_test_vectors.txt"

#define SAMPLE_RATE_HZ                   21.0f
#define LSB_PER_G                        4096.0f

#define GRAVITY_G                        1.0f

#define WALKING_STEP_SAMPLES             15
#define WALKING_STEP_COUNT               24

#define STILLNESS_SAMPLES                63
#define GAP_SAMPLES                      42

#define VERTICAL_ACCEL_AMPLITUDE_G       0.25f
#define HORIZONTAL_ACCEL_AMPLITUDE_G     0.05f

#define STILLNESS_NOISE_STDDEV_G         0.0025f
#define MOTION_NOISE_STDDEV_G            0.015f

#define FREQUENCY_VARIATION              (4.0f / WALKING_STEP_SAMPLES)
#define AMPLITUDE_VARIATION              0.15f

#define WEAK_PEAK_SCALE                  0.15f

#define TWO_PI                           6.28318530717958647692f

/*
 * Fixed seed so generated fixtures never change between test runs.
 */
static uint32_t rng_state = 0x13579BDFu;

static uint32_t rng_u32(void) {
    /*
     * xorshift32
     */
    uint32_t x = rng_state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    rng_state = x;

    return x;
}

static float uniform_01(void) {
    return ((float)(rng_u32() & 0x00FFFFFFu)) / 16777216.0f;
}

/*
 * Standard normal distribution using Box-Muller.
 */
static float gaussian_noise(float stddev) {
    float u1;
    float u2;

    do {
        u1 = uniform_01();
    } while (u1 <= 0.0f);

    u2 = uniform_01();

    return stddev * sqrtf(-2.0f * logf(u1)) *
           cosf(TWO_PI * u2);
}

static int16_t g_to_raw(float acceleration_g) {
    float raw = acceleration_g * LSB_PER_G;

    if (raw > 32767.0f) {
        raw = 32767.0f;
    } else if (raw < -32768.0f) {
        raw = -32768.0f;
    }

    return (int16_t)lrintf(raw);
}

static void write_sample(
    FILE *file,
    float x_g,
    float y_g,
    float z_g) {
    int16_t x = g_to_raw(x_g);
    int16_t y = g_to_raw(y_g);
    int16_t z = g_to_raw(z_g);

    fprintf(file, "%d %d %d\n", x, y, z);
}

/*
 * Add a stationary sample.
 *
 * Noise is deliberately added independently to ALL THREE axes.
 *
 * This is important because the real sensor will not produce an exactly
 * constant [0, 0, 1g] vector while the watch is stationary.
 */
static void write_still_sample(FILE *file) {
    float x = gaussian_noise(STILLNESS_NOISE_STDDEV_G);
    float y = gaussian_noise(STILLNESS_NOISE_STDDEV_G);
    float z = GRAVITY_G + gaussian_noise(STILLNESS_NOISE_STDDEV_G);

    write_sample(file, x, y, z);
}

static size_t generate_stillness(FILE *file, size_t samples) {
    for (size_t i = 0; i < samples; i++) {
        write_still_sample(file);
    }
    return samples;
}

/*
 * Walking waveform.
 *
 * The dominant component is the vertical acceleration along gravity,
 * with a smaller second harmonic. X/Y contain smaller horizontal motion.
 *
 * phase runs continuously through the generated step waveform.
 */
static void write_walking_sample(
    FILE *file,
    float phase,
    float amplitude,
    float horizontal_amplitude,
    float noise_stddev) {
    /*
     * Main vertical walking component.
     *
     * The second harmonic makes this less artificially sinusoidal while
     * retaining a deterministic, smooth walking-like waveform.
     */
    float vertical =
        amplitude *
        (0.82f * sinf(phase) +
         0.18f * sinf(2.0f * phase));

    /*
     * Small horizontal components.
     *
     * They are intentionally phase shifted relative to the vertical
     * component so the input is genuinely 3-axis motion.
     */
    float x =
        horizontal_amplitude *
        sinf(phase + 0.65f);

    float y =
        horizontal_amplitude *
        0.75f *
        sinf(phase - 0.9f);

    float z = GRAVITY_G + vertical;

    x += gaussian_noise(noise_stddev);
    y += gaussian_noise(noise_stddev);
    z += gaussian_noise(noise_stddev);

    write_sample(file, x, y, z);
}

/*
 * Generate normal walking. Returns the exact number of samples generated.
 */
static size_t generate_walking(
    FILE *file,
    size_t step_count,
    size_t samples_per_step,
    int varying_frequency,
    int varying_amplitude,
    int noisy) {
    
    size_t total_samples = 0;
    float phase = 0.0f;

    for (size_t step = 0; step < step_count; step++) {

        float frequency_scale = 1.0f;

        if (varying_frequency) {
            /*
             * Deterministic variation around nominal cadence.
             */
            float variation =
                (uniform_01() * 2.0f - 1.0f) *
                FREQUENCY_VARIATION;

            frequency_scale += variation;
        }

        float amplitude = VERTICAL_ACCEL_AMPLITUDE_G;

        if (varying_amplitude) {
            float variation =
                (uniform_01() * 2.0f - 1.0f) *
                AMPLITUDE_VARIATION;

            amplitude *= 1.0f + variation;
        }

        float horizontal_amplitude =
            HORIZONTAL_ACCEL_AMPLITUDE_G;

        float noise_stddev =
            noisy ? MOTION_NOISE_STDDEV_G : 0.0f;

        /*
         * Frequency scale changes the amount of phase accumulated during
         * each step while preserving the same sample rate.
         */
        float phase_increment =
            TWO_PI /
            ((float)samples_per_step * frequency_scale);

        while (phase < TWO_PI) {
            write_walking_sample(
                file,
                phase,
                amplitude,
                horizontal_amplitude,
                noise_stddev);

            phase += phase_increment;
            total_samples++;
        }
        
        phase -= TWO_PI;
    }
    
    return total_samples;
}

/*
 * Generate walking where one step has a deliberately weak peak.
 */
static size_t generate_single_weak_peak(FILE *file) {
    size_t total_samples = 0;
    float phase = 0.0f;

    for (size_t step = 0;
         step < WALKING_STEP_COUNT;
         step++) {

        float amplitude =
            VERTICAL_ACCEL_AMPLITUDE_G;

        if (step == WALKING_STEP_COUNT / 2) {
            amplitude *= WEAK_PEAK_SCALE;
        }

        float phase_increment =
            TWO_PI / (float)WALKING_STEP_SAMPLES;

        while (phase < TWO_PI) {
            write_walking_sample(
                file,
                phase,
                amplitude,
                HORIZONTAL_ACCEL_AMPLITUDE_G,
                MOTION_NOISE_STDDEV_G);

            phase += phase_increment;
            total_samples++;
        }
        phase -= TWO_PI;
    }
    
    return total_samples;
}

/*
 * Walking -> stillness -> walking.
 */
static size_t generate_steps_gap_steps(FILE *file) {
    size_t total_samples = 0;
    total_samples += generate_walking(file, 12, WALKING_STEP_SAMPLES, 0, 0, 1);
    total_samples += generate_stillness(file, GAP_SAMPLES);
    total_samples += generate_walking(file, 12, WALKING_STEP_SAMPLES, 0, 0, 1);
    return total_samples;
}

/*
 * Walking -> stillness -> walking with a deliberately abrupt boundary.
 */
static size_t generate_discontinuous_walking(FILE *file) {
    size_t total_samples = 0;
    total_samples += generate_walking(file, 8, WALKING_STEP_SAMPLES, 0, 0, 1);
    
    /* Hard physical transition to stationary acceleration. */
    total_samples += generate_stillness(file, GAP_SAMPLES);
    
    total_samples += generate_walking(file, 8, WALKING_STEP_SAMPLES, 1, 1, 1);
    return total_samples;
}

static long write_scenario_header(FILE *file, const char *name) {
    long pos = ftell(file);
    fprintf(file, "# %-40s %8zu\n", name, (size_t)0);
    return pos;
}

static void finalize_scenario_header(FILE *file, long pos, const char *name, size_t count) {
    long end_pos = ftell(file);
    fseek(file, pos, SEEK_SET);
    fprintf(file, "# %-40s %8zu\n", name, count);
    fseek(file, end_pos, SEEK_SET);
}

int main(void) {
    FILE *file = fopen(OUTPUT_FILE, "w");

    if (!file) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    long header_pos;
    size_t sample_count;

    /* 1. Simple clean walking. */
    rng_state = 0x13579BDFu;
    header_pos = write_scenario_header(file, "simple_sine");
    sample_count = generate_walking(file, WALKING_STEP_COUNT, WALKING_STEP_SAMPLES, 0, 0, 0);
    finalize_scenario_header(file, header_pos, "simple_sine", sample_count);
    fprintf(file, "\n");

    /* 2. Walking with Gaussian noise. */
    rng_state = 0x13579BDFu;
    header_pos = write_scenario_header(file, "sine_noise");
    sample_count = generate_walking(file, WALKING_STEP_COUNT, WALKING_STEP_SAMPLES, 0, 0, 1);
    finalize_scenario_header(file, header_pos, "sine_noise", sample_count);
    fprintf(file, "\n");

    /* 3. Varying frequency. */
    rng_state = 0x13579BDFu;
    header_pos = write_scenario_header(file, "varying_frequency");
    sample_count = generate_walking(file, WALKING_STEP_COUNT, WALKING_STEP_SAMPLES, 1, 0, 0);
    finalize_scenario_header(file, header_pos, "varying_frequency", sample_count);
    fprintf(file, "\n");

    /* 4. Varying frequency + noise. */
    rng_state = 0x13579BDFu;
    header_pos = write_scenario_header(file, "varying_frequency_noise");
    sample_count = generate_walking(file, WALKING_STEP_COUNT, WALKING_STEP_SAMPLES, 1, 0, 1);
    finalize_scenario_header(file, header_pos, "varying_frequency_noise", sample_count);
    fprintf(file, "\n");

    /* 5. Varying frequency + amplitude. */
    rng_state = 0x13579BDFu;
    header_pos = write_scenario_header(file, "varying_frequency_amplitude");
    sample_count = generate_walking(file, WALKING_STEP_COUNT, WALKING_STEP_SAMPLES, 1, 1, 0);
    finalize_scenario_header(file, header_pos, "varying_frequency_amplitude", sample_count);
    fprintf(file, "\n");

    /* 6. Varying frequency + amplitude + noise. */
    rng_state = 0x13579BDFu;
    header_pos = write_scenario_header(file, "varying_frequency_amplitude_noise");
    sample_count = generate_walking(file, WALKING_STEP_COUNT, WALKING_STEP_SAMPLES, 1, 1, 1);
    finalize_scenario_header(file, header_pos, "varying_frequency_amplitude_noise", sample_count);
    fprintf(file, "\n");

    /* 7. Stillness -> walking -> stillness. */
    rng_state = 0x13579BDFu;
    header_pos = write_scenario_header(file, "stillness_steps_stillness");
    sample_count = 0;
    sample_count += generate_stillness(file, STILLNESS_SAMPLES);
    sample_count += generate_walking(file, WALKING_STEP_COUNT, WALKING_STEP_SAMPLES, 0, 0, 1);
    sample_count += generate_stillness(file, STILLNESS_SAMPLES);
    finalize_scenario_header(file, header_pos, "stillness_steps_stillness", sample_count);
    fprintf(file, "\n");

    /* 8. Walking -> gap -> walking. */
    rng_state = 0x13579BDFu;
    header_pos = write_scenario_header(file, "steps_gap_steps");
    sample_count = generate_steps_gap_steps(file);
    finalize_scenario_header(file, header_pos, "steps_gap_steps", sample_count);
    fprintf(file, "\n");

    /* 9. Walking with one deliberately weak step. */
    rng_state = 0x13579BDFu;
    header_pos = write_scenario_header(file, "single_missed_peak");
    sample_count = generate_single_weak_peak(file);
    finalize_scenario_header(file, header_pos, "single_missed_peak", sample_count);
    fprintf(file, "\n");

    /* 10. Discontinuous walking. */
    rng_state = 0x13579BDFu;
    header_pos = write_scenario_header(file, "discontinuous_steps");
    sample_count = generate_discontinuous_walking(file);
    finalize_scenario_header(file, header_pos, "discontinuous_steps", sample_count);

    fclose(file);

    printf("Generated %s\n", OUTPUT_FILE);
    printf("Accelerometer scale: %.0f LSB/g\n", LSB_PER_G);
    printf("Scaling factor: %.7f g/LSB\n", 1.0f / LSB_PER_G);

    return EXIT_SUCCESS;
}

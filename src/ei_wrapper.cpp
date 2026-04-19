#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "model-parameters/model_metadata.h"
#include "ei_wrapper.h"

extern "C" int ei_classify(const float *features, size_t n,
                           int *out_label, float *out_score,
                           float *out_anomaly)
{
    if (n != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE)
        return -1;

    ei::signal_t signal;
    numpy::signal_from_buffer(features, n, &signal);

    ei_impulse_result_t result = {0};
    EI_IMPULSE_ERROR r = run_classifier(&signal, &result, false);
    if (r != EI_IMPULSE_OK)
        return -2;

    int best = 0;
    float best_v = result.classification[0].value;
    for (int i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++)
    {
        if (result.classification[i].value > best_v)
        {
            best_v = result.classification[i].value;
            best = i;
        }
    }
    *out_label = best;
    *out_score = best_v;

#if EI_CLASSIFIER_HAS_ANOMALY
    *out_anomaly = result.anomaly;
#else
    *out_anomaly = 0.0f;
#endif
    return 0;
}
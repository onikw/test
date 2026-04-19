#ifndef EI_WRAPPER_H_
#define EI_WRAPPER_H_
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif

    int ei_classify(const float *features, size_t n,
                    int *out_label, float *out_score,
                    float *out_anomaly);

#ifdef __cplusplus
}
#endif
#endif
#ifndef DHT22_H
#define DHT22_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
int dht22_read(float *temp, float *humi);

#ifdef __cplusplus
}
#endif

#endif // DHT22_H

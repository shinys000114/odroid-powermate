#ifndef MAIN_SERVICE_DATALOG_H_
#define MAIN_SERVICE_DATALOG_H_

#include <stdint.h>

#define NUM_CHANNELS 3

typedef struct
{
    float voltage;
    float current;
    float power;
} channel_data_t;

void datalog_init(void);
void datalog_add(uint32_t timestamp, const channel_data_t* channel_data);
const char* datalog_get_path(void);

#endif /* MAIN_SERVICE_DATALOG_H_ */

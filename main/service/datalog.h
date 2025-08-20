#ifndef MAIN_SERVICE_DATALOG_H_
#define MAIN_SERVICE_DATALOG_H_

#include <stdint.h>

void datalog_init(void);
void datalog_add(uint32_t timestamp, float voltage, float current, float power);
const char* datalog_get_path(void);

#endif /* MAIN_SERVICE_DATALOG_H_ */

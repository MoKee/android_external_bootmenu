#ifndef BATT_CPCAP_H
#define BATT_CPCAP_H

#include "common.h"

#define SUCCEED(...) \
  if (! (__VA_ARGS__)) { \
    LOGI("%s:%d expression '%s' failed: %s", __func__, __LINE__, #__VA_ARGS__, strerror(errno)); \
    if (cpcap_fd > 0) { close(cpcap_fd); } \
    return 1; \
  }


int cpcap_batt_percent(void);


#endif

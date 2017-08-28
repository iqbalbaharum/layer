

#ifndef LAYERT_H
#define LAYERT_H

#include "string.h"

#define BASE "base"
#define DYNAMIC "dynamic"
#define DATA "data"
//
#define PUBLIC "PUBLIC"
#define PRIVATE "PRIVATE"

enum LAYER_TYPE {
  BASE_LAYER,
  DYNAMIC_LAYER,
  DATA_LAYER
};

// Layer access
enum ACCESS_LAYER {
  AC_PUBLIC,
  AC_PRIVATE
};

#endif

#ifdef USE_FULL_ASSERT
#include "main.h"

#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))

void assert_failed(uint8_t *file, uint32_t line);
#endif

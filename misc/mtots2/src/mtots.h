#ifndef mtots_h
#define mtots_h

#include "mtots1err.h"

typedef struct Mtots Mtots;

Mtots *newMtots(void);

void releaseMtots(Mtots *mtots);

Status runMtotsFile(Mtots *mtots, const char *filePath);

#endif /*mtots_h*/

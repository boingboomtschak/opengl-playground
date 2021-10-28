// Color.h, Copyright (c) Jules Bloomenthal, 2018, all rights reserved

#ifndef COLOR_HDR
#define COLOR_HDR

#include "VecMat.h"

vec3 RGBfromHSV(vec3 hsv);

vec3 HSVfromRGB(vec3 rgb);

int NStockColors();

vec3 GetStockColor(int i);

void DrawStockColors(int screenX, int screenY, int blockWidth, int blockHeight,
                     int nColumns = 10, int margin = 0, vec3 *background = NULL);

*/
#endif

#include <ui_touch.h>

// Zweck: Liest Rohdaten vom Touch und mappt sie in Bildschirmkoordinaten.
// Side Effects: schreibt mapX/mapY; liest Touch-Hardware.
// Assumptions: Touch ist initialisiert; Kalibrierwerte passen zur Hardware.
bool readTouchMapped(int &mapX, int &mapY){
    TS_Point p = ts.getPoint();
    // Kalibrierwerte kommen aus hardware_map.h (TOUCH_X/Y_MIN/MAX, TOUCH_SWAP_XY).
    int rawX = TOUCH_SWAP_XY ? p.y : p.x;
    int rawY = TOUCH_SWAP_XY ? p.x : p.y;
    mapX = 320 - map(rawX, TOUCH_X_MIN, TOUCH_X_MAX, 0, 320);
    mapY = 240 - map(rawY, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, 240);
    // Koordinaten ausserhalb des Bildschirmbereichs = ungueltige Messung
    return (mapX >= 0 && mapX < 320 && mapY >= 0 && mapY < 240);
}

// Zweck: Ordnet eine Touch-Position einer logischen UI-Zone zu.
// Side Effects: keine.
// Assumptions: mapX/mapY sind in Screen-Koordinaten (0..320/0..240).
uint16_t getTipPositionFromXY(int mapX, int mapY){
    if(mapX < 80 && mapY < 80){
      return UL;
    }else if(mapX > 240 && mapY < 80){
      return UR;
    }else if(mapX < 80 && mapY > 160){
      return LL;
    }else if(mapX > 240 && mapY > 160){
      return LR;
    }else if(mapX >= 75 && mapX < 125 && mapY >= 60 && mapY < 110){
      return P1U;
    }else if(mapX >= 75 && mapX < 125 && mapY >= 130 && mapY < 180){
      return P1L;
    }else if(mapX >= 115 && mapX < 165 && mapY >= 60 && mapY < 110){
      return P2U;
    }else if(mapX >= 115 && mapX < 165 && mapY >= 130 && mapY < 180){
      return P2L;
    }else if(mapX >= 155 && mapX < 205 && mapY >= 60 && mapY < 110){
      return P3U;
    }else if(mapX >= 155 && mapX < 205 && mapY >= 130 && mapY < 180){
      return P3L;
    }else if(mapX >= 195 && mapX < 245 && mapY >= 60 && mapY < 110){
      return P4U;
    }else if(mapX >= 195 && mapX < 245 && mapY >= 130 && mapY < 180){
      return P4L;
    }
    return CP;
}

// Zweck: Liest Touch und liefert die logische UI-Zone.
// Side Effects: liest Touch-Hardware.
// Assumptions: Touch ist initialisiert; readTouchMapped liefert gueltige Werte.
uint16_t getTipPosition(){
    int mapX = 0;
    int mapY = 0;
    if(!readTouchMapped(mapX, mapY)){
      return CP;
    }
    return getTipPositionFromXY(mapX, mapY);
}

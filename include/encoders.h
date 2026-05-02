#pragma once

void setupEncoders();
void handleEncoders();
int  getPitchItvlCursor();   // aktuell selektiertes Intervall (0..6) fuer Enc3
int  getPitchBoxCursor();    // 0=Scale, 1=Root, 2=Spread (Enc1 browse cursor)
bool getPitchBoxEditMode();  // Enc1 edit mode aktiv

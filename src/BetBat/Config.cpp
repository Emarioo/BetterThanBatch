#include "BetBat/Config.h"

static int s_activelogs=0;
void SetLog(int type, bool active){
    if(active) s_activelogs |= type;
    else s_activelogs &= ~type;
}
bool GetLog(int type){
    return s_activelogs&type;
}
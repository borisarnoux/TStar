
#include <misc.h>

extern "C" {
    __thread int current_color = 0;




void set_current_color( int color ) {
        current_color = color;
}

}

#include "gfx2d.h"

Dimension height;
Dimension width;

Position x = 200, y = 200, i = 1, j = 1;
Screen screen;

void handler(void *a, void *b)
{
    draw_circle(x, y, 20, 0);
    if (y == screen.height - 20)
        j = -2;
    else if (y == 20)
        j = 2;
    if (x == screen.width - 20)
        i = -2;
    else if (x == 20)
        i = 2;
    x += i;
    y += j;
    draw_circle(x, y, 20, 0xFFFF);
}

int main()
{
    gfx2d_init();
    screen = get_screen();
    create_timer((efi_event_notify_t)handler, 166666);
    while (1)
    {
    }
}

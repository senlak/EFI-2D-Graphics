#include <uefi.h>

typedef uint64_t Position, Dimension;
typedef uint32_t Pixel;
typedef struct
{
    Pixel *data;
    Dimension height;
    Dimension width;
} Image;

typedef struct
{
    Dimension height;
    Dimension width;
} Screen;

typedef const char *string;

void gfx2d_init();
Screen get_screen();
void draw_pixel(Position x, Position y, Pixel pixel);
void draw_line(Position x0, Position y0, Position x1, Position y1, Pixel pixel);
void draw_rect(Position x, Position y, Dimension height, Dimension width, Pixel pixel);
void draw_circle(Position x, Position y, Dimension radius, Pixel pixel);
Image load_image(string path);
void load_font(string path, Pixel pixel);
void draw_image(Position x, Position y, Image image);
void draw_text(int x, int y, string s);
void create_timer(efi_event_notify_t handler,uint64_t count);

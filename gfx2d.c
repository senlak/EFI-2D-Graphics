#define STB_IMAGE_IMPLEMENTATION
#define SSFN_IMPLEMENTATION
#define SSFN_MAXLINES 4096
#include "gfx2d.h"
#include "stb_image.h"
#include "ssfn.h"

ssfn_buf_t dst = {0};
ssfn_t ctx = {0};
efi_event_t periodic;
efi_gop_t *gop = NULL;

void gfx2d_init()
{
    efi_status_t status;
    efi_guid_t gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    status = BS->LocateProtocol(&gopGuid, NULL, (void **)&gop);
    if (!EFI_ERROR(status) && gop)
    {
        status = gop->SetMode(gop, 0);
        ST->ConOut->Reset(ST->ConOut, 0);
        ST->StdErr->Reset(ST->StdErr, 0);
        if (EFI_ERROR(status))
        {
            printf("unable to set video mode\n");
            return;
        }
    }
    else
    {
        printf("unable to get graphics output protocol\n");
        return;
    }
}

Screen get_screen()
{
    Screen screen;
    screen.height = gop->Mode->Information->VerticalResolution;
    screen.width = gop->Mode->Information->HorizontalResolution;
    return screen;
}

void draw_pixel(Position x, Position y, Pixel pixel)
{
    *((uint32_t *)(gop->Mode->FrameBufferBase + 4 * gop->Mode->Information->PixelsPerScanLine * y + 4 * x)) = pixel;
}

void swap(Position *x, Position *y)
{
    Position tmp = *x;
    *x = *y;
    *y = tmp;
}

void draw_line(Position x0, Position y0, Position x1, Position y1, Pixel pixel)
{
    if (x0 > x1 || y0 > y1)
    {
        swap(&x0, &x1);
        swap(&y0, &y1);
    }
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2;

    while (draw_pixel(x0, y0, pixel), x0 != x1 || y0 != y1)
    {
        int e2 = err;
        if (e2 > -dx)
        {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy)
        {
            err += dx;
            y0 += sy;
        }
    }
}

void draw_rect(Position x, Position y, Dimension height, Dimension width, Pixel pixel)
{
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            draw_pixel(j + x, height + i + y, pixel);
        }
    }
}

void _draw_circle(int xc, int yc, int x, int y, int pixel)
{
    draw_line(xc - x, yc + y, xc + x, yc + y, pixel);
    draw_line(xc - x, yc - y, xc + x, yc - y, pixel);
    draw_line(xc - y, yc + x, xc + y, yc + x, pixel);
    draw_line(xc - y, yc - x, xc + y, yc - x, pixel);
}

void draw_circle(Position xc, Position yc, Dimension r, Pixel pixel)
{
    int x = 0, y = r;
    int d = 3 - 2 * r;
    _draw_circle(xc, yc, x, y, pixel);
    while (y >= x)
    {
        x++;
        if (d > 0)
        {
            y--;
            d = d + 4 * (x - y) + 10;
        }
        else
            d = d + 4 * x + 6;
        _draw_circle(xc, yc, x, y, pixel);
    }
}

Image load_image(string path)
{
    efi_status_t status;
    FILE *f;
    unsigned char *buff;
    Image image;
    int w, h, l;
    long int size;
    stbi__context s;
    stbi__result_info ri;

    if ((f = fopen(path, "r")))
    {
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fseek(f, 0, SEEK_SET);
        buff = (unsigned char *)malloc(size);
        if (!buff)
        {
            printf("unable to allocate memory\n");
            return image;
        }
        fread(buff, size, 1, f);
        fclose(f);
        ri.bits_per_channel = 8;
        s.read_from_callbacks = 0;
        s.img_buffer = s.img_buffer_original = buff;
        s.img_buffer_end = s.img_buffer_original_end = buff + size;
        image.data = (uint32_t *)stbi__png_load(&s, &w, &h, &l, 4, &ri);
        if (!image.data)
        {
            printf("Unable to decode png: %s\n", stbi__g_failure_reason);
            return image;
        }
    }
    else
    {
        printf("Unable to load image\n");
        return image;
    }

    if (gop->Mode->Information->PixelFormat == PixelBlueGreenRedReserved8BitPerColor ||
        (gop->Mode->Information->PixelFormat == PixelBitMask && gop->Mode->Information->PixelInformation.BlueMask != 0xff0000))
    {
        for (l = 0; l < w * h; l++)
            image.data[l] = ((image.data[l] & 0xff) << 16) | (image.data[l] & 0xff00) | ((image.data[l] >> 16) & 0xff);
    }
    image.height = h;
    image.width = w;
    free(buff);
    return image;
}

void draw_image(Position x, Position y, Image image)
{
    gop->Blt(gop, image.data, EfiBltBufferToVideo, 0, 0, (gop->Mode->Information->HorizontalResolution - image.width) / 2,
             (gop->Mode->Information->VerticalResolution - image.height) / 2, image.width, image.height, 0);
}

void load_font(string path, Pixel pixel)
{
    efi_status_t status;
    FILE *f;
    ssfn_font_t *font;
    long int size;

    /* load font */
    if ((f = fopen(path, "r")))
    {
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fseek(f, 0, SEEK_SET);
        font = (ssfn_font_t *)malloc(size + 1);
        if (!font)
        {
            printf("unable to allocate memory\n");
            return;
        }
        fread(font, size, 1, f);
        fclose(f);
        ssfn_load(&ctx, font);
    }
    else
    {
        printf("Unable to load font\n");
        return;
    }

    /* set up destination buffer */
    dst.ptr = (unsigned char *)gop->Mode->FrameBufferBase;
    dst.w = gop->Mode->Information->HorizontalResolution;
    dst.h = gop->Mode->Information->VerticalResolution;
    dst.p = sizeof(unsigned int) * gop->Mode->Information->PixelsPerScanLine;
    dst.fg = 0xFFFFFFFF;

    /* select typeface to use */
    ssfn_select(&ctx, SSFN_FAMILY_ANY, NULL, SSFN_STYLE_REGULAR | SSFN_STYLE_NOCACHE, 40);
}

void draw_text(int x, int y, string s)
{
    int ret;
    dst.x = x;
    dst.y = y;
    while ((ret = ssfn_render(&ctx, &dst, s)) > 0)
        s += ret;
}

void create_timer(efi_event_notify_t handler, uint64_t count)
{
    gBS->CreateEvent(
        EVT_TIMER | EVT_NOTIFY_SIGNAL,
        TPL_NOTIFY,
        handler,
        NULL,
        &periodic);
    gBS->SetTimer(periodic, TimerPeriodic, count);
}
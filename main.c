#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <curl/curl.h>
#include <jpeglib.h>

#define MAP_SIZE 960
#define WORLD_HALF 3000
#define FINAL_W 1080
#define FINAL_H 1080
#define MARKER_SIZE 15
#define STROKE_WIDTH 3
#define ZOOM_FACTOR 38

#define TOKEN "YOUR_TOKEN_HERE"
#define CHAT_ID "YOUR_CHAT_ID"

typedef unsigned char u8;

u8* load_jpeg(const char* filename, int* width, int* height) {
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, file);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    *width = cinfo.output_width;
    *height = cinfo.output_height;
    int channels = cinfo.output_components;

    u8* data = malloc((*width) * (*height) * channels);
    while (cinfo.output_scanline < cinfo.output_height) {
        u8* rowptr = data + cinfo.output_scanline * (*width) * channels;
        jpeg_read_scanlines(&cinfo, &rowptr, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(file);

    return data;
}

void save_jpeg(const char* filename, u8* data, int w, int h) {
    FILE* file = fopen(filename, "wb");

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, file);

    cinfo.image_width = w;
    cinfo.image_height = h;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 95, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        u8* rowptr = &data[cinfo.next_scanline * w * 3];
        jpeg_write_scanlines(&cinfo, &rowptr, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(file);
}

void draw_cross(u8* img, int w, int h) {
    int cx = w / 2;
    int cy = h / 2;

    for (int i = -MARKER_SIZE; i <= MARKER_SIZE; i++) {
        int x1 = cx + i;
        int y1 = cy + i;
        int x2 = cx + i;
        int y2 = cy - i;

        if (x1 >= 0 && x1 < w && y1 >= 0 && y1 < h) {
            u8* p = &img[(y1*w + x1)*3];
            p[0] = 255; p[1] = 0; p[2] = 0;
        }

        if (x2 >= 0 && x2 < w && y2 >= 0 && y2 < h) {
            u8* p = &img[(y2*w + x2)*3];
            p[0] = 255; p[1] = 0; p[2] = 0;
        }
    }
}

void send_telegram(const char* filename) {
    CURL *curl = curl_easy_init();
    if (!curl) return;

    char url[512];
    snprintf(url, sizeof(url),
        "https://api.telegram.org/bot%s/sendPhoto", TOKEN);

    curl_mime *mime;
    curl_mimepart *part;

    mime = curl_mime_init(curl);

    part = curl_mime_addpart(mime);
    curl_mime_name(part, "chat_id");
    curl_mime_data(part, CHAT_ID, CURL_ZERO_TERMINATED);

    part = curl_mime_addpart(mime);
    curl_mime_name(part, "photo");
    curl_mime_filedata(part, filename);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    curl_easy_perform(curl);

    curl_mime_free(mime);
    curl_easy_cleanup(curl);
}

int main(int argc, char** argv) {
    if (argc < 4) {
        printf("Usage: %s x y z\n", argv[0]);
        return 1;
    }

    double x = atof(argv[1]);
    double y = atof(argv[2]);

    int w, h;
    u8* map = load_jpeg("map.jpg", &w, &h);
    if (!map) {
        printf("Failed to load map\n");
        return 1;
    }

    u8* final = calloc(FINAL_W * FINAL_H * 3, 1);

    // Просто масштабируем всю карту (упрощённая версия)
    for (int j = 0; j < FINAL_H; j++) {
        for (int i = 0; i < FINAL_W; i++) {
            int src_x = i * w / FINAL_W;
            int src_y = j * h / FINAL_H;

            memcpy(
                &final[(j*FINAL_W + i)*3],
                &map[(src_y*w + src_x)*3],
                3
            );
        }
    }

    draw_cross(final, FINAL_W, FINAL_H);

    save_jpeg("result.jpg", final, FINAL_W, FINAL_H);

    send_telegram("result.jpg");

    free(map);
    free(final);

    printf("Done\n");
    return 0;
}
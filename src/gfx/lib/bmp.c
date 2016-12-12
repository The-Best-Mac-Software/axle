#include "bmp.h"
#include <std/std.h>
#include <kernel/util/vfs/fs.h>
#include "gfx.h"
#include <user/programs/jpeg.h>

void bmp_teardown(Bmp* bmp) {
	if (!bmp) return;

	layer_teardown(bmp->layer);
	kfree(bmp);
}

Bmp* create_bmp(Rect frame, ca_layer* layer) {
	if (!layer) return NULL;

	Bmp* bmp = (Bmp*)kmalloc(sizeof(Bmp));
	bmp->frame = frame;
	bmp->layer = layer;
	bmp->needs_redraw = 1;
	return bmp;
}

Bmp* load_bmp(Rect frame, char* filename) {
	FILE* file = fopen(filename, (char*)"");
	if (!file) {
		printf_err("File %s not found! Not loading BMP", filename);
		return NULL;
	}

	unsigned char header[54];
	fread(header, sizeof(char), 54, file);

	//get width and height from header
	int width = *(int*)&header[18];
	int height = *(int*)&header[22];
	printf_info("loading BMP with dimensions (%d,%d)", width, height);

	int bpp = gfx_bpp();
	ca_layer* layer = create_layer(size_make(width, height));
	//image is upside down in memory so build array from bottom up
	for (int i = width * height; i >= 0; i--) {
		int idx = i * bpp;
		//we process 3 bytes at a time because image is stored in BGR, we need RGB
		layer->raw[idx + 0] = fgetc(file);
		layer->raw[idx + 1] = fgetc(file);
		layer->raw[idx + 2] = fgetc(file);
		//fourth byte is for alpha channel if we used 32bit BMPs
		//we only use 24bit, so don't try to read it
		//fgetc(file);
	}

	Bmp* bmp = create_bmp(frame, layer);
	return bmp;
}

static bool done_jpeg_init = false;
Bmp* load_jpeg(Rect frame, char* filename) {
	if (!done_jpeg_init) {
		njInit();
		done_jpeg_init = true;
	}

	FILE* file = fopen(filename, (char*)"");
	if (!file) {
		printf_err("File %s not found! Not loading JPEG", filename);
		return NULL;
	}

	char* buf = kmalloc(ftell(file));
	fread(buf, sizeof(char), ftell(file), file);
	njDecode(buf, ftell(file));
	kfree(buf);

	int width = njGetWidth();
	int height = njGetHeight();
	printf_info("decoding JPEG with dimensions (%d,%d}", width, height);

	int bpp = gfx_bpp();
	ca_layer* layer = create_layer(size_make(width, height));
	unsigned char* decoded = njGetImage();

	memcpy(layer->raw, decoded, width * height * bpp);

	Bmp* bmp = create_bmp(frame, layer);
	return bmp;
}

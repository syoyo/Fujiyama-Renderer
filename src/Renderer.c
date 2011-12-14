/*
Copyright (c) 2011-2012 Hiroshi Tsubokawa
See LICENSE and README
*/

#include "Renderer.h"
#include "FrameBuffer.h"
#include "Progress.h"
#include "Sampler.h"
#include "Camera.h"
#include "Filter.h"
#include "Tiler.h"
#include "Timer.h"
#include "Ray.h"
#include "Box.h"
#include "SL.h"

#include "Vector.h"
#include "Property.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct Renderer {
	struct Camera *camera;
	struct FrameBuffer *framebuffers;
	struct ObjectInstance *object;
	struct ObjectGroup *target_objects;

	int resolution[2];
	int render_region[4];
	int pixelsamples[2];
	int tilesize[2];
	float filterwidth[2];
};

static int prepare_render(struct Renderer *renderer);
static int render_scene(struct Renderer *renderer);

struct Renderer *RdrNew(void)
{
	struct Renderer *renderer;

	renderer = (struct Renderer *) malloc(sizeof(struct Renderer));
	if (renderer == NULL)
		return NULL;

	renderer->camera = NULL;
	renderer->framebuffers = NULL;
	renderer->object = NULL;

	RdrSetResolution(renderer, 320, 240);
	RdrSetPixelSamples(renderer, 3, 3);
	RdrSetTileSize(renderer, 64, 64);
	RdrSetFilterWidth(renderer, 2, 2);

	return renderer;
}

void RdrFree(struct Renderer *renderer)
{
	if (renderer == NULL)
		return;
	free(renderer);
}

void RdrSetResolution(struct Renderer *renderer, int xres, int yres)
{
	assert(xres > 0);
	assert(yres > 0);
	renderer->resolution[0] = xres;
	renderer->resolution[1] = yres;

	RdrSetRenderRegion(renderer, 0, 0, xres, yres);
}

void RdrSetRenderRegion(struct Renderer *renderer, int xmin, int ymin, int xmax, int ymax)
{
	assert(xmin >= 0);
	assert(ymin >= 0);
	assert(xmax >= 0);
	assert(ymax >= 0);
	assert(xmin < xmax);
	assert(ymin < ymax);

	BOX2_SET(renderer->render_region, xmin, ymin, xmax, ymax);
}

void RdrSetPixelSamples(struct Renderer *renderer, int xrate, int yrate)
{
	assert(xrate > 0);
	assert(yrate > 0);
	renderer->pixelsamples[0] = xrate;
	renderer->pixelsamples[1] = yrate;
}

void RdrSetTileSize(struct Renderer *renderer, int xtilesize, int ytilesize)
{
	assert(xtilesize > 0);
	assert(ytilesize > 0);
	renderer->tilesize[0] = xtilesize;
	renderer->tilesize[1] = ytilesize;
}

void RdrSetFilterWidth(struct Renderer *renderer, float xfwidth, float yfwidth)
{
	assert(xfwidth > 0);
	assert(yfwidth > 0);
	renderer->filterwidth[0] = xfwidth;
	renderer->filterwidth[1] = yfwidth;
}

void RdrSetCamera(struct Renderer *renderer, struct Camera *cam)
{
	assert(cam != NULL);
	renderer->camera = cam;
}

void RdrSetFrameBuffers(struct Renderer *renderer, struct FrameBuffer *fb)
{
	assert(fb != NULL);
	renderer->framebuffers = fb;
}

void RdrSetTargetObjects(struct Renderer *renderer, struct ObjectGroup *grp)
{
	assert(grp != NULL);
	renderer->target_objects = grp;
}

int RdrRender(struct Renderer *renderer)
{
	int err;

	err = prepare_render(renderer);
	if (err) {
		/* TODO error handling */
		return -1;
	}

	err = render_scene(renderer);
	if (err) {
		/* TODO error handling */
		return -1;
	}

	return 0;
}

static int prepare_render(struct Renderer *renderer)
{
	int xres, yres;
	struct Camera *cam;
	struct FrameBuffer *fb;

	cam = renderer->camera;
	fb  = renderer->framebuffers;

	if (cam == NULL) {
		/* TODO error handling */
		return -1;
	}
	if (fb == NULL) {
		/* TODO error handling */
		return -1;
	}

	/* Preparing Camera */
	xres = renderer->resolution[0];
	yres = renderer->resolution[1];
	CamSetAspect(cam, xres/(double)yres);

	/* Prepare framebuffers */
	FbResize(fb, xres, yres, 4);

	return 0;
}

static int render_scene(struct Renderer *renderer)
{
	struct Camera *cam = renderer->camera;
	struct FrameBuffer *fb = renderer->framebuffers;
	struct ObjectGroup *target_objects = renderer->target_objects;

	/* context */
	struct TraceContext cxt;

	int x, y;
	struct Sample *pixelsmps = NULL;
	struct Sample *smp;
	struct Ray ray;
	struct Timer t;
	struct Progress *progress;

	/* aux */
	struct Sampler *sampler = NULL;
	struct Filter *filter = NULL;
	struct Tiler *tiler = NULL;
	struct Tile *tile = NULL;

	int region[4] = {0};

	const int xres = renderer->resolution[0];
	const int yres = renderer->resolution[1];
	const int xrate = renderer->pixelsamples[0];
	const int yrate = renderer->pixelsamples[1];
	const double xfwidth = renderer->filterwidth[0];
	const double yfwidth = renderer->filterwidth[1];
	const int xtilesize = renderer->tilesize[0];
	const int ytilesize = renderer->tilesize[1];

	/* Progress */
	progress = PrgNew();
	if (progress == NULL)
		goto fatal_error;

	/* Sampler */
	sampler = SmpNew(xres, yres, xrate, yrate, xfwidth, yfwidth);
	if (sampler == NULL)
		goto fatal_error;

	/* Filter */
	filter = FltNew(FLT_GAUSSIAN, xfwidth, yfwidth);
	if (filter == NULL)
		goto fatal_error;

	/* Tiler */
	tiler = TlrNew(xres, yres, xtilesize, ytilesize);
	if (tiler == NULL)
		goto fatal_error;

	/* context */
	cxt = SlCameraContext(target_objects);
/*
	cxt.max_reflect_depth = 0;
	cxt.max_refract_depth = 0;
	cxt.cast_shadow = 0;
*/

	/* region */
	BOX2_COPY(region, renderer->render_region);
	TlrGenerateTiles(tiler, region[0], region[1], region[2], region[3]);

	/* samples for a pixel */
	pixelsmps = SmpAllocatePixelSamples(sampler);

	/* Run sampling */
	TimerStart(&t);
	printf("Rendering ...\n");
	while ((tile = TlrGetNextTile(tiler)) != NULL) {
		int pixel_bounds[4];
		int err;
		pixel_bounds[0] = tile->xmin;
		pixel_bounds[1] = tile->ymin;
		pixel_bounds[2] = tile->xmax;
		pixel_bounds[3] = tile->ymax;
		err = SmpGenerateSamples(sampler, pixel_bounds);
		if (err) {
			goto fatal_error;
		}

		PrgStart(progress, SmpGetSampleCount(sampler));

		while ((smp = SmpGetNextSample(sampler)) != NULL) {
			int hit;
			float Cs[3] = {0};

			CamGetRay(cam, smp->uv, &ray);

			hit = SlTrace(&cxt, ray.orig, ray.dir, ray.tmin, ray.tmax, Cs);
			VEC4_SET(smp->data, Cs[0], Cs[1], Cs[2], hit);

			PrgIncrement(progress);
		}

		for (y = pixel_bounds[1]; y < pixel_bounds[3]; y++) {
			for (x = pixel_bounds[0]; x < pixel_bounds[2]; x++) {
				const int NSAMPLES = SmpGetSampleCountForPixel(sampler);
				float *dst = NULL;
				float pixel[4] = {0};
				float sum = 0;
				int i;

				SmpGetPixelSamples(sampler, pixelsmps, x, y);

				for (i = 0; i < NSAMPLES; i++) {
					struct Sample *sample = pixelsmps + i;
					double filtx, filty;
					double wgt;

					filtx = xres * sample->uv[0] - (x+.5);
					filty = yres * (1-sample->uv[1]) - (y+.5);
					wgt = FltEvaluate(filter, filtx, filty);
					pixel[0] += wgt * sample->data[0];
					pixel[1] += wgt * sample->data[1];
					pixel[2] += wgt * sample->data[2];
					pixel[3] += wgt * sample->data[3];
					sum += wgt;
				}
				VEC4_DIV(pixel, sum);

				dst = FbGetWritable(fb, x, y, 0);
				VEC4_COPY(dst, pixel);
			}
		}
		printf(" Tile Done: %d/%d (%d %%)\n",
				tile->id+1,
				TlrGetTileCount(tiler),
				(int ) ((tile->id+1) / (double )TlrGetTileCount(tiler) * 100));
		PrgDone(progress);
	}
	printf("Done: %.5f sec\n", TimerElapsed(&t));

	/* clean up */
	SmpFreePixelSamples(pixelsmps);
	PrgFree(progress);
	SmpFree(sampler);
	FltFree(filter);
	TlrFree(tiler);
	return 0;

fatal_error:
	/* clean up (may be useless anymore...) */
	SmpFreePixelSamples(pixelsmps);
	PrgFree(progress);
	SmpFree(sampler);
	FltFree(filter);
	TlrFree(tiler);
	return -1;
}

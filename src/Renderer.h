/*
Copyright (c) 2011-2012 Hiroshi Tsubokawa
See LICENSE and README
*/

#ifndef RENDERER_H
#define RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

struct Renderer;
struct FrameBuffer;
struct ObjectGroup;
struct Camera;

extern struct Renderer *RdrNew(void);
extern void RdrFree(struct Renderer *renderer);

extern void RdrSetResolution(struct Renderer *renderer, int xres, int yres);
extern void RdrSetRenderRegion(struct Renderer *renderer, int xmin, int ymin, int xmax, int ymax);
extern void RdrSetPixelSamples(struct Renderer *renderer, int xrate, int yrate);
extern void RdrSetTileSize(struct Renderer *renderer, int xtilesize, int ytilesize);
extern void RdrSetFilterWidth(struct Renderer *renderer, float xfwidth, float yfwidth);
extern void RdrSetCamera(struct Renderer *renderer, struct Camera *cam);
extern void RdrSetFrameBuffers(struct Renderer *renderer, struct FrameBuffer *fb);
extern void RdrSetTargetObjects(struct Renderer *renderer, struct ObjectGroup *grp);

extern int RdrRender(struct Renderer *renderer);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* XXX_H */

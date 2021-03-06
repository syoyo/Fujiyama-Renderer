/*
Copyright (c) 2011-2012 Hiroshi Tsubokawa
See LICENSE and README
*/

#include "SceneInterface.h"
#include "FrameBufferIO.h"
#include "PrimitiveSet.h"
#include "CurveIO.h"
#include "MeshIO.h"
#include "Shader.h"
#include "Scene.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define GET_LAST_ADDED_ID(Type) (ScnGet##Type##Count(scene) - 1)

enum { TYPE_ID_OFFSET = 10000000 };

enum {
	SI_NOERR = 0,
	SI_ERR_BADTYPE,
	SI_ERR_FAILOPENPLG,
	SI_ERR_FAILLOAD,
	SI_ERR_FAILNEW,
	SI_ERR_NOMEM
};

enum {
	Type_Begin = 0,
	Type_ObjectInstance = 1,
	Type_Accelerator,
	Type_FrameBuffer,
	Type_Turbulence,
	Type_Procedure,
	Type_Renderer,
	Type_Texture,
	Type_Camera,
	Type_Plugin,
	Type_Shader,
	Type_Volume,
	Type_Curve,
	Type_Light,
	Type_Mesh,
	Type_End
};

struct Entry {
	int type;
	int index;
};

static struct Scene *scene = NULL;
static int si_errno = SI_NOERR;

static int is_valid_type(int type);
static ID encode_id(int type, int index);
static struct Entry decode_id(ID id);
static int create_implicit_groups(void);
static void set_errno(int err_no);
static Status status_of_error(int err);

/*
#include "property_list_include.c"
*/

static int set_property(const struct Entry *entry,
		const char *name, const struct PropertyValue *value);

static int get_property_list(const char *type_name,
		const char ***property_types,
		const char ***property_names,
		int *property_count);

/* Error interfaces */
int SiGetErrorNo(void)
{
	return si_errno;
}

const char *SiGetErrorMessage(int err_no)
{
	static const char *errmsg[] = {
		"",                   /* SI_NOERR */
		"bad arg",            /* SI_ERR_BADARG */
		"invalid entry type", /* SI_ERR_BADTYPE */
		"open plugin failed", /* SI_ERR_FAILOPENPLG */
		"load file failed",   /* SI_ERR_FAILLOAD */
		"new entry failed",   /* SI_ERR_FAILNEW */
		"no memory"           /* SI_ERR_NOMEM */
	};
	static const int nerrs = (int) sizeof(errmsg)/sizeof(errmsg[0]);

	if (err_no >= nerrs) {
		fprintf(stderr, "Logic error: err_no %d is out of range\n", err_no);
		abort();
	}
	return errmsg[err_no];
}

/* Plugin interfaces */
Status SiOpenPlugin(const char *filename)
{
	if (ScnOpenPlugin(scene, filename) == NULL) {
		set_errno(SI_ERR_FAILOPENPLG);
		return SI_FAIL;
	}

	set_errno(SI_NOERR);
	return SI_SUCCESS;
}

/* Scene interfaces */
Status SiOpenScene(void)
{
	scene = ScnNew();

	if (scene == NULL) {
		set_errno(SI_ERR_NOMEM);
		return SI_FAIL;
	}

	set_errno(SI_NOERR);
	return SI_SUCCESS;
}

Status SiCloseScene(void)
{
	ScnFree(scene);
    scene = NULL;

	set_errno(SI_NOERR);
	return SI_SUCCESS;
}

Status SiRenderScene(ID renderer)
{
	int err = 0;

	err = create_implicit_groups();
	if (err) {
		/* TODO error handling */
		return SI_FAIL;
	}

	/* TODO fix hard-coded renderer index */
	err = RdrRender(ScnGetRenderer(scene, 0));
	if (err) {
		/* TODO error handling */
		return SI_FAIL;
	}

	set_errno(SI_NOERR);
	return SI_SUCCESS;
}

Status SiSaveFrameBuffer(ID framebuffer, const char *filename)
{
	const struct Entry entry = decode_id(framebuffer);
	struct FrameBuffer *framebuffer_ptr = NULL;
	int err = 0;

	if (entry.type != Type_FrameBuffer)
		return SI_FAIL;

	framebuffer_ptr = ScnGetFrameBuffer(scene, entry.index);
	if (framebuffer_ptr == NULL)
		return SI_FAIL;

	err = FbSaveCroppedData(framebuffer_ptr, filename);
	if (err) {
		/* TODO error handling */
		return SI_FAIL;
	}

	set_errno(SI_NOERR);
	return SI_SUCCESS;
}

Status SiRunProcedure(ID procedure)
{
	const struct Entry entry = decode_id(procedure);
	struct Procedure *procedure_ptr = NULL;
	int err = 0;

	if (entry.type != Type_Procedure)
		return SI_FAIL;

	procedure_ptr = ScnGetProcedure(scene, entry.index);
	if (procedure_ptr == NULL)
		return SI_FAIL;

	err = PrcRun(procedure_ptr);
	if (err) {
		/* TODO error handling */
		return SI_FAIL;
	}

	set_errno(SI_NOERR);
	return SI_SUCCESS;
}

/* TODO change argument name accelerator */
ID SiNewObjectInstance(ID accelerator)
{
	const struct Entry entry = decode_id(accelerator);

	if (entry.type == Type_Accelerator) {
		struct Accelerator *acc = NULL;

		acc = ScnGetAccelerator(scene, entry.index);
		if (acc == NULL) {
			set_errno(SI_ERR_BADTYPE);
			return SI_BADID;
		}

		/* TODO come up with another way to pass acc */
		if (ScnNewObjectInstance(scene, acc) == NULL) {
			set_errno(SI_ERR_FAILNEW);
			return SI_BADID;
		}
	}
	else if (entry.type == Type_Volume) {
		struct ObjectInstance *object = NULL;
		struct Volume *volume = NULL;
		int err = 0;

		volume = ScnGetVolume(scene, entry.index);
		if (volume == NULL) {
			set_errno(SI_ERR_BADTYPE);
			return SI_BADID;
		}

		object = ScnNewObjectInstance(scene, NULL);
		if (object == NULL) {
			set_errno(SI_ERR_FAILNEW);
			return SI_BADID;
		}

		err = ObjSetVolume(object, volume);
		if (err) {
			set_errno(SI_ERR_FAILNEW);
			return SI_BADID;
		}

	}
	else {
		set_errno(SI_ERR_BADTYPE);
		return SI_BADID;
	}

	set_errno(SI_NOERR);
	return encode_id(Type_ObjectInstance, GET_LAST_ADDED_ID(ObjectInstance));
}

ID SiNewFrameBuffer(const char *arg)
{
	if (ScnNewFrameBuffer(scene) == NULL) {
		set_errno(SI_ERR_FAILNEW);
		return SI_BADID;
	}

	set_errno(SI_NOERR);
	return encode_id(Type_FrameBuffer, GET_LAST_ADDED_ID(FrameBuffer));
}

ID SiNewTurbulence(void)
{
	if (ScnNewTurbulence(scene) == NULL) {
		set_errno(SI_ERR_FAILNEW);
		return SI_BADID;
	}

	set_errno(SI_NOERR);
	return encode_id(Type_Turbulence, GET_LAST_ADDED_ID(Turbulence));
}

ID SiNewProcedure(const char *plugin_name)
{
	struct Plugin **plugins = ScnGetPluginList(scene);
	struct Plugin *found = NULL;
	const int N = (int) ScnGetPluginCount(scene);
	int i = 0;

	for (i = 0; i < N; i++) {
		if (strcmp(plugin_name, PlgGetName(plugins[i])) == 0) {
			found = plugins[i];
			break;
		}
	}
	if (found == NULL) {
		set_errno(SI_ERR_FAILNEW);
		return SI_BADID;
	}
	if (ScnNewProcedure(scene, found) == NULL) {
		set_errno(SI_ERR_FAILNEW);
		return SI_BADID;
	}

	set_errno(SI_NOERR);
	return encode_id(Type_Procedure, GET_LAST_ADDED_ID(Procedure));
}

ID SiNewRenderer(void)
{
	if (ScnNewRenderer(scene) == NULL) {
		set_errno(SI_ERR_FAILNEW);
		return SI_BADID;
	}

	set_errno(SI_NOERR);
	return encode_id(Type_Renderer, GET_LAST_ADDED_ID(Renderer));
}

ID SiNewTexture(const char *filename)
{
	struct Texture *tex = NULL;

	tex = ScnNewTexture(scene);
	if (tex == NULL) {
		set_errno(SI_ERR_FAILNEW);
		return SI_BADID;
	}
	if (TexLoadFile(tex, filename)) {
		set_errno(SI_ERR_FAILLOAD);
		return SI_FAIL;
	}

	set_errno(SI_NOERR);
	return encode_id(Type_Texture, GET_LAST_ADDED_ID(Texture));
}

ID SiNewCamera(const char *arg)
{
	if (ScnNewCamera(scene, arg) == NULL) {
		set_errno(SI_ERR_FAILNEW);
		return SI_BADID;
	}

	set_errno(SI_NOERR);
	return encode_id(Type_Camera, GET_LAST_ADDED_ID(Camera));
}

ID SiNewShader(const char *plugin_name)
{
	struct Plugin **plugins = ScnGetPluginList(scene);
	struct Plugin *found = NULL;
	const int N = (int) ScnGetPluginCount(scene);
	int i = 0;

	for (i = 0; i < N; i++) {
		if (strcmp(plugin_name, PlgGetName(plugins[i])) == 0) {
			found = plugins[i];
			break;
		}
	}
	if (found == NULL) {
		set_errno(SI_ERR_FAILNEW);
		return SI_BADID;
	}
	if (ScnNewShader(scene, found) == NULL) {
		set_errno(SI_ERR_FAILNEW);
		return SI_BADID;
	}

	set_errno(SI_NOERR);
	return encode_id(Type_Shader, GET_LAST_ADDED_ID(Shader));
}

ID SiNewVolume(void)
{
	struct Volume *volume = NULL;

	volume = ScnNewVolume(scene);
	if (volume == NULL) {
		set_errno(SI_ERR_FAILNEW);
		return SI_BADID;
	}

	set_errno(SI_NOERR);
	return encode_id(Type_Volume, GET_LAST_ADDED_ID(Volume));
}

ID SiNewCurve(const char *filename)
{
	struct Curve *curve = NULL;
	struct Accelerator *acc = NULL;
	struct PrimitiveSet primset;

	curve = ScnNewCurve(scene);
	if (curve == NULL) {
		set_errno(SI_ERR_FAILNEW);
		return SI_BADID;
	}
	if (CrvLoadFile(curve, filename)) {
		set_errno(SI_ERR_FAILLOAD);
		return SI_BADID;
	}

	acc = ScnNewAccelerator(scene, ACC_GRID);
	if (acc == NULL) {
		set_errno(SI_ERR_FAILNEW);
		return SI_BADID;
	}

	CrvGetPrimitiveSet(curve, &primset);
	AccSetPrimitiveSet(acc, &primset);

	set_errno(SI_NOERR);
	return encode_id(Type_Accelerator, GET_LAST_ADDED_ID(Accelerator));
}

ID SiNewLight(const char *arg)
{
	if (ScnNewLight(scene, arg) == NULL) {
		set_errno(SI_ERR_FAILNEW);
		return SI_BADID;
	}

	set_errno(SI_NOERR);
	return encode_id(Type_Light, GET_LAST_ADDED_ID(Light));
}

ID SiNewMesh(const char *filename)
{
	struct Mesh *mesh = NULL;
	struct Accelerator *acc = NULL;
	struct PrimitiveSet primset;

	mesh = ScnNewMesh(scene);
	if (mesh == NULL) {
		set_errno(SI_ERR_FAILNEW);
		return SI_BADID;
	}
	if (MshLoadFile(mesh, filename)) {
		set_errno(SI_ERR_FAILLOAD);
		return SI_BADID;
	}

	acc = ScnNewAccelerator(scene, ACC_GRID);
	if (acc == NULL) {
		set_errno(SI_ERR_FAILNEW);
		return SI_BADID;
	}

	MshGetPrimitiveSet(mesh, &primset);
	AccSetPrimitiveSet(acc, &primset);

	set_errno(SI_NOERR);
	return encode_id(Type_Accelerator, GET_LAST_ADDED_ID(Accelerator));
}

Status SiAssignShader(ID object, ID shader)
{
	struct ObjectInstance *object_ptr = NULL;
	struct Shader *shader_ptr = NULL;
	{
		const struct Entry entry = decode_id(object);

		if (entry.type != Type_ObjectInstance)
			return SI_FAIL;

		object_ptr = ScnGetObjectInstance(scene, entry.index);
		if (object_ptr == NULL)
			return SI_FAIL;
	}
	{
		const struct Entry entry = decode_id(shader);

		if (entry.type != Type_Shader)
			return SI_FAIL;

		shader_ptr = ScnGetShader(scene, entry.index);
		if (shader_ptr == NULL)
			return SI_FAIL;
	}

	ObjSetShader(object_ptr, shader_ptr);
	return SI_SUCCESS;
}

Status SiAssignTexture(ID shader, const char *prop_name, ID texture)
{
	struct PropertyValue value = InitPropValue();
	struct Shader *shader_ptr = NULL;
	struct Texture *texture_ptr = NULL;
	{
		const struct Entry entry = decode_id(shader);

		if (entry.type != Type_Shader)
			return SI_FAIL;

		shader_ptr = ScnGetShader(scene, entry.index);
		if (shader_ptr == NULL)
			return SI_FAIL;
	}
	{
		const struct Entry entry = decode_id(texture);

		if (entry.type != Type_Texture)
			return SI_FAIL;

		texture_ptr = ScnGetTexture(scene, entry.index);
		if (texture_ptr == NULL)
			return SI_FAIL;
	}

	value = PropTexture(texture_ptr);
	return ShdSetProperty(shader_ptr, prop_name, &value);
}

Status SiAssignCamera(ID renderer, ID camera)
{
	struct Renderer *renderer_ptr = NULL;
	struct Camera *camera_ptr = NULL;
	{
		const struct Entry entry = decode_id(renderer);

		if (entry.type != Type_Renderer)
			return SI_FAIL;

		renderer_ptr = ScnGetRenderer(scene, entry.index);
		if (renderer_ptr == NULL)
			return SI_FAIL;
	}
	{
		const struct Entry entry = decode_id(camera);

		if (entry.type != Type_Camera)
			return SI_FAIL;

		camera_ptr = ScnGetCamera(scene, entry.index);
		if (camera_ptr == NULL)
			return SI_FAIL;
	}

	RdrSetCamera(renderer_ptr, camera_ptr);
	return SI_SUCCESS;
}

Status SiAssignFrameBuffer(ID renderer, ID framebuffer)
{
	struct Renderer *renderer_ptr = NULL;
	struct FrameBuffer *framebuffer_ptr = NULL;
	{
		const struct Entry entry = decode_id(renderer);

		if (entry.type != Type_Renderer)
			return SI_FAIL;

		renderer_ptr = ScnGetRenderer(scene, entry.index);
		if (renderer_ptr == NULL)
			return SI_FAIL;
	}
	{
		const struct Entry entry = decode_id(framebuffer);

		if (entry.type != Type_FrameBuffer)
			return SI_FAIL;

		framebuffer_ptr = ScnGetFrameBuffer(scene, entry.index);
		if (framebuffer_ptr == NULL)
			return SI_FAIL;
	}

	RdrSetFrameBuffers(renderer_ptr, framebuffer_ptr);
	return SI_SUCCESS;
}

Status SiSetProperty1(ID id, const char *name, double v0)
{
	const struct Entry entry = decode_id(id);
	const struct PropertyValue value = PropScalar(v0);
	const int err = set_property(&entry, name, &value);

	return status_of_error(err);
}

Status SiSetProperty2(ID id, const char *name, double v0, double v1)
{
	const struct Entry entry = decode_id(id);
	const struct PropertyValue value = PropVector2(v0, v1);
	const int err = set_property(&entry, name, &value);

	return status_of_error(err);
}

Status SiSetProperty3(ID id, const char *name, double v0, double v1, double v2)
{
	const struct Entry entry = decode_id(id);
	const struct PropertyValue value = PropVector3(v0, v1, v2);
	const int err = set_property(&entry, name, &value);

	return status_of_error(err);
}

Status SiSetProperty4(ID id, const char *name, double v0, double v1, double v2, double v3)
{
	const struct Entry entry = decode_id(id);
	const struct PropertyValue value = PropVector4(v0, v1, v2, v3);
	const int err = set_property(&entry, name, &value);

	return status_of_error(err);
}

Status SiAssignTurbulence(ID id, const char *name, ID turbulence)
{
	const struct Entry entry = decode_id(id);
	const struct Entry turbulence_ent = decode_id(turbulence);
	struct PropertyValue value = InitPropValue();
	struct Turbulence *turbulence_ptr = NULL;
	int err = 0;

	if (turbulence_ent.type != Type_Turbulence)
		return SI_FAIL;

	turbulence_ptr = ScnGetTurbulence(scene, turbulence_ent.index);
	if (turbulence_ptr == NULL)
		return SI_FAIL;

	value = PropTurbulence(turbulence_ptr);
	err = set_property(&entry, name, &value);

	return status_of_error(err);
}

Status SiAssignVolume(ID id, const char *name, ID volume)
{
	const struct Entry entry = decode_id(id);
	const struct Entry volume_ent = decode_id(volume);
	struct PropertyValue value = InitPropValue();
	struct Volume *volume_ptr = NULL;
	int err = 0;

	if (volume_ent.type != Type_Volume)
		return SI_FAIL;

	volume_ptr = ScnGetVolume(scene, volume_ent.index);
	if (volume_ptr == NULL)
		return SI_FAIL;

	value = PropVolume(volume_ptr);
	err = set_property(&entry, name, &value);

	return status_of_error(err);
}

Status SiGetPropertyList(const char *type_name,
		const char ***property_types,
		const char ***property_names,
		int *property_count)
{
	return get_property_list(type_name, property_types, property_names, property_count);
}

static int is_valid_type(int type)
{
	return type > Type_Begin && type < Type_End;
}

static ID encode_id(int type, int index)
{
	return TYPE_ID_OFFSET * type + index;
}

static struct Entry decode_id(ID id)
{
	struct Entry entry = {-1, -1};
	const int type = id / TYPE_ID_OFFSET;

	if (!is_valid_type(type))
		return entry;

	entry.type = type;
	entry.index = id - (type * TYPE_ID_OFFSET);

	return entry;
}

static int create_implicit_groups(void)
{
	size_t i;
	struct ObjectGroup *all_objects;
	struct Renderer *renderer;

	all_objects = ScnNewObjectGroup(scene);
	if (all_objects == NULL) {
		set_errno(SI_ERR_FAILNEW);
		return SI_FAIL;
	}

	for (i = 0; i < ScnGetObjectInstanceCount(scene); i++) {
		struct ObjectInstance *obj = ScnGetObjectInstance(scene, i);
		ObjGroupAdd(all_objects, obj);
	}

	/* Preparing ObjectInstance */
	for (i = 0; i < ScnGetObjectInstanceCount(scene); i++) {
		struct ObjectInstance *obj = ScnGetObjectInstance(scene, i);
		const struct Light **lightlist = (const struct Light **) ScnGetLightList(scene);
		int nlights = ScnGetLightCount(scene);
		ObjSetLightList(obj, lightlist, nlights);

		if (ObjGetReflectTarget(obj) == NULL)
			ObjSetReflectTarget(obj, all_objects);

		if (ObjGetRefractTarget(obj) == NULL)
			ObjSetRefractTarget(obj, all_objects);
	}

	renderer = ScnGetRenderer(scene, 0);
	RdrSetTargetObjects(renderer, all_objects);

	return SI_SUCCESS;
}

static void set_errno(int err_no)
{
	si_errno = err_no;
}

static Status status_of_error(int err)
{
	if (err)
		return SI_FAIL;
	else
		return SI_SUCCESS;
}

/* property settings */
static int set_ObjectInstance_transform_order(void *self, const struct PropertyValue *value)
{
	ObjSetTransformOrder((struct ObjectInstance *) self, (int) value->vector[0]);
	return 0;
}

static int set_ObjectInstance_rotate_order(void *self, const struct PropertyValue *value)
{
	ObjSetRotateOrder((struct ObjectInstance *) self, (int) value->vector[0]);
	return 0;
}

static int set_ObjectInstance_translate(void *self, const struct PropertyValue *value)
{
	ObjSetTranslate((struct ObjectInstance *) self,
			value->vector[0], value->vector[1], value->vector[2]);
	return 0;
}

static int set_ObjectInstance_rotate(void *self, const struct PropertyValue *value)
{
	ObjSetRotate((struct ObjectInstance *) self,
			value->vector[0], value->vector[1], value->vector[2]);
	return 0;
}

static int set_ObjectInstance_scale(void *self, const struct PropertyValue *value)
{
	ObjSetScale((struct ObjectInstance *) self,
			value->vector[0], value->vector[1], value->vector[2]);
	return 0;
}

static int set_Turbulence_lacunarity(void *self, const struct PropertyValue *value)
{
	TrbSetLacunarity((struct Turbulence *) self, value->vector[0]);
	return 0;
}

static int set_Turbulence_gain(void *self, const struct PropertyValue *value)
{
	TrbSetGain((struct Turbulence *) self, value->vector[0]);
	return 0;
}

static int set_Turbulence_octaves(void *self, const struct PropertyValue *value)
{
	TrbSetOctaves((struct Turbulence *) self, (int) value->vector[0]);
	return 0;
}

static int set_Turbulence_amplitude(void *self, const struct PropertyValue *value)
{
	TrbSetAmplitude((struct Turbulence *) self,
			value->vector[0], value->vector[1], value->vector[2]);
	return 0;
}

static int set_Turbulence_frequency(void *self, const struct PropertyValue *value)
{
	TrbSetFrequency((struct Turbulence *) self,
			value->vector[0], value->vector[1], value->vector[2]);
	return 0;
}

static int set_Turbulence_offset(void *self, const struct PropertyValue *value)
{
	TrbSetOffset((struct Turbulence *) self,
			value->vector[0], value->vector[1], value->vector[2]);
	return 0;
}

static int set_Renderer_sample_jitter(void *self, const struct PropertyValue *value)
{
	RdrSetSampleJitter((struct Renderer *) self, value->vector[0]);
	return 0;
}

static int set_Renderer_cast_shadow(void *self, const struct PropertyValue *value)
{
	RdrSetShadowEnable((struct Renderer *) self, (int) value->vector[0]);
	return 0;
}

static int set_Renderer_max_reflect_depth(void *self, const struct PropertyValue *value)
{
	RdrSetMaxReflectDepth((struct Renderer *) self, (int) value->vector[0]);
	return 0;
}

static int set_Renderer_max_refract_depth(void *self, const struct PropertyValue *value)
{
	RdrSetMaxRefractDepth((struct Renderer *) self, (int) value->vector[0]);
	return 0;
}

static int set_Renderer_raymarch_step(void *self, const struct PropertyValue *value)
{
	RdrSetRaymarchStep((struct Renderer *) self, value->vector[0]);
	return 0;
}

static int set_Renderer_raymarch_shadow_step(void *self, const struct PropertyValue *value)
{
	RdrSetRaymarchShadowStep((struct Renderer *) self, value->vector[0]);
	return 0;
}

static int set_Renderer_raymarch_reflect_step(void *self, const struct PropertyValue *value)
{
	RdrSetRaymarchReflectStep((struct Renderer *) self, value->vector[0]);
	return 0;
}

static int set_Renderer_raymarch_refract_step(void *self, const struct PropertyValue *value)
{
	RdrSetRaymarchRefractStep((struct Renderer *) self, value->vector[0]);
	return 0;
}

static int set_Renderer_resolution(void *self, const struct PropertyValue *value)
{
	RdrSetResolution((struct Renderer *) self, (int) value->vector[0], (int) value->vector[1]);
	return 0;
}

static int set_Renderer_pixelsamples(void *self, const struct PropertyValue *value)
{
	RdrSetPixelSamples((struct Renderer *) self,
			(int) value->vector[0], (int) value->vector[1]);
	return 0;
}

static int set_Renderer_tilesize(void *self, const struct PropertyValue *value)
{
	RdrSetTileSize((struct Renderer *) self, (int) value->vector[0], (int) value->vector[1]);
	return 0;
}

static int set_Renderer_filterwidth(void *self, const struct PropertyValue *value)
{
	RdrSetFilterWidth((struct Renderer *) self, value->vector[0], value->vector[1]);
	return 0;
}

static int set_Renderer_render_region(void *self, const struct PropertyValue *value)
{
	RdrSetRenderRegion((struct Renderer *) self,
			(int) value->vector[0], (int) value->vector[1],
			(int) value->vector[2], (int) value->vector[3]);
	return 0;
}

/* Volume property settings */
static int set_Volume_resolution(void *self, const struct PropertyValue *value)
{
	VolResize((struct Volume *) self,
			value->vector[0], value->vector[1], value->vector[2]);
	return 0;
}

static int set_Volume_bounds_min(void *self, const struct PropertyValue *value)
{
	struct Volume *volume = (struct Volume *) self;
	double bounds[6] = {0};

	VolGetBounds(volume, bounds);

	bounds[0] = value->vector[0];
	bounds[1] = value->vector[1];
	bounds[2] = value->vector[2];

	VolSetBounds(volume, bounds);

	return 0;
}

static int set_Volume_bounds_max(void *self, const struct PropertyValue *value)
{
	struct Volume *volume = (struct Volume *) self;
	double bounds[6] = {0};

	VolGetBounds(volume, bounds);

	bounds[3] = value->vector[0];
	bounds[4] = value->vector[1];
	bounds[5] = value->vector[2];

	VolSetBounds(volume, bounds);

	return 0;
}

/* Camera property settings */
static int set_Camera_fov(void *self, const struct PropertyValue *value)
{
	CamSetFov((struct Camera *) self, value->vector[0]);
	return 0;
}

static int set_Camera_znear(void *self, const struct PropertyValue *value)
{
	CamSetNearPlane((struct Camera *) self, value->vector[0]);
	return 0;
}

static int set_Camera_zfar(void *self, const struct PropertyValue *value)
{
	CamSetFarPlane((struct Camera *) self, value->vector[0]);
	return 0;
}

static int set_Camera_position(void *self, const struct PropertyValue *value)
{
	CamSetPosition((struct Camera *) self,
			value->vector[0], value->vector[1], value->vector[2]);
	return 0;
}

static int set_Camera_direction(void *self, const struct PropertyValue *value)
{
	CamSetDirection((struct Camera *) self,
			value->vector[0], value->vector[1], value->vector[2]);
	return 0;
}

static int set_Light_intensity(void *self, const struct PropertyValue *value)
{
	struct Light *light = (struct Light *) self;
	LgtSetIntensity(light, value->vector[0]);
	return 0;
}

static int set_Light_position(void *self, const struct PropertyValue *value)
{
	struct Light *light = (struct Light *) self;
	LgtSetPosition(light, value->vector[0], value->vector[1], value->vector[2]);
	return 0;
}

static const struct Property ObjectInstance_properties[] = {
	{PROP_SCALAR, "transform_order", set_ObjectInstance_transform_order},
	{PROP_SCALAR, "rotate_order",    set_ObjectInstance_rotate_order},
	{PROP_VECTOR3, "translate", set_ObjectInstance_translate},
	{PROP_VECTOR3, "rotate",    set_ObjectInstance_rotate},
	{PROP_VECTOR3, "scale",     set_ObjectInstance_scale},
	{PROP_NONE, NULL, NULL}
};

static const struct Property Turbulence_properties[] = {
	{PROP_SCALAR, "lacunarity", set_Turbulence_lacunarity},
	{PROP_SCALAR, "gain",       set_Turbulence_gain},
	{PROP_SCALAR, "octaves",    set_Turbulence_octaves},
	{PROP_VECTOR3, "amplitude", set_Turbulence_amplitude},
	{PROP_VECTOR3, "frequency", set_Turbulence_frequency},
	{PROP_VECTOR3, "offset",    set_Turbulence_offset},
	{PROP_NONE, NULL, NULL}
};

static const struct Property Renderer_properties[] = {
	{PROP_SCALAR,  "sample_jitter",          set_Renderer_sample_jitter},
	{PROP_SCALAR,  "cast_shadow",            set_Renderer_cast_shadow},
	{PROP_SCALAR,  "max_reflect_depth",      set_Renderer_max_reflect_depth},
	{PROP_SCALAR,  "max_refract_depth",      set_Renderer_max_refract_depth},
	{PROP_SCALAR,  "raymarch_step",          set_Renderer_raymarch_step},
	{PROP_SCALAR,  "raymarch_shadow_step",   set_Renderer_raymarch_shadow_step},
	{PROP_SCALAR,  "raymarch_reflect_step",  set_Renderer_raymarch_reflect_step},
	{PROP_SCALAR,  "raymarch_refract_step",  set_Renderer_raymarch_refract_step},
	{PROP_VECTOR2, "resolution",   set_Renderer_resolution},
	{PROP_VECTOR2, "pixelsamples", set_Renderer_pixelsamples},
	{PROP_VECTOR2, "tilesize",     set_Renderer_tilesize},
	{PROP_VECTOR2, "filterwidth",  set_Renderer_filterwidth},
	{PROP_VECTOR4, "render_region", set_Renderer_render_region},
	{PROP_NONE, NULL, NULL}
};

static const struct Property Volume_properties[] = {
	{PROP_VECTOR3, "resolution",       set_Volume_resolution},
	{PROP_VECTOR3, "bounds_min",       set_Volume_bounds_min},
	{PROP_VECTOR3, "bounds_max",       set_Volume_bounds_max},
	{PROP_NONE, NULL, NULL}
};

static const struct Property Camera_properties[] = {
	{PROP_SCALAR,  "fov",       set_Camera_fov},
	{PROP_SCALAR,  "znear",     set_Camera_znear},
	{PROP_SCALAR,  "zfar",      set_Camera_zfar},
	{PROP_VECTOR3, "position",  set_Camera_position},
	{PROP_VECTOR3, "direction", set_Camera_direction},
	{PROP_NONE, NULL, NULL}
};

static const struct Property Light_properties[] = {
	{PROP_SCALAR,  "intensity", set_Light_intensity},
	{PROP_VECTOR3, "position",  set_Light_position},
	{PROP_NONE, NULL, NULL}
};

static int find_and_set_property(void *self, const struct Property *src_props,
		const char *prop_name, const struct PropertyValue *src_data)
{
	const struct Property *dst_prop = PropFind(src_props, src_data->type, prop_name);
	if (dst_prop == NULL)
		return -1;

	if (self == NULL)
		return -1;

	assert(dst_prop->SetProperty != NULL);
	return dst_prop->SetProperty(self, src_data);
}

static int set_property(const struct Entry *entry,
		const char *name, const struct PropertyValue *value)
{
	const struct Property *src_props = NULL;
	void *dst_object = NULL;

	/* plugin object properties */
	if (entry->type == Type_Procedure) {
		struct Procedure *procedure = ScnGetProcedure(scene, entry->index);
		if (procedure == NULL)
			return SI_FAIL;

		return PrcSetProperty(procedure, name, value);
	}
	else if (entry->type == Type_Shader) {
		struct Shader *shader = ScnGetShader(scene, entry->index);
		if (shader == NULL)
			return SI_FAIL;

		return ShdSetProperty(shader, name, value);
	}

	/* builtin object properties */
	switch (entry->type) {
	case Type_ObjectInstance:
		dst_object = ScnGetObjectInstance(scene, entry->index);
		src_props = ObjectInstance_properties;
		break;
	case Type_Turbulence:
		dst_object = ScnGetTurbulence(scene, entry->index);
		src_props = Turbulence_properties;
		break;
	case Type_Procedure:
		assert(!is_valid_type(entry->type) && "Should not be here!");
		break;
	case Type_Renderer:
		dst_object = ScnGetRenderer(scene, entry->index);
		src_props = Renderer_properties;
		break;
	case Type_Volume:
		dst_object = ScnGetVolume(scene, entry->index);
		src_props = Volume_properties;
		break;
	case Type_Camera:
		dst_object = ScnGetCamera(scene, entry->index);
		src_props = Camera_properties;
		break;
	case Type_Shader:
		assert(!is_valid_type(entry->type) && "Should not be here!");
		break;
	case Type_Light:
		dst_object = ScnGetLight(scene, entry->index);
		src_props = Light_properties;
		break;
	default:
		assert(!is_valid_type(entry->type) && "Some types are not implemented yet");
		break;
	}

	if (dst_object == NULL)
		return -1;

	return find_and_set_property(dst_object, src_props, name, value);
}

static int get_property_list(const char *type_name,
		const char ***property_types,
		const char ***property_names,
		int *property_count)
{
	const struct Property *prop = NULL;
	const struct Property *help_props = NULL;
	static const char *prop_types[1024] = {NULL};
	static const char *prop_names[1024] = {NULL};
	int i = 0;

	/* builtin object properties */
	if (strcmp(type_name, "ObjectInstance") == 0) {
		help_props = ObjectInstance_properties;
	}
	else if (strcmp(type_name, "Turbulence") == 0) {
		help_props = Turbulence_properties;
	}
	else if (strcmp(type_name, "Renderer") == 0) {
		help_props = Renderer_properties;
	}
	else if (strcmp(type_name, "Volume") == 0) {
		help_props = Volume_properties;
	}
	else if (strcmp(type_name, "Camera") == 0) {
		help_props = Camera_properties;
	}
	else if (strcmp(type_name, "Light") == 0) {
		help_props = Light_properties;
	}
	else {
	/* plugin object properties */
		const char *plugin_name = type_name;
		struct Plugin **plugins = ScnGetPluginList(scene);
		struct Plugin *found = NULL;
		const int N = (int) ScnGetPluginCount(scene);
		int i = 0;

		for (i = 0; i < N; i++) {
			if (strcmp(plugin_name, PlgGetName(plugins[i])) == 0) {
				found = plugins[i];
				break;
			}
		}
		if (found == NULL) {
			set_errno(SI_ERR_FAILNEW);
			return SI_BADID;
		}
		help_props = PlgGetPropertyList(found);
	}

	if (help_props == NULL) {
		/* TODO error handling */
		return SI_FAIL;
	}

	for (prop = help_props; prop->name != NULL; prop++, i++) {
		switch (prop->type) {
		case PROP_SCALAR:      prop_types[i] = "scalar"; break;
		case PROP_VECTOR2:     prop_types[i] = "vector2"; break;
		case PROP_VECTOR3:     prop_types[i] = "vector3"; break;
		case PROP_VECTOR4:     prop_types[i] = "vector4"; break;
		case PROP_TURBULENCE:  prop_types[i] = "Turbulence"; break;
		case PROP_TEXTURE:     prop_types[i] = "Texture"; break;
		case PROP_SHADER:      prop_types[i] = "Shader"; break;
		case PROP_VOLUME:      prop_types[i] = "Volume"; break;
		default:               prop_types[i] = NULL; break;
		}
		prop_names[i] = prop->name;
	}
	prop_types[i] = NULL;
	prop_names[i] = NULL;

	*property_types = prop_types;
	*property_names = prop_names;
	*property_count = i;

	return SI_SUCCESS;
}


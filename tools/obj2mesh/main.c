/*
Copyright (c) 2011-2013 Hiroshi Tsubokawa
See LICENSE and README
*/

#include "ObjParser.h"
#include "Triangle.h"
#include "MeshIO.h"
#include "Vector.h"
#include "Array.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char USAGE[] =
"Usage: obj2mesh [options] inputfile(*.obj) outputfile(*.mesh)\n"
"Options:\n"
"  --help         Display this information\n"
"\n";

struct ObjBuffer {
	struct Array *P;
	struct Array *N;
	struct Array *uv;
	struct Array *vertex_indices;
	struct Array *texture_indices;
	struct Array *normal_indices;

	long nverts;
	long nfaces;
};

extern struct ObjBuffer *ObjBufferNew(void);
extern void ObjBufferFree(struct ObjBuffer *buffer);
extern int ObjBufferFromFile(struct ObjBuffer *buffer, const char *filename);
extern int ObjBufferToMeshFile(const struct ObjBuffer *buffer, const char *filename);
extern int ObjBufferComputeNormals(struct ObjBuffer *buffer);

int main(int argc, const char **argv)
{
	struct ObjBuffer *buffer = NULL;
	const char *in_filename = NULL;
	const char *out_filename = NULL;
	int err = 0;

	if (argc == 2 && strcmp(argv[1], "--help") == 0) {
		printf("%s", USAGE);
		return 0;
	}

	if (argc != 3) {
		fprintf(stderr, "error: invalid number of arguments.\n");
		fprintf(stderr, "%s", USAGE);
		return -1;
	}

	in_filename = argv[1];
	out_filename = argv[2];

	buffer = ObjBufferNew();
	if (buffer == NULL) {
		/* TODO error handling */
		return -1;
	}

	err = ObjBufferFromFile(buffer, in_filename);
	if (err) {
		/* TODO error handling */
		return -1;
	}

	err = ObjBufferComputeNormals(buffer);
	if (err) {
		/* TODO error handling */
		return -1;
	}

	err = ObjBufferToMeshFile(buffer, out_filename);
	if (err) {
		/* TODO error handling */
		return -1;
	}

	printf("nverts: %ld\n", buffer->nverts);
	printf("nfaces: %ld\n", buffer->nfaces);

	ObjBufferFree(buffer);
	return 0;
}

struct ObjBuffer *ObjBufferNew(void)
{
	struct ObjBuffer *buffer = (struct ObjBuffer *) malloc(sizeof(struct ObjBuffer));
	if (buffer == NULL)
		return NULL;

	buffer->P = ArrNew(sizeof(double));
	buffer->N = ArrNew(sizeof(double));
	buffer->uv = ArrNew(sizeof(float));
	buffer->vertex_indices = ArrNew(sizeof(int));
	buffer->texture_indices = ArrNew(sizeof(int));
	buffer->normal_indices = ArrNew(sizeof(int));

	buffer->nverts = 0;
	buffer->nfaces = 0;

	return buffer;
}

void ObjBufferFree(struct ObjBuffer *buffer)
{
	if (buffer == NULL)
		return;

	ArrFree(buffer->P);
	ArrFree(buffer->N);
	ArrFree(buffer->uv);
	ArrFree(buffer->vertex_indices);
	ArrFree(buffer->texture_indices);
	ArrFree(buffer->normal_indices);

	free(buffer);
}

static int read_vertx(
		void *interpreter,
		int scanned_ncomponents,
		double x,
		double y,
		double z,
		double w)
{
	struct ObjBuffer *buffer = (struct ObjBuffer *) interpreter;

	ArrPush(buffer->P, &x);
	ArrPush(buffer->P, &y);
	ArrPush(buffer->P, &z);

	buffer->nverts++;
	return 0;
}

static int read_texture(
		void *interpreter,
		int scanned_ncomponents,
		double x,
		double y,
		double z,
		double w)
{
	struct ObjBuffer *buffer = (struct ObjBuffer *) interpreter;

	float uv[2] = {0, 0};
	uv[0] = x;
	uv[1] = y;
	ArrPush(buffer->uv, &uv[0]);
	ArrPush(buffer->uv, &uv[1]);

	return 0;
}

static int read_normal(
		void *interpreter,
		int scanned_ncomponents,
		double x,
		double y,
		double z,
		double w)
{
	struct ObjBuffer *buffer = (struct ObjBuffer *) interpreter;

	ArrPush(buffer->N, &x);
	ArrPush(buffer->N, &y);
	ArrPush(buffer->N, &z);

	return 0;
}

static int read_face(
		void *interpreter,
		long index_count,
		const long *vertex_indices,
		const long *texture_indices,
		const long *normal_indices)
{
	struct ObjBuffer *buffer = (struct ObjBuffer *) interpreter;
	int i;

	const int ntriangles = index_count - 2;

	for (i = 0; i < ntriangles; i++) {
		if (vertex_indices != NULL) {
			int indices[3] = {0, 0, 0};
			indices[0] = vertex_indices[0] - 1;
			indices[1] = vertex_indices[i + 1] - 1;
			indices[2] = vertex_indices[i + 2] - 1;
			ArrPush(buffer->vertex_indices, &indices[0]);
			ArrPush(buffer->vertex_indices, &indices[1]);
			ArrPush(buffer->vertex_indices, &indices[2]);
		}

		if (texture_indices != NULL) {
			int indices[3] = {0, 0, 0};
			indices[0] = texture_indices[0] - 1;
			indices[1] = texture_indices[i + 1] - 1;
			indices[2] = texture_indices[i + 2] - 1;
			ArrPush(buffer->texture_indices, &indices[0]);
			ArrPush(buffer->texture_indices, &indices[1]);
			ArrPush(buffer->texture_indices, &indices[2]);
		}

		if (normal_indices != NULL) {
			int indices[3] = {0, 0, 0};
			indices[0] = normal_indices[0] - 1;
			indices[1] = normal_indices[i + 1] - 1;
			indices[2] = normal_indices[i + 2] - 1;
			ArrPush(buffer->normal_indices, &indices[0]);
			ArrPush(buffer->normal_indices, &indices[1]);
			ArrPush(buffer->normal_indices, &indices[2]);
		}
	}

	buffer->nfaces += ntriangles;
	return 0;
}

int ObjBufferFromFile(struct ObjBuffer *buffer, const char *filename)
{
	struct ObjParser *parser = NULL;
	int err = 0;

	if (buffer == NULL)
		return -1;

	if (filename == NULL)
		return -1;

	parser = ObjParserNew(
			buffer,
			read_vertx,
			read_texture,
			read_normal,
			read_face);

	err = ObjParse(parser, filename);
	if (err) {
		return -1;
	}

	ObjParserFree(parser);
	return 0;
}

int ObjBufferToMeshFile(const struct ObjBuffer *buffer, const char *filename)
{
	struct MeshOutput *out = MshOpenOutputFile(filename);
	if (out == NULL) {
		return -1;
	}

	out->nverts = buffer->nverts;
	out->P = (double *) buffer->P->data;
	out->N = (double *) buffer->N->data;
	out->uv = (float *) buffer->uv->data;
	out->nfaces = buffer->nfaces;
	out->nface_attrs = 1;
	out->indices = (int *) buffer->vertex_indices->data;

	MshWriteFile(out);
	MshCloseOutputFile(out);
	return 0;
}

int ObjBufferComputeNormals(struct ObjBuffer *buffer)
{
	const int nverts = buffer->nverts;
	const int nfaces = buffer->nfaces;
	double *P = (double *) buffer->P->data;
	double *N = (double *) buffer->N->data;
	int *indices = (int *) buffer->vertex_indices->data;
	int i;

	if (P == NULL || indices == NULL)
		return -1;

	if (N == NULL) {
		ArrResize(buffer->N, 3 * nverts);
		N = (double *) buffer->N->data;
	}

	/* initialize N */
	for (i = 0; i < nverts; i++) {
		double *nml = &N[3*i];
		VEC3_SET(nml, 0, 0, 0);
	}

	/* compute N */
	for (i = 0; i < nfaces; i++) {
		double *P0, *P1, *P2;
		double *N0, *N1, *N2;
		double Ng[3] = {0, 0, 0};
		const int i0 = indices[3*i + 0];
		const int i1 = indices[3*i + 1];
		const int i2 = indices[3*i + 2];

		P0 = &P[3*i0];
		P1 = &P[3*i1];
		P2 = &P[3*i2];
		N0 = &N[3*i0];
		N1 = &N[3*i1];
		N2 = &N[3*i2];

		TriComputeFaceNormal(Ng, P0, P1, P2);
		VEC3_ADD_ASGN(N0, Ng);
		VEC3_ADD_ASGN(N1, Ng);
		VEC3_ADD_ASGN(N2, Ng);
	}

	/* normalize N */
	for (i = 0; i < nverts; i++) {
		double *nml = &N[3*i];
		VEC3_NORMALIZE(nml);
	}

	return 0;
}

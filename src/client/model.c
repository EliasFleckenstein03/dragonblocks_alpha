#include <dragonstd/tree.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "client/camera.h"
#include "client/client_config.h"
#include "client/frustum.h"
#include "client/opengl.h"
#include "client/model.h"
#include "client/shadows.h"

typedef struct {
	GLuint texture;
	Array vertices;
} ModelBatchTexture;

static List scene;
static List scene_new;
static pthread_mutex_t lock_scene_new;

static int cmp_node(const ModelNode *node, const char *str)
{
	if (str == node->name)
		return 0;

	if (!node->name)
		return -1;

	if (!str)
		return +1;

	return strcmp(node->name, str);
}

static void transform_node(ModelNode *node)
{
	if (node->parent)
		mat4x4_mul(node->abs, node->parent->abs, node->rel);
	else
		mat4x4_dup(node->abs, node->rel);

	list_itr(&node->children, &transform_node, NULL, NULL);
}

static void render_node(ModelNode *node, bool *shadow)
{
	if (!node->visible)
		return;

	if (node->clockwise) {
		glFrontFace(GL_CW); GL_DEBUG
	}

	for (size_t i = 0; i < node->meshes.siz; i++) {
		ModelMesh *mesh = &((ModelMesh *) node->meshes.ptr)[i];

		if (!mesh->mesh)
			continue;

		if (*shadow) {
			shadows_set_model(node->abs);
		} else {
			glUseProgram(mesh->shader->prog); GL_DEBUG
			glUniformMatrix4fv(mesh->shader->loc_transform, 1, GL_FALSE, node->abs[0]); GL_DEBUG
		}

		// bind textures
		for (GLuint i = 0; i < mesh->num_textures; i++) {
			glBindTextureUnit(i, mesh->textures[i]); GL_DEBUG
		}

		mesh_render(mesh->mesh);
	}

	list_itr(&node->children, &render_node, shadow, NULL);

	if (node->clockwise) {
		glFrontFace(GL_CCW); GL_DEBUG
	}
}

static void free_node_meshes(ModelNode *node)
{
	for (size_t i = 0; i < node->meshes.siz; i++) {
		ModelMesh *mesh = &((ModelMesh *) node->meshes.ptr)[i];

		mesh_destroy(mesh->mesh);
		free(mesh->mesh);
	}

	list_clr(&node->children, &free_node_meshes, NULL, NULL);
}

static void delete_node(ModelNode *node)
{
	for (size_t i = 0; i < node->meshes.siz; i++) {
		ModelMesh *mesh = &((ModelMesh *) node->meshes.ptr)[i];

		if (mesh->textures)
			free(mesh->textures);
	}
	list_clr(&node->children, &delete_node, NULL, NULL);
	array_clr(&node->meshes);

	if (node->name)
		free(node->name);
	free(node);
}

static void init_node(ModelNode *node, ModelNode *parent)
{
	if ((node->parent = parent))
		list_apd(&parent->children, node);

	list_ini(&node->children);
}

static void clone_mesh(ModelMesh *mesh)
{
	GLuint *old_textures = mesh->textures;

	if (old_textures)
		memcpy(mesh->textures = malloc(mesh->num_textures * sizeof *mesh->textures),
			old_textures, mesh->num_textures * sizeof *mesh->textures);
}

static ModelNode *clone_node(ModelNode *original, ModelNode *parent)
{
	ModelNode *node = malloc(sizeof *node);
	*node = *original;
	if (original->name)
		node->name = strdup(original->name);
	init_node(node, parent);

	array_cln(&node->meshes, &original->meshes);
	for (size_t i = 0; i < node->meshes.siz; i++)
		clone_mesh(&((ModelMesh *) node->meshes.ptr)[i]);

	list_itr(&original->children, &clone_node, node, NULL);
	return node;
}

static int cmp_model(const Model *model, const f32 *distance)
{
	return -f32_cmp(&model->distance, distance);
}

static void render_model(Model *model)
{
	if (model->callbacks.before_render)
		model->callbacks.before_render(model);

	bool shadow = false;
	render_node(model->root, &shadow);

	if (model->callbacks.after_render)
		model->callbacks.after_render(model);
}

// step model help im stuck
static void model_step(Model *model, f64 dtime)
{
	if (model->callbacks.step)
		model->callbacks.step(model, dtime);
}

static void model_render_shadow(Model *model)
{
	bool shadow = true;
	render_node(model->root, &shadow);
}

static void model_render_main(Model *model, Tree *transparent)
{
	if (client_config.view_distance < (model->distance = sqrt(
			pow(model->root->pos.x - camera.eye[0], 2) +
			pow(model->root->pos.y - camera.eye[1], 2) +
			pow(model->root->pos.z - camera.eye[2], 2))))
		return;

	if (!model->root->visible)
		return;

	if (model->flags.frustum_culling && frustum_cull((aabb3f32) {
			v3f32_add(model->box.min, model->root->pos),
		 	v3f32_add(model->box.max, model->root->pos)}))
		return;

	// fixme: if there are multiple objects with the exact same distance, only one is rendered
	if (model->flags.transparent)
		tree_add(transparent, &model->distance, model, &cmp_model, NULL);
	else
		render_model(model);
}

// init
void model_init()
{
	list_ini(&scene);
	list_ini(&scene_new);

	pthread_mutex_init(&lock_scene_new, NULL);
}

// ded
void model_deinit()
{
	list_clr(&scene, &model_delete, NULL, NULL);
	list_clr(&scene_new, &model_delete, NULL, NULL);

	pthread_mutex_destroy(&lock_scene_new);
}

// Model functions

Model *model_create()
{
	Model *model = malloc(sizeof *model);
	model->root = model_node_create(NULL);
	model->extra = NULL;
	model->replace = NULL;

	model->callbacks.step = NULL;
	model->callbacks.before_render = NULL;
	model->callbacks.after_render = NULL;
	model->callbacks.delete = NULL;

	model->flags.delete =
		model->flags.frustum_culling =
		model->flags.transparent = 0;

	return model;
}

Model *model_load(const char *path, const char *textures_path, Mesh *cube, ModelShader *shader)
{
	Model *model = model_create();

	Array stack;
	array_ini(&stack, sizeof(ModelNode *), 5);
	array_apd(&stack, &model->root);

	FILE *file = fopen(path, "r");
	if (!file) {
		fprintf(stderr, "[warning] failed to open model %s\n", path);
		return NULL;
	}

	char *line = NULL;
	size_t siz = 0;
	ssize_t length;
	int count = 0;

	while ((length = getline(&line, &siz, file)) > 0) {
		count++;

		if (*line == '#')
			continue;

		char *cursor = line - 1;

		size_t tabs = 0;
		while (*++cursor == '\t')
			tabs++;

		if (tabs >= stack.siz) {
			fprintf(stderr, "[warning] invalid indent in model %s in line %d\n", path, count);
			continue;
		}

		ModelNode *node = model_node_create(((ModelNode **) stack.ptr)[tabs]);

		int n;
		char key[length + 1];
		while (sscanf(cursor, "%s %n", key, &n) == 1) {
			cursor += n;

			if (strcmp(key, "name") == 0) {
				char name[length + 1];

				if (sscanf(cursor, "%s %n", name, &n) == 1) {
					cursor += n;

					if (node->name)
						free(node->name);
					node->name = strdup(name);
				} else {
					fprintf(stderr, "[warning] invalid value for name in model %s in line %d\n", path, count);
				}
			} else if (strcmp(key, "pos") == 0) {
				if (sscanf(cursor, "%f %f %f %n", &node->pos.x, &node->pos.y, &node->pos.z, &n) == 3)
					cursor += n;
				else
					fprintf(stderr, "[warning] invalid value for pos in model %s in line %d\n", path, count);
			} else if (strcmp(key, "rot") == 0) {
				if (sscanf(cursor, "%f %f %f %n", &node->rot.x, &node->rot.y, &node->rot.z, &n) == 3)
					cursor += n;
				else
					fprintf(stderr, "[warning] invalid value for rot in model %s in line %d\n", path, count);

				node->rot = v3f32_scale(node->rot, M_PI / 180.0);
			} else if (strcmp(key, "scale") == 0) {
				if (sscanf(cursor, "%f %f %f %n", &node->scale.x, &node->scale.y, &node->scale.z, &n) == 3)
					cursor += n;
				else
					fprintf(stderr, "[warning] invalid value for scale in model %s in line %d\n", path, count);
			} else if (strcmp(key, "clockwise") == 0) {
				node->clockwise = 1;
			} else if (strcmp(key, "cube") == 0) {
				char texture[length + 1];

				if (sscanf(cursor, "%s %n", texture, &n) == 1) {
					cursor += n;

					char filepath[strlen(textures_path) + 1 + strlen(texture) + 1];
					sprintf(filepath, "%s/%s", textures_path, texture);
					Texture *texture = texture_load_cubemap(filepath, false);

					model_node_add_mesh(node, &(ModelMesh) {
						.mesh = cube,
						.textures = &texture->txo,
						.num_textures = 1,
						.shader = shader,
					});
				} else {
					fprintf(stderr, "[warning] invalid value for cube in model %s in line %d\n", path, count);
				}
			} else {
				fprintf(stderr, "[warning] invalid key '%s' in model %s in line %d\n", key, path, count);
			}
		}

		model_node_transform(node);

		stack.siz = tabs + 1;
		array_apd(&stack, &node);
	}

	if (line)
		free(line);

	fclose(file);
	array_clr(&stack);

	transform_node(model->root);
	return model;
}

Model *model_clone(Model *original)
{
	Model *model = malloc(sizeof *model);
	*model = *original;
	model->root = clone_node(original->root, NULL);
	return model;
}

void model_delete(Model *model)
{
	if (model->callbacks.delete)
		model->callbacks.delete(model);

	delete_node(model->root);
	free(model);
}

void model_free_meshes(Model *model)
{
	free_node_meshes(model->root);
}

void model_get_bones(Model *model, ModelBoneMapping *mappings, size_t num_mappings)
{
	char *name, *cursor, *saveptr, *tok;

	for (size_t i = 0; i < num_mappings; i++) {
		name = cursor = strdup(mappings[i].name);

		ModelNode *node = model->root;

		while (node && (tok = strtok_r(cursor, ".", &saveptr))) {
			node = list_get(&node->children, tok, &cmp_node, NULL);
			cursor = NULL;
		}

		if (node)
			*mappings[i].node = node;
		else
			fprintf(stderr, "[warning] no such bone: %s\n", mappings[i].name);

		free(name);
	}
}

// ModelNode functions

ModelNode *model_node_create(ModelNode *parent)
{
	ModelNode *node = malloc(sizeof *node);
	node->name = NULL;
	node->visible = 1;
	node->clockwise = 0;
	node->pos = (v3f32) {0.0f, 0.0f, 0.0f};
	node->rot = (v3f32) {0.0f, 0.0f, 0.0f};
	node->scale = (v3f32) {1.0f, 1.0f, 1.0f};
	array_ini(&node->meshes, sizeof(ModelMesh), 0);
	init_node(node, parent);
	return node;
}

void model_node_transform(ModelNode *node)
{
	mat4x4_identity(node->rel);

	mat4x4_translate(node->rel,
		node->pos.x,
		node->pos.y,
		node->pos.z);

	mat4x4_scale_aniso(node->rel, node->rel,
		node->scale.x,
		node->scale.y,
		node->scale.z);

	mat4x4_rotate_X(node->rel, node->rel, node->rot.x);
	mat4x4_rotate_Y(node->rel, node->rel, node->rot.y);
	mat4x4_rotate_Z(node->rel, node->rel, node->rot.z);

	transform_node(node);
}

void model_node_add_mesh(ModelNode *node, const ModelMesh *mesh)
{
	array_apd(&node->meshes, mesh);
	clone_mesh(&((ModelMesh *) node->meshes.ptr)[node->meshes.siz - 1]);
}

// scene functions

void model_scene_add(Model *model)
{
	pthread_mutex_lock(&lock_scene_new);
	list_apd(&scene_new, model);
	pthread_mutex_unlock(&lock_scene_new);
}

void model_scene_update(f64 dtime)
{
	pthread_mutex_lock(&lock_scene_new);
	if (scene_new.fst) {
		*scene.end = scene_new.fst;
		scene.end = scene_new.end;

		list_ini(&scene_new);
	}
	pthread_mutex_unlock(&lock_scene_new);

	for (ListNode **node = &scene.fst; *node != NULL;) {
		Model *model = (*node)->dat;

		if (model->flags.delete) {
			if (model->replace)
				(*node)->dat = model->replace;
			else
				list_nrm(&scene, node);

			model_delete(model);
		} else {
			node = &(*node)->nxt;
			model_step(model, dtime);
		}
	}
}

void model_scene_render_shadows()
{
	LIST_ITERATE(&scene, node) {
		model_render_shadow(node->dat);
	}
}

void model_scene_render()
{
	// model_render_shadow

	Tree transparent;
	tree_ini(&transparent);

	LIST_ITERATE(&scene, node) {
		model_render_main(node->dat, &transparent);
	}

	tree_clr(&transparent, &render_model, NULL, NULL, TRAVERSION_INORDER);
}

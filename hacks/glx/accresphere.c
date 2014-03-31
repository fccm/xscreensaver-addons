/* Accretion Spheres,
 * a simple minimalistic screensaver featuring generation of spheres.
 * Copyright (c) 2010 Florent Monnier <monnier.florent(_)gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#define DEFAULTS    "*delay:        30000   \n" \
                    "*count:        30      \n" \
                    "*showFPS:      False   \n" \
                    "*wireframe:    False   \n" \

#define refresh_accsph 0

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "gln.h"
#include <ctype.h>
#include <math.h>
#include <string.h>


#ifdef USE_GL /* whole file */

#define DEF_SPEED       "1.0"


static float speed;

static XrmOptionDescRec opts[] = {
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
};

ENTRYPOINT ModeSpecOpt accsph_opts =
  {countof(opts), opts, countof(vars), vars, NULL};


/* perspective projection parameters */
#define FOV_Y  30.0
#define Z_NEAR  2.0
#define Z_FAR   8.0

/* just the initial size, the buffer increases as needed */
#define INIT_NUM_SPHERES 40


struct generator {
  float rgb[3];
  float xyz[3];
  float angle;
  float size;
};

struct sphere {
  float color[3];
  float dir[3];
  float pos[3];
  float scale;
  float life;
  float dying_step;
  int is_dying;
  int alive;
};

struct app {
  float angle;
  double ratio;
  double prev_t;
  unsigned int n_gens;
  struct generator *gens;
  gln_mesh *meshes[2];
  gln_matrices matrices;
  gln_drawMeshParams p;
  unsigned int num_spheres;
  struct sphere *spheres;
  GLXContext *glx_context;
};
static struct app * apps = NULL;


static double my_gettimeofday(void)
{
  struct timeval tp;
  if (gettimeofday(&tp, NULL) == -1)
    fprintf(stderr, "error: gettimeofday\n");
  return ((double) tp.tv_sec + (double) tp.tv_usec / 1e6);
}


/* color components */
static inline float low(void)  { return (frand(0.3) + 0.1); }
static inline float mid(void)  { return (frand(0.4) + 0.3); }
static inline float high(void) { return (frand(0.4) + 0.5); }
static inline float full(void) { return (frand(0.6) + 0.3); }

#define R(def) rgb[0] = def()
#define G(def) rgb[1] = def()
#define B(def) rgb[2] = def()

static void red(float *rgb) { R(high); G(low); B(low); }
static void blue(float *rgb) { R(low); G(mid); B(high); }
static void cyan(float *rgb) { R(low); G(high); B(high); }
static void green(float *rgb) { R(low); G(high); B(low); }
static void yellow(float *rgb) { R(high); G(high); B(low); }
static void orange(float *rgb) { R(mid); G(mid); B(low); }
static void bright(float *rgb) { R(high); G(high); B(high); }
static void any(float *rgb) { R(full); G(full); B(full); }

#undef R
#undef G
#undef B

static void grey(float *rgb) {
  float v;
  v = full();
  rgb[0] = v;
  rgb[1] = v;
  rgb[2] = v;
}

static void new_color(float *rgb)
{
  unsigned int which = NRAND(16);
  switch (which) {
    case 0: red(rgb); break;
    case 1: blue(rgb); break;
    case 2: cyan(rgb); break;
    case 3: green(rgb); break;
    case 4: yellow(rgb); break;
    case 5: orange(rgb); break;
    case 6: bright(rgb); break;
    case 7: grey(rgb); break;
    default: any(rgb); break;
  }
}


static void init_generator(struct generator *gen)
{
  gen->xyz[0] = frand(2.0) - 1.0;
  gen->xyz[1] = frand(2.0) - 1.0;
  gen->xyz[2] = frand(2.0) - 6.0;

  gen->angle = frand(0.06) - 0.03;

  gen->size = frand(1.2) + 0.4;

  new_color(gen->rgb);
}


static void make_generators(struct app *app)
{
  unsigned int i;
  unsigned int n = NRAND(4) + 3;
  app->gens = (struct generator *)
    calloc(n, sizeof(struct generator));
  if (!app->gens) {
    fprintf(stderr, "%s: out of memory\n", progname);
    exit(1);
  }
  app->n_gens = n;
  for (i = 0; i < n; i++)
  {
    init_generator(&(app->gens[i]));
  }
}


static void new_generators(struct app *app)
{
  free(app->gens);
  make_generators(app);
}


static void clear_spheres(struct app *app)
{
  int i;
  for (i = 0; i < app->num_spheres; i++)
  {
    app->spheres[i].alive = 0;
  }
}


static void clear_spheres_from(struct app *app, int from)
{
  int i;
  for (i = from; i < app->num_spheres; i++)
  {
    app->spheres[i].alive = 0;
  }
}


static void init_spheres(struct app *app)
{
  app->num_spheres = INIT_NUM_SPHERES;

  app->spheres = (struct sphere *)
    calloc(app->num_spheres, sizeof(struct sphere));
  if (!app->spheres) {
    fprintf(stderr, "%s: out of memory\n", progname);
    exit(1);
  }

  clear_spheres(app);
}


static void init_app_content(struct app *app)
{
  app->meshes[0] = glnMakeSphere(0.2, 32, 16, GLN_GEN_NORMALS);
  app->meshes[1] = glnMakeSphere(0.2, 64, 32, GLN_GEN_NORMALS);

  make_generators(app);
  init_spheres(app);

  app->prev_t = my_gettimeofday();
}


static void bg_clear_color(void)
{
  float base, r, g, b;
  base = frand(0.1) + 0.2;
  r = base + frand(0.02);
  g = base + frand(0.02);
  b = base + frand(0.02);
  glClearColor(r, g, b, 1.0);
}


static void accsph_init_gl(void)
{
  bg_clear_color();

  glEnable(GL_DEPTH_TEST);

  glFrontFace(GL_CCW);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
}


ENTRYPOINT void
reshape_accsph (ModeInfo *mi, int width, int height)
{
  struct app *app = &apps[MI_SCREEN(mi)];

  glViewport(0, 0, (GLint) width, (GLint) height);

  if (height == 0) height = 1;
  app->ratio = (double) width / (double) height;

  /* set up a perspective projection matrix */
  glnPerspective(&app->matrices, FOV_Y, app->ratio, Z_NEAR, Z_FAR);
}


ENTRYPOINT void 
init_accsph (ModeInfo *mi)
{
  struct app *app;
  /* int wire = MI_IS_WIREFRAME(mi); TODO */

  if (!apps) {
    apps = (struct app *)
      calloc (MI_NUM_SCREENS(mi), sizeof(struct app));
    if (!apps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }

  app = &apps[MI_SCREEN(mi)];

  app->glx_context = init_GL(mi);

  accsph_init_gl();
  init_app_content(app);

  reshape_accsph(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
}


static void delete_app(struct app *app)
{
  glnDeleteMesh(app->meshes[0]);
  glnDeleteMesh(app->meshes[1]);

  free(app->spheres);
  free(app->gens);
}


ENTRYPOINT void
release_accsph(ModeInfo * mi)
{
  int i;
  int num_screens = MI_NUM_SCREENS(mi);

  for (i = 0; i < num_screens; i++)
  {
    delete_app(&(apps[i]));
  }
  free(apps);
}


static inline void vec_mul(float *r, float f, const float *v)
{
  r[0] = f * v[0];
  r[1] = f * v[1];
  r[2] = f * v[2];
}

static inline void vec_add(float *v1, const float *v2)
{
  v1[0] += v2[0];
  v1[1] += v2[1];
  v1[2] += v2[2];
}

static void normalize(float *xyz)
{
  double x = xyz[0];
  double y = xyz[1];
  double z = xyz[2];
  double len = sqrt(x * x + y * y + z * z);
  xyz[0] = x / len;
  xyz[1] = y / len;
  xyz[2] = z / len;
}


static void init_sphere(struct sphere *s, float *rgb, float *xyz, float size)
{
  /* position approximatively the same than its generator */
  s->pos[0] = xyz[0] + frand(0.1) - 0.05;
  s->pos[1] = xyz[1] + frand(0.1) - 0.05;
  s->pos[2] = xyz[2] + frand(0.1) - 0.05;

  /* direction */
  s->dir[0] = frand(1.0) - 0.5;
  s->dir[1] = frand(1.0) - 0.5;
  s->dir[2] = frand(1.0) - 0.5;

  /* color approximatively the same than its generator */
  s->color[0] = rgb[0] + frand(0.2) - 0.1;
  s->color[1] = rgb[1] + frand(0.2) - 0.1;
  s->color[2] = rgb[2] + frand(0.2) - 0.1;

  /* duration that the sphere will be living */
  s->life = frand(8.0) + 6.0;

  s->scale = size;

  s->alive = 1;
  s->is_dying = 0;
  s->dying_step = 0.0;  /* not used until the sphere is dying */
}

static int find_empty_sphere(struct app *app)
{
  int i;
  for (i = 0; i < app->num_spheres; i++)
  {
    if (!app->spheres[i].alive) return i;
  }
  return -1;
}

static void new_sphere(struct app *app, struct generator g)
{
  int i = find_empty_sphere(app);
  if (i < 0)  {
    struct sphere *new_spheres;
    i = app->num_spheres;
    new_spheres = realloc(app->spheres, (app->num_spheres + 10) *
                                        sizeof(struct sphere));
    if (!new_spheres) {
      fprintf(stderr, "%s: out of memory\n", progname);
      return;
    } else {
      app->num_spheres += 10;
      app->spheres = new_spheres;
      clear_spheres_from(app, i);
    }
  }
  init_sphere(&(app->spheres[i]), g.rgb, g.xyz, g.size);
}

static void filter_spheres(struct app *app)
{
  int i;
  for (i = 0; i < app->num_spheres; i++)
  {
    struct sphere *s = &(app->spheres[i]);
    if (s->alive) {
      if (s->scale < 0.04) s->alive = 0;  /* too small */
      if (s->is_dying && s->dying_step < 0.0) s->alive = 0;
    }
  }
}

static void check_spheres(struct app *app)
{
  int i;
  for (i = 0; i < app->num_spheres; i++)
  {
    struct sphere *s = &(app->spheres[i]);
    if (s->alive) {
      if (s->life < 0.0 && s->is_dying == 0)
      {
        s->is_dying = 1;
        s->dying_step = 1.0;
      }
    }
  }
}

static void draw_item(
    struct app *app, gln_matrices *m, float *pos, float *color, float s)
{
  gln_mesh *mesh;
  glnLoadIdentity(m);
  glnTranslate(m, pos[0], pos[1], pos[2]);
  glnScale(m, s, s, s);
  if (s < 0.7) mesh = app->meshes[0]; else mesh = app->meshes[1];
  memcpy(app->p.color, color, 3 * sizeof(float));
  glnDrawMesh(mesh, m, &app->p);
}

static void draw_sphere(struct app *app, struct sphere *s, double dt)
{
  float dir[3];
  gln_matrices mat;
  vec_mul(dir, (dt * 0.1), s->dir);
  vec_add(s->pos, dir);
  s->scale -= (dt * 0.05);
  s->life -= dt;
  if (s->is_dying == 0) {
    memcpy(mat.projection, app->matrices.projection, 16 * sizeof(float));
    glDepthRange(0.0, 1.0);
  } else {
    /* using the z-near plane of the projection
       to simulate the explosion of the bubbles */
    double inv, near, d;
    s->dying_step -= (dt * 1.2);
    d = s->dying_step;
    inv = 1.0 - d;
    near = (Z_NEAR * d) + (Z_FAR * inv);
    glnPerspective(&mat, FOV_Y, app->ratio, near, Z_FAR);
    glDepthRange(inv, 1.0);
  }
  draw_item(app, &mat, s->pos, s->color, s->scale);
}

static void draw_gen(struct app *app, struct generator *g, double dt)
{
  float angle = g->angle * dt;
  float cos_a = cosf(angle);
  float sin_a = sinf(angle);
  float x = g->xyz[0];
  float y = g->xyz[1];
  float _x = x * cos_a - y * sin_a;
  float _y = x * sin_a + y * cos_a;
  g->xyz[0] = _x;
  g->xyz[1] = _y;
  glDepthRange(0.0, 1.0);
  draw_item(app, &app->matrices, g->xyz, g->rgb, g->size);
}

static void draw_spheres(struct app *app, double dt)
{
  int i;
  for (i = 0; i < app->num_spheres; i++)
  {
    struct sphere *s = &(app->spheres[i]);
    if (s->alive) draw_sphere(app, s, dt);
  }
}

static void draw_gens(struct app *app, double dt)
{
  int i;
  for (i = 0; i < app->n_gens; i++)
  {
    struct generator *g = &(app->gens[i]);
    draw_gen(app, g, dt);
  }
}

static void display(struct app *app)
{
  double t;
  double dt;
  int i;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  t = my_gettimeofday();
  dt = t - app->prev_t;
  dt *= speed;
  app->prev_t = t;

  {
    double a = (t * 0.1);

    app->p.light_dir[0] = cos(a);
    app->p.light_dir[1] = sin(a);
    app->p.light_dir[2] = 1.2;

    normalize(app->p.light_dir);
  }

  for (i = 0; i < app->n_gens; i++)
  {
    if (dt > frand(2.0))
    {
      new_sphere(app, app->gens[i]);
    }
  }

  filter_spheres(app);
  check_spheres(app);

  draw_spheres(app, dt);
  draw_gens(app, dt);
}


ENTRYPOINT void
draw_accsph (ModeInfo *mi)
{
  struct app *app = &apps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!app->glx_context)
    return;

  glXMakeCurrent(dpy, window, *(app->glx_context));

  display(app);

  if (mi->fps_p) do_fps(mi);
  glFinish();
  glXSwapBuffers(dpy, window);
}


ENTRYPOINT Bool
accsph_handle_event (ModeInfo *mi, XEvent *ev)
{
  struct app *app = &apps[MI_SCREEN(mi)];
  char key;
  int screen;

  screen = MI_SCREEN(mi);

  if (ev->xany.type == KeyPress) {
    key = XKeycodeToKeysym(mi->dpy, ev->xkey.keycode, 0);
    switch (key) {
      case 'c':
        clear_spheres(app);
        return True;
      case 'n':
        new_generators(app);
        return True;
      case 'b':
        bg_clear_color();
        return True;
      case ' ':
        clear_spheres(app);
        new_generators(app);
        bg_clear_color();
        return True;
      case 'm':
        speed *= 1.2;
        return True;
      case 'd':
        speed *= 0.8;
        return True;
    }
  }
  return False;
}

XSCREENSAVER_MODULE_2 ("AccreSphere", accresphere, accsph)

#endif /* USE_GL */

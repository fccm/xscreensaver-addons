/* -*- Mode: C; tab-width: 2 -*- */
/*
 * Copyright (C) 2008 Florent Monnier <fmonnier@linux-nantes.org>
 * Basic Z-sorting demo without Z-Buffer.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 */

#include <stdlib.h>
#include <math.h>

#ifdef STANDALONE
# define DEFAULTS	\
          "*delay: 60000 \n" \
          "*showFPS: false \n" \

# include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
# include "xlock.h"			/* in xlockmore distribution */
#endif /* STANDALONE */


#ifdef USE_GL


ENTRYPOINT ModeSpecOpt unsorted_opts = {
  0, NULL, 0, NULL, NULL
};

#ifdef USE_MODULES
ModStruct  unsorted_description =
  { "unsorted",
    "init_unsorted", "draw_unsorted", "release_unsorted",
    "refresh_unsorted", "init_unsorted", NULL,
    &unsorted_opts,
    /* I dont know what are the parameters of the line below: */
    1000, 25, 40, 1, 4, 1.0, "",
    "Basic Z-sorting demo without Z-Buffer", 0, NULL};
#endif


/* number of cubes along each axis */
#define SIDE 7

/* total number of cubes */
#define NB_CUBES (SIDE * SIDE * SIDE)

/* number of segments in circles */
#define CIRC_SEGi 16
#define CIRC_SEGf 16.0f

#define CIRC_SEGi2 24
#define CIRC_SEGf2 24.0f

/* number of cylinder rows */
#define CYL_ZN 6

/* total number of cylinders */
#define NB_CYLINDERS (CIRC_SEGi2 * CYL_ZN)

#define TWO_PI (M_PI * 2.0)


#define bound_random(bound) (random() % bound)

#define mat_get_z(mat) ((mat)[14])
#define mat_get_y(mat) ((mat)[13])


enum geom_type {
  CUBE_GEOM = 0,
  CYLINDER_GEOM = 1
};

struct cube_struct {
  float r, g, b;
  int vis;
  GLfloat mat[16];
};

struct cylinder_struct {
  float r, g, b;
  int vis;
  GLfloat mat[16];
};

struct cube_geom {
  struct cube_struct cubes[NB_CUBES];
  int colors[NB_CUBES];
  int visibility[NB_CUBES];
};

struct cylinder_geom {
  struct cylinder_struct cylinders[NB_CYLINDERS];
  int colors[NB_CYLINDERS];
  int visibility[NB_CYLINDERS];
  float circ_cache[CIRC_SEGi][2];
  float circ_cache2[CIRC_SEGi][2];
};

union geom_disp {
  struct cube_geom cube_app;
  struct cylinder_geom cylinder_app;
};

struct app_contents {
  GLXContext *glx_context;
  float anglex;
  float angley;
  int draw_mode;
  enum geom_type type;
  union geom_disp geom;
};


static void init_cubes_visibility(struct app_contents *app) {
  int i;
  for (i=0; i < NB_CUBES; i++)
    app->geom.cube_app.visibility[i] = bound_random(2);
}

static void init_cylinder_visibility(struct app_contents *app) {
  int i;
  for (i=0; i < NB_CUBES; i++) {
    if (bound_random(300) < 20)
      app->geom.cylinder_app.visibility[i] = 0;
    else
      app->geom.cylinder_app.visibility[i] = 1;
  }
}

static int get_cube_visibility(struct app_contents app, int index) {
  return app.geom.cube_app.visibility[index];
}

static int get_cylinder_visibility(struct app_contents app, int index) {
  return app.geom.cylinder_app.visibility[index];
}

static void step_visibility(struct app_contents *app, int threshold) {
  int i;
  switch (app->type)
  {
    case CUBE_GEOM:
      for (i=0; i < NB_CUBES; i++)
        if (bound_random(threshold) < 10)
          app->geom.cube_app.visibility[i] =
            !app->geom.cube_app.visibility[i];
      break;

    case CYLINDER_GEOM:
      for (i=0; i < NB_CYLINDERS; i++)
        if (bound_random(threshold) < 10)
          app->geom.cylinder_app.visibility[i] =
            !app->geom.cylinder_app.visibility[i];
      break;
  }
}

static void init_cubes_color(struct app_contents *app) {
  int i;
  for (i=0; i < NB_CUBES; i++)
    app->geom.cube_app.colors[i] = bound_random(4);
}

static void init_cylinders_color(struct app_contents *app) {
  int i;
  for (i=0; i < NB_CYLINDERS; i++)
    app->geom.cylinder_app.colors[i] = bound_random(4);
}

static void get_cube_color(struct app_contents app,
                           int index, float *r, float *g, float *b)
{
  switch (app.geom.cube_app.colors[index]) {
    case 0: *r = 1.0; *g = 0.0; *b = 0.0; break;
    case 1: *r = 0.1; *g = 0.8; *b = 0.0; break;
    case 2: *r = 0.1; *g = 0.4; *b = 1.0; break;
    case 3: *r = 0.7; *g = 0.0; *b = 0.6; break;
  }
}

static void get_cylinder_color(struct app_contents app,
                               int index, float *r, float *g, float *b)
{
  switch (app.geom.cylinder_app.colors[index]) {
    case 0: *r = 1.0; *g = 0.0; *b = 0.0; break;
    case 1: *r = 0.1; *g = 0.8; *b = 0.0; break;
    case 2: *r = 0.1; *g = 0.4; *b = 1.0; break;
    case 3: *r = 0.7; *g = 0.0; *b = 0.6; break;
  }
}

static void change_colors(struct app_contents * app, int threshold) {
  int i;
  switch (app->type)
  {
    case CUBE_GEOM:
      for (i=0; i < NB_CUBES; i++)
        if (bound_random(threshold) < 10)
          app->geom.cube_app.colors[i] = bound_random(4);
      break;

    case CYLINDER_GEOM:
      for (i=0; i < NB_CYLINDERS; i++)
        if (bound_random(threshold) < 10)
          app->geom.cylinder_app.colors[i] = bound_random(4);
      break;
  }

}

static void init_cylinder_circ_cache(struct app_contents *app) {
  int i;
  for (i = 0; i < CIRC_SEGi; i++)
  {
    float a;
    float x, y;
    a = TWO_PI / CIRC_SEGf * ((float) i);
    x = 1.1f * cosf(a);
    y = 1.1f * sinf(a);
    app->geom.cylinder_app.circ_cache[i][0] = x;
    app->geom.cylinder_app.circ_cache[i][1] = y;
  }
  for (i = 0; i < CIRC_SEGi2; i++)
  {
    float a;
    float x, y;
    a = TWO_PI / CIRC_SEGf2 * ((float) i);
    x = 1.3f * cosf(a);
    y = 1.3f * sinf(a);
    app->geom.cylinder_app.circ_cache2[i][0] = x;
    app->geom.cylinder_app.circ_cache2[i][1] = y;
  }
}

static struct app_contents * app_storage = NULL;

static void init_app(ModeInfo *mi, struct app_contents *apps[])
{
  if (*apps == NULL)
  {
    int num_screens;
    num_screens = MI_NUM_SCREENS(mi);

    *apps = calloc(num_screens, sizeof(struct app_contents));

    if (*apps == NULL) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }

  /* one app cell per screen */
  {
    int screen = MI_SCREEN(mi);
    struct app_contents *app;

    app = &((*apps)[screen]);

    app->glx_context = init_GL(mi);

    app->anglex = bound_random(90);
    app->angley = bound_random(90);
    app->draw_mode = bound_random(1);

    switch (bound_random(2)) {
      case 0: app->type = CUBE_GEOM; break;
      case 1: app->type = CYLINDER_GEOM; break;
    }

    if (app->type == CUBE_GEOM)
    {
      init_cubes_color(app);
      init_cubes_visibility(app);
    }
    if (app->type == CYLINDER_GEOM)
    {
      init_cylinders_color(app);
      init_cylinder_visibility(app);
      init_cylinder_circ_cache(app);
    }
  }
}

static void gradient_toggle (struct app_contents * app) {
    app->draw_mode = !app->draw_mode;
}

static void gradient_toggle_step (struct app_contents * app) {
  if (bound_random(30000) < 100)
    app->draw_mode = !app->draw_mode;
}

static int z_compare(const void * _a, const void * _b)
{
  struct cube_struct *a, *b;
  a = (struct cube_struct *) _a;
  b = (struct cube_struct *) _b;
  /* sort along the Z axis, the direction of the look */
  if (mat_get_z(a->mat) > mat_get_z(b->mat))
    return 1;
  else
    return -1;
}

void draw_solid_cube(float size);
void draw_wire_cube(float size);
void draw_wire_cylinder(struct app_contents app, float scale);
void draw_solid_cylinder(struct app_contents app, float scale);

static void main_display (struct app_contents app) {
  float step;
  float half;
  float scale;
  int i, j, k;
  int index;

  if (app.draw_mode)
    glClearColor(0.24, 0.25, 0.26, 0.0);
  else
    glClearColor(0.38, 0.16, 0.0, 0.0);

  glClear(GL_COLOR_BUFFER_BIT);
  glLoadIdentity();

  glTranslatef(0.0, 0.0, -4.0);

  glRotatef(app.angley, 1.0, 0.0, 0.0);
  glRotatef(app.anglex, 0.0, 1.0, 0.0);

  if (app.type == CUBE_GEOM)
  {
    scale = 3.0 / SIDE;
    glScalef(scale, scale, scale);

    step = 0.8;
    half = step * ((float)(SIDE - 1)) / 2.0;

    index = 0;
    for (i=0; i < SIDE; i++)
      for (j=0; j < SIDE; j++)
        for (k=0; k < SIDE; k++)
        {
          struct cube_struct * cube;
          float x, y, z;
          int vis;

          cube = &(app.geom.cube_app.cubes[index]);

          /* one color assigned per cube */
          if (!app.draw_mode) {
            get_cube_color(app, index,
                &(cube->r),
                &(cube->g),
                &(cube->b) );
          }

          vis = get_cube_visibility(app, index);
          cube->vis = vis;

          x = (((float)i) * step) - half;
          y = (((float)j) * step) - half;
          z = (((float)k) * step) - half;

          glPushMatrix();
            glTranslatef(x, y, z);
            glGetFloatv(GL_MODELVIEW_MATRIX, cube->mat);
          glPopMatrix();

          index++;
        }

    /* sort the items along the Z axis */
    qsort(app.geom.cube_app.cubes, NB_CUBES,
          sizeof(struct cube_struct), z_compare);

    /* make a vertical gradient with the cubes */
    if (app.draw_mode)
      for (i=0; i < NB_CUBES; i++)
      {
        float r;
        float y, v;
        struct cube_struct * cube;
        cube = &(app.geom.cube_app.cubes[i]);
        y = mat_get_y(cube->mat);
        r = sqrt(half * half * 3.) * 0.5;
        v = y + r;
        v = v / r / 2.;
        v = v * 1.4;
        cube->r = v;
        cube->b = 1.0 - v;
        cube->g = 0.0;
      }

    /* finally do draw the cubes */
    for (i=0; i < NB_CUBES; i++)
    {
      struct cube_struct * cube;
      cube = &(app.geom.cube_app.cubes[i]);
      if (cube->vis)
      {
        glLoadIdentity();
        glMultMatrixf(cube->mat);

        glScalef(0.5, 0.5, 0.5);
        glColor3f(0.0, 0.0, 0.0);
        draw_solid_cube(1.0);

        glColor3f(cube->r, cube->g, cube->b);
        draw_solid_cube(0.89);
        glColor3f(1.0, 1.0, 1.0);
        draw_wire_cube(0.89);
      }
    }
  }

  if (app.type == CYLINDER_GEOM)
  {
    int i;

    index = 0;
    for (i=0; i < CIRC_SEGi2; i++)
    {
      struct cylinder_struct * cyld;
      float x, y;
      x = app.geom.cylinder_app.circ_cache2[i][0];
      y = app.geom.cylinder_app.circ_cache2[i][1];

      { int zi; float z;
        for (zi = 0; zi < CYL_ZN; zi++) /* each rows */
        {
          z = -1.0f + (zi * (2.0f / (CYL_ZN - 1)));

          cyld = &(app.geom.cylinder_app.cylinders[index]);
          if (!app.draw_mode) {
            get_cylinder_color(app, index,
                &(cyld->r),
                &(cyld->g),
                &(cyld->b) );
          }

          cyld->vis = get_cylinder_visibility(app, index);

          glPushMatrix();
            glTranslatef(x, y, z);
            glGetFloatv(GL_MODELVIEW_MATRIX, cyld->mat);
          glPopMatrix();
          index++;
        }
      }
    }

    /* sort the items along the Z axis */
    qsort(app.geom.cylinder_app.cylinders, NB_CYLINDERS,
          sizeof(struct cylinder_struct), z_compare);

    /* make a vertical gradient */
    if (app.draw_mode)
      for (i=0; i < NB_CYLINDERS; i++)
      {
        float r;
        float y, v;
        struct cylinder_struct * cyld;
        cyld = &(app.geom.cylinder_app.cylinders[i]);
        y = mat_get_y(cyld->mat);
        r = sqrt(half * half * 3.) * 0.5;
        v = y + r;
        v = v / r / 2.;
        v = v * 1.4;
        cyld->r = v;
        cyld->b = 1.0 - v;
        cyld->g = 0.0;
      }

    for (i=0; i < NB_CYLINDERS; i++)
    {
      struct cylinder_struct * cyld;
      cyld = &(app.geom.cylinder_app.cylinders[i]);
      if (cyld->vis)
      {
        glLoadIdentity();
        glMultMatrixf(cyld->mat);

        glColor3f(0.0, 0.0, 0.0);             draw_solid_cylinder(app, 0.10);
        glColor3f(cyld->r, cyld->g, cyld->b); draw_solid_cylinder(app, 0.09);
        glColor3f(1.0, 1.0, 1.0);             draw_wire_cylinder(app,  0.09);
      }
    }
  }
}

/* points for one cube */
#define A() glVertex3f( - size, + size, - size )
#define B() glVertex3f( + size, + size, - size )
#define C() glVertex3f( + size, - size, - size )
#define D() glVertex3f( - size, - size, - size )
#define E() glVertex3f( - size, - size, + size )
#define F() glVertex3f( + size, - size, + size )
#define G() glVertex3f( + size, + size, + size )
#define H() glVertex3f( - size, + size, + size )

/* normals directions for each face */
#define pos_x() glNormal3d( 1.0,  0.0,  0.0)
#define neg_x() glNormal3d(-1.0,  0.0,  0.0)
#define pos_y() glNormal3d( 0.0,  1.0,  0.0)
#define neg_y() glNormal3d( 0.0, -1.0,  0.0)
#define pos_z() glNormal3d( 0.0,  0.0,  1.0)
#define neg_z() glNormal3d( 0.0,  0.0, -1.0)

void draw_solid_cube(float _size)
{
  float size = _size * 0.5;
  /* we dont need normals for this demo */
#if 0
  glBegin(GL_TRIANGLE_STRIP);
    neg_z();    D(); A(); C(); B();
    pos_x();    F(); G();
    pos_z();    E(); H();
    neg_x();    D(); A();
  glEnd();

  glBegin(GL_TRIANGLE_STRIP);
    pos_y();    A(); H(); B(); G();
  glEnd();

  glBegin(GL_TRIANGLE_STRIP);
    neg_y();    E(); D(); F(); C();
  glEnd();
#else
  glBegin(GL_TRIANGLE_STRIP);
    D(); A(); C(); B();
    F(); G();
    E(); H();
    D(); A();
  glEnd();

  glBegin(GL_TRIANGLE_STRIP);
    A(); H(); B(); G();
  glEnd();

  glBegin(GL_TRIANGLE_STRIP);
    E(); D(); F(); C();
  glEnd();
#endif
}

void draw_wire_cube(float _size)
{
  float size = _size * 0.5f;

  glBegin(GL_LINE_LOOP);
    A(); B(); C(); D(); E(); F(); G(); H();
  glEnd();

  glBegin(GL_LINES);
    A(); D();
    B(); G();
    C(); F();
    E(); H();
  glEnd();
}

#undef pos_x
#undef neg_x
#undef pos_y
#undef neg_y
#undef pos_z
#undef neg_z

#undef A
#undef B
#undef C
#undef D
#undef E
#undef F
#undef G
#undef H


void draw_wire_cylinder(struct app_contents app, float scale)
{
  glPushMatrix();
  glScalef(scale, scale, scale);
  { int i, alt;
    glBegin(GL_LINE_LOOP);
    for (i=0; i < CIRC_SEGi; i++)
    {
      float x, y;
      x = app.geom.cylinder_app.circ_cache[i][0];
      y = app.geom.cylinder_app.circ_cache[i][1];
      glVertex3f( x, y, 1.0f );
    }
    glEnd();

    glBegin(GL_LINE_LOOP);
    for (i=0; i < CIRC_SEGi; i++)
    {
      float x, y;
      x = app.geom.cylinder_app.circ_cache[i][0];
      y = app.geom.cylinder_app.circ_cache[i][1];
      glVertex3f( x, y, -1.0f );
    }
    glEnd();

    alt = 1;
    glBegin(GL_LINES);
    for (i=0; i < CIRC_SEGi; i++)
    {
      if (alt) {
        float x, y;
        x = app.geom.cylinder_app.circ_cache[i][0];
        y = app.geom.cylinder_app.circ_cache[i][1];
        glVertex3f( x, y, 1.0f );
        glVertex3f( x, y, -1.0f );
      }
      alt = !alt;
    }
    glEnd();
  }
  glPopMatrix();
}

void draw_solid_cylinder(struct app_contents app, float scale)
{
  glPushMatrix();
  glScalef(scale, scale, scale);
  { int i;
    glBegin(GL_QUADS);
    for (i=0; i < (CIRC_SEGi - 1); i++)
    {
      float x, y;

      x = app.geom.cylinder_app.circ_cache[i][0];
      y = app.geom.cylinder_app.circ_cache[i][1];
      glVertex3f( x, y, 1.0f );
      glVertex3f( x, y, -1.0f );

      x = app.geom.cylinder_app.circ_cache[i+1][0];
      y = app.geom.cylinder_app.circ_cache[i+1][1];
      glVertex3f( x, y, -1.0f );
      glVertex3f( x, y, 1.0f );
    }
    {
      float x, y;

      x = app.geom.cylinder_app.circ_cache[CIRC_SEGi-1][0];
      y = app.geom.cylinder_app.circ_cache[CIRC_SEGi-1][1];
      glVertex3f( x, y, 1.0f );
      glVertex3f( x, y, -1.0f );

      x = app.geom.cylinder_app.circ_cache[0][0];
      y = app.geom.cylinder_app.circ_cache[0][1];
      glVertex3f( x, y, -1.0f );
      glVertex3f( x, y, 1.0f );
    }
    glEnd();

    /*
    glBegin(GL_TRIANGLE_FAN);
      glVertex3f( 0.0f, 0.0f, 1.0f );
    */
    glBegin(GL_POLYGON);
    for (i=0; i < CIRC_SEGi; i++)
    {
      float x, y;
      x = app.geom.cylinder_app.circ_cache[i][0];
      y = app.geom.cylinder_app.circ_cache[i][1];
      glVertex3f( x, y, 1.0f );
    }
    glEnd();
  }
  glPopMatrix();
}


static void rotation_ticks(struct app_contents *app, float rotx, float roty) {
  app->anglex += rotx;
  app->angley += roty;
}

static void init_local_gl(void) {
  /* no depth buffer */
  glDisable(GL_DEPTH_TEST);
  glShadeModel(GL_FLAT);

  /* assumes clean models */
  glFrontFace(GL_CCW);
  glCullFace(GL_FRONT);
}

static GLXContext *
get_glxcontext_of_screen(int screen)
{
  return app_storage[screen].glx_context;
}

ENTRYPOINT void reshape_unsorted(ModeInfo *mi, int width, int height);

ENTRYPOINT void init_unsorted(ModeInfo *mi)
{
  GLXContext *glx_context;
  Display *display;
  Window window;

  display = MI_DISPLAY(mi);
  window = MI_WINDOW(mi);

  init_app(mi, &app_storage);

  glx_context = get_glxcontext_of_screen( MI_SCREEN(mi) );

  if (glx_context == NULL)
  {
    MI_CLEARWINDOW(mi);
    return;
  }

  init_local_gl();
  reshape_unsorted(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
}

ENTRYPOINT void draw_unsorted(ModeInfo *mi)
{
  GLXContext *glx_context;
  Display *display;
  Window window;
  int screen;

  display = MI_DISPLAY(mi);
  window = MI_WINDOW(mi);
  screen = MI_SCREEN(mi);

  glx_context = app_storage[screen].glx_context;

  MI_IS_DRAWN(mi) = True;

  if (!glx_context)
    return;

  glXMakeCurrent(display, window, *glx_context);

  if (screen != 0) {
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    return;

  } else {
    /* display */
    main_display(app_storage[screen]);

    /* animate */
    rotation_ticks(&app_storage[screen], 0.02, 0.2);
    gradient_toggle_step(&app_storage[screen]);
    change_colors(&app_storage[screen], 10000);
    step_visibility(&app_storage[screen], 30000);
  }

  if (mi->fps_p) do_fps (mi);
  glFinish();
  glXSwapBuffers(display, window);
}

ENTRYPOINT void reshape_unsorted(ModeInfo *mi, int width, int height)
{
  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(60.0, 1.0, 0.5, 100.0);
  glMatrixMode(GL_MODELVIEW);
}

static Bool keyboard(struct app_contents * app, char key) {
  switch (key)
  {
    case 'g':
      gradient_toggle(app);
      return True;

    case 'c':
      change_colors(app, 12);
      return True;

    case 'v':
      step_visibility(app, 30);
      return True;

    case 'r':
      rotation_ticks(app, 0.06, 0.6);
      return True;

    case ' ':
      switch (app->type) {
        case CUBE_GEOM: app->type = CYLINDER_GEOM; break;
        case CYLINDER_GEOM: app->type = CUBE_GEOM; break;
      }
      if (app->type == CUBE_GEOM)
      {
        init_cubes_color(app);
        init_cubes_visibility(app);
      }
      if (app->type == CYLINDER_GEOM)
      {
        init_cylinders_color(app);
        init_cylinder_visibility(app);
        init_cylinder_circ_cache(app);
      }
      return True;

    default:
      return False;
  }
}

ENTRYPOINT Bool unsorted_handle_event(ModeInfo *mi, XEvent *event)
{
  int screen;
  screen = MI_SCREEN(mi);
  if (screen != 0) return False;

  if(event->xany.type == KeyPress)
  {
    char key;
    key = XKeycodeToKeysym(mi->dpy, event->xkey.keycode, 0);

    return keyboard(&app_storage[screen], key);
  }
  return False;
}

ENTRYPOINT void refresh_unsorted(ModeInfo *mi)
{
}

ENTRYPOINT void release_unsorted(ModeInfo *mi)
{
  if (app_storage) {
    free(app_storage);
    app_storage = NULL;
  }
  FreeAllGL(mi);
}

#ifndef STANDALONE
ENTRYPOINT void change_unsorted(ModeInfo * mi)
{
  GLXContext *glx_context;
  glx_context = get_glxcontext_of_screen( MI_SCREEN(mi) );

  if (!glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *glx_context);
  Init(mi);
}
#endif /* !STANDALONE */


XSCREENSAVER_MODULE("Unsorted", unsorted)

#endif /* USE_GL */

/* vim: sw=2 sts=2 ts=2 et
 */

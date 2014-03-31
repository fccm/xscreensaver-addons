/* -*- Mode: C; tab-width: 2 -*- */
/*
 * Copyright (C) 2008 Florent Monnier <fmonnier@linux-nantes.org>
 * Demo with the texture of a leaf which looks like the surface of the Moon.
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

#ifdef STANDALONE
  #define DEFAULTS \
       "*delay:      60000 \n" \
       "*showFPS:    False \n" \
       "*wireframe:  False \n"
  #include "xlockmore.h"  /* from the xscreensaver distribution */
#else  /* !STANDALONE */
  #include "xlock.h"      /* from the xlockmore distribution */
  #include "vis.h"
#endif /* !STANDALONE */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
#include <jpeglib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/*#include "leaf_moon.h"*/

#ifdef USE_GL


ENTRYPOINT ModeSpecOpt leaf_moon_opts = {
  0, NULL, 0, NULL, NULL
};

#ifdef USE_MODULES
ModStruct  leaf_moon_description =
  { "leaf_moon",
    "init_leaf_moon", "draw_leaf_moon", "release_leaf_moon",
    "refresh_leaf_moon", "init_leaf_moon", NULL,
    &leaf_moon_opts,
    /* I dont know what are the parameters of the line below: */
    1000, 25, 40, 1, 4, 1.0, "",
    "Demo with the texture of a sheet which looks like the surface of the moon", 0, NULL};
#endif


#define bound_random(bound) (random() % bound)


struct cube_struct {
  GLfloat mat[16];
  float r, g, b;
  int vis;
};

struct sphere_struct {
  GLfloat mat[16];
  float r, g, b;
  int vis;
};

struct cube_app {
  float angle_x;
  float angle_y;
  float texture_y;
  float texture_rot;
  GLuint texture_id;
};


struct tex_container {
  unsigned char * tex_data;
  unsigned int width;
  unsigned int height;
  unsigned int num_components;
  unsigned int color_space;
};

/* {{{ JPEG */

struct my_error_mgr {
  struct jpeg_error_mgr pub;    /* "public" fields */
  jmp_buf setjmp_buffer;        /* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

/* Routine that replaces the standard error_exit method */
METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
  my_error_ptr myerr = (my_error_ptr) cinfo->err;
  (*cinfo->err->output_message) (cinfo);
  longjmp(myerr->setjmp_buffer, 1);
}

/* JPEG decompression */
static void
load_jpeg_file (char * filename, struct tex_container * cont)
{
  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr;
  char err_buf[192];
  FILE * infile;
  unsigned char * image;
  unsigned char * img_data;

  if ((infile = fopen(filename, "rb")) == NULL) {
    snprintf(err_buf, 192, "Error: can't open jpeg file '%s'", filename);
    /* TODO make an xscreensaver compatible error handling */
    fprintf(stderr, "%s", err_buf);
    exit(1);
  }

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;

  if (setjmp(jerr.setjmp_buffer)) {
    snprintf(err_buf, 192, "Error while loading jpeg file '%s'", filename);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    /* TODO make an xscreensaver compatible error handling */
    fprintf(stderr, "%s", err_buf);
    exit(1);
  }

  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, infile);

  (void) jpeg_read_header(&cinfo, TRUE);

  (void) jpeg_start_decompress(&cinfo);
  {
    unsigned char *line;
    img_data = (unsigned char *)malloc(sizeof(char) *
                                       cinfo.output_width *
                                       cinfo.output_height *
                                       cinfo.output_components);
    image = img_data;
    line = image;

    while (cinfo.output_scanline < cinfo.output_height) {
      line = image + cinfo.output_components * cinfo.output_width * cinfo.output_scanline;
      (void) jpeg_read_scanlines(&cinfo, &line, 1);
    }
  }

  (void) jpeg_finish_decompress(&cinfo);

  fclose(infile);

  cont->tex_data       = img_data;
  cont->width          = cinfo.output_width;
  cont->height         = cinfo.output_height;
  cont->num_components = cinfo.output_components;
  cont->color_space    = cinfo.out_color_space;

  jpeg_destroy_decompress(&cinfo);
}

/* }}} */

static int file_exists(const char *name)
{
  struct stat st;
  return (stat(name, &st) == 0);
}

static GLuint init_texture (void)
{
  struct tex_container cont;
  GLuint tex_id;

  GLint  internal_format   = GL_RGB;
  GLenum pixel_data_format = GL_RGB;

  char *img_locs[] = {
    "/usr/share/xscreensaver/images/leafmoon.jpg",
    "/usr/local/share/xscreensaver/images/leafmoon.jpg",
    "./leafmoon.jpg",
  };
  int i;

  cont.tex_data = NULL;
  /* TODO find how to handle associated files in xscreensaver */
  for (i=0; i < (sizeof(img_locs) / sizeof(char *)); i++) {
    if (file_exists (img_locs[i])) {
      printf("loading image: '%s'\n", img_locs[i]);
      load_jpeg_file (img_locs[i], &cont);
      break;
    }
  }
  if (cont.tex_data == NULL) exit(1);

  if (cont.num_components == 3 && cont.color_space == JCS_RGB) {
    internal_format   = GL_RGB;
    pixel_data_format = GL_RGB;
  }
  if (cont.num_components == 1 && cont.color_space == JCS_GRAYSCALE) {
    internal_format   = GL_LUMINANCE;
    pixel_data_format = GL_LUMINANCE;
  }

  /* generate texture */
  glGenTextures (1, &tex_id);
  glBindTexture (GL_TEXTURE_2D, tex_id);

  /* setup some parameters for texture filters and mipmapping */
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glTexImage2D (GL_TEXTURE_2D, 0, internal_format,
         cont.width, cont.height, 0, pixel_data_format,
         GL_UNSIGNED_BYTE, cont.tex_data);

  /*
  gluBuild2DMipmaps (GL_TEXTURE_2D, internal_format,
         cont.width, cont.height, pixel_data_format,
         GL_UNSIGNED_BYTE, cont.tex_data);
  */

  /* OpenGL has its own copy of texture data */
  free (cont.tex_data);

  return tex_id;
}

static GLuint init_local_gl(void)
{
  glEnable(GL_DEPTH_TEST);
  glShadeModel(GL_FLAT);

  glClearColor(0.0, 0.0, 0.0, 0.0);

  glFrontFace(GL_CCW);
  glCullFace(GL_FRONT);

  glEnable (GL_TEXTURE_2D);

  return init_texture ();
}



static struct cube_app * app_storage = NULL;

static void init_app_vars(struct cube_app *app)
{
  app->angle_x = (((float)bound_random(4)) * 90.0f);
  app->angle_y = (((float)bound_random(4)) * 90.0f);
  app->texture_y = 0.0f;
  app->texture_rot = 0.0f;
}

static void init_app(ModeInfo *mi, struct cube_app *apps[])
{
  if (*apps == NULL)
  {
    int i;
    int num_screens;
    /* WARNING: MI_NUM_SCREENS() gives me a result of 2
       while I have only 1 screen! */
    num_screens = MI_NUM_SCREENS(mi);

    *apps = calloc(num_screens, sizeof(struct cube_app));

    for (i = 0; i < num_screens; i++)
    {
      struct cube_app *app;
      app = &((*apps)[i]);

      init_app_vars(app);
    }
  }
}


static void cube_display (struct cube_app app)
{
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();

  glTranslatef(0.0, 0.0, -1.9);
  glRotatef(app.angle_y, 1.0, 0.0, 0.0);
  glRotatef(app.angle_x, 0.0, 1.0, 0.0);

  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();
  { float s = 1.4;
    glScalef(s, s, s); }
  glTranslatef(0.0, app.texture_y, 0.0);
  glRotatef(app.texture_rot, 0.0, 0.0, 1.0);
  glMatrixMode(GL_MODELVIEW);

  glEnable (GL_TEXTURE_2D); /* begin textured */
  glBindTexture(GL_TEXTURE_2D, app.texture_id);

  glBegin(GL_QUADS);
  glTexCoord2f(0.0, 0.25);   glVertex3f(-0.5,  0.5,  0.5);
  glTexCoord2f(0.0, 0.5);    glVertex3f(-0.5, -0.5,  0.5);
  glTexCoord2f(0.25, 0.5);   glVertex3f( 0.5, -0.5,  0.5);
  glTexCoord2f(0.25, 0.25);  glVertex3f( 0.5,  0.5,  0.5);

  glTexCoord2f(0.25, 0.25);  glVertex3f( 0.5,  0.5,  0.5);
  glTexCoord2f(0.25, 0.5);   glVertex3f( 0.5, -0.5,  0.5);
  glTexCoord2f(0.5, 0.5);    glVertex3f( 0.5, -0.5, -0.5);
  glTexCoord2f(0.5, 0.25);   glVertex3f( 0.5,  0.5, -0.5);

  glTexCoord2f(0.5, 0.25);   glVertex3f( 0.5,  0.5, -0.5);
  glTexCoord2f(0.5, 0.5);    glVertex3f( 0.5, -0.5, -0.5);
  glTexCoord2f(0.75, 0.5);   glVertex3f(-0.5, -0.5, -0.5);
  glTexCoord2f(0.75, 0.25);  glVertex3f(-0.5,  0.5, -0.5);

  glTexCoord2f(0.75, 0.25);  glVertex3f(-0.5,  0.5, -0.5);
  glTexCoord2f(0.75, 0.5);   glVertex3f(-0.5, -0.5, -0.5);
  glTexCoord2f(1.0, 0.5);    glVertex3f(-0.5, -0.5,  0.5);
  glTexCoord2f(1.0, 0.25);   glVertex3f(-0.5,  0.5,  0.5);

  glTexCoord2f(0.0, 0.0);    glVertex3f(-0.5,  0.5, -0.5);
  glTexCoord2f(0.0, 0.25);   glVertex3f(-0.5,  0.5,  0.5);
  glTexCoord2f(0.25, 0.25);  glVertex3f( 0.5,  0.5,  0.5);
  glTexCoord2f(0.25, 0.0);   glVertex3f( 0.5,  0.5, -0.5);

  glTexCoord2f(0.0, 0.75);   glVertex3f(-0.5, -0.5, -0.5);
  glTexCoord2f(0.0, 0.5);    glVertex3f(-0.5, -0.5,  0.5);
  glTexCoord2f(0.25, 0.5);   glVertex3f( 0.5, -0.5,  0.5);
  glTexCoord2f(0.25, 0.75);  glVertex3f( 0.5, -0.5, -0.5);
  glEnd();

  glDisable (GL_TEXTURE_2D); /* end of textured */
}

static void animation_ticks(struct cube_app *app)
{
  app->angle_x += 0.12;
  app->angle_y += 0.06;
  app->texture_y -= 0.00014;
  app->texture_rot += 0.03;
}


static GLXContext *glx_context;

static GLXContext *
get_glxcontext_of_screen(int screen)
{
  if (screen != 0) return NULL;
  return glx_context;
}

ENTRYPOINT void reshape_leaf_moon(ModeInfo *mi, int width, int height);

ENTRYPOINT void init_leaf_moon(ModeInfo *mi)
{
  Display *display;
  Window window;
  int screen;

  display = MI_DISPLAY(mi);
  window = MI_WINDOW(mi);
  screen = MI_SCREEN(mi);

  glx_context = init_GL(mi);

  if (glx_context == NULL)
  {
    MI_CLEARWINDOW(mi);
    return;
  }
  else
  {
    GLuint tex_id;
    int i;
    int num_screens;
    num_screens = MI_NUM_SCREENS(mi);

    reshape_leaf_moon(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    init_app(mi, &app_storage);
    tex_id = init_local_gl();
    for (i=0; i<num_screens; ++i) {
      app_storage[i].texture_id = tex_id;
    }
  }
}

ENTRYPOINT void draw_leaf_moon(ModeInfo *mi)
{
  GLXContext *glx_context;
  Display *display;
  Window window;
  int screen;

  display = MI_DISPLAY(mi);
  window = MI_WINDOW(mi);
  glx_context = get_glxcontext_of_screen( MI_SCREEN(mi) );

  MI_IS_DRAWN(mi) = True;

  if (!glx_context)
    return;
  
  glXMakeCurrent(display, window, *glx_context);

  screen = MI_SCREEN(mi);

  {
    /* display */
    cube_display(app_storage[screen]);

    /* animate */
    animation_ticks(&(app_storage[screen]));
  }

  if (mi->fps_p) do_fps (mi);
  glFinish();
  glXSwapBuffers(display, window);
}

ENTRYPOINT void reshape_leaf_moon(ModeInfo *mi, int width, int height)
{
  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(60.0, (((float) width) / ((float) (height == 0 ? 1 : height))), 0.2, 8.0);
  glMatrixMode(GL_MODELVIEW);
}

static Bool keyboard(struct cube_app * app, char key) {
  switch (key)
  {
    case 'l':
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      return True;
    case 'n':
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      return True;
    default:
      return False;
  }
}

ENTRYPOINT Bool leaf_moon_handle_event(ModeInfo *mi, XEvent *event) 
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

ENTRYPOINT void refresh_leaf_moon(ModeInfo *mi)
{
}

ENTRYPOINT void release_leaf_moon(ModeInfo *mi)
{
  glDeleteTextures (1, &(app_storage[0].texture_id));
  free(app_storage);
  app_storage = NULL;
  glXDestroyContext(MI_DISPLAY(mi), *glx_context); /* is this necessary? */
  FreeAllGL(mi);
}

#ifndef STANDALONE
ENTRYPOINT void change_leaf_moon(ModeInfo * mi)
{
  GLXContext *glx_context;
  glx_context = get_glxcontext_of_screen( MI_SCREEN(mi) );

  if (!glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *glx_context);
  Init(mi);
}
#endif /* !STANDALONE */


XSCREENSAVER_MODULE("Leaf Moon", leaf_moon)

#endif /* USE_GL */

/* vim: sw=2 sts=2 ts=2 et fdm=marker
 */

/* Fake GLES2 validates the actual GLSM wrappers using opaque driver IDs.
 * No game, GL driver, proprietary SDK, or copied implementation is required. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define glCreateProgram fake_create
#define glLinkProgram fake_link
#define glGetProgramiv fake_program_iv
#define glUniform1f fake_1f
#define glUniform1i fake_1i
#define glUniform2f fake_2f
#define glUniform2i fake_2i
#define glUniform3f fake_3f
#define glUniform4f fake_4f
#define glUniform4i fake_4i
#define glUniform1fv fake_fv
#define glUniform2fv fake_fv
#define glUniform3fv fake_fv
#define glUniform4fv fake_fv
#define glUniform1iv fake_iv
#define glGetString fake_string
#define glGetIntegerv fake_integer
#define glBindFramebuffer fake_bind
#define glGetFramebufferAttachmentParameteriv fake_attachment
#define glFramebufferTexture2D fake_texture
#define glFramebufferRenderbuffer fake_renderbuffer
#include "glsm/glsm.c"
#include "glsym/glsym_es2.c"

static unsigned calls;
static GLuint next_program = 1, host_framebuffer = 1;
static GLint last_location;
static GLsizei last_count;
static GLfloat last_float;
GLuint GL_APIENTRY fake_create(void) { return next_program; }
void GL_APIENTRY fake_link(GLuint program) { (void)program; }
void GL_APIENTRY fake_program_iv(GLuint program, GLenum pname, GLint* value) {
    (void)program; assert(pname == GL_LINK_STATUS); *value = GL_TRUE;
}
void GL_APIENTRY fake_1f(GLint p, GLfloat a) { ++calls; last_location=p; last_float=a; }
void GL_APIENTRY fake_1i(GLint p, GLint a) { ++calls; last_location=p; (void)a; }
void GL_APIENTRY fake_2f(GLint p, GLfloat a, GLfloat b) { ++calls; last_location=p; (void)a;(void)b; }
void GL_APIENTRY fake_2i(GLint p, GLint a, GLint b) { ++calls; last_location=p; (void)a;(void)b; }
void GL_APIENTRY fake_3f(GLint p, GLfloat a, GLfloat b, GLfloat c) { ++calls; last_location=p; (void)a;(void)b;(void)c; }
void GL_APIENTRY fake_4f(GLint p, GLfloat a, GLfloat b, GLfloat c, GLfloat d) { ++calls; last_location=p; (void)a;(void)b;(void)c;(void)d; }
void GL_APIENTRY fake_4i(GLint p, GLint a, GLint b, GLint c, GLint d) { ++calls; last_location=p; (void)a;(void)b;(void)c;(void)d; }
void GL_APIENTRY fake_fv(GLint p, GLsizei n, const GLfloat* v) { ++calls; last_location=p; last_count=n; if(n>0)last_float=v[0]; }
void GL_APIENTRY fake_iv(GLint p, GLsizei n, const GLint* v) { ++calls; last_location=p; last_count=n; (void)v; }
const GLubyte* GL_APIENTRY fake_string(GLenum name) { (void)name; return (const GLubyte*)""; }
void GL_APIENTRY fake_integer(GLenum name, GLint* value) { (void)name; *value=8; }
void GL_APIENTRY fake_bind(GLenum target, GLuint name) { assert(target==GL_FRAMEBUFFER);(void)name; }
void GL_APIENTRY fake_attachment(GLenum target, GLenum at, GLenum pname, GLint* value) {
    (void)target;(void)at;(void)pname;*value=1;
}
void GL_APIENTRY fake_texture(GLenum t, GLenum a, GLenum tt, GLuint tex, GLint level) {
    (void)t;(void)a;(void)tt;(void)tex;(void)level;++calls;
}
void GL_APIENTRY fake_renderbuffer(GLenum t, GLenum a, GLenum rt, GLuint rb) {
    (void)t;(void)a;(void)rt;(void)rb;++calls;
}
static uintptr_t framebuffer(void) { return host_framebuffer; }
static retro_proc_address_t get_proc(const char* name) { (void)name;return NULL; }
void initGLFunctions(void) {}
void retroChangeWindow(void) {}
void rglgen_resolve_symbols(retro_hw_get_proc_address_t proc) { (void)proc; }

static void uniform_set(GLint location) {
    const GLfloat f[4]={3,4,5,6}; const GLint i[4]={3,4,5,6};
    rglUniform1f(location,3);rglUniform1i(location,3);
    rglUniform2f(location,3,4);rglUniform2i(location,3,4);
    rglUniform3f(location,3,4,5);rglUniform4i(location,3,4,5,6);
    rglUniform4f(location,3,4,5,6);
    rglUniform1fv(location,1,f);rglUniform1iv(location,1,i);
    rglUniform2fv(location,1,f);rglUniform3fv(location,1,f);
    rglUniform4fv(location,1,f);
}

int main(void) {
    hw_render.get_current_framebuffer=framebuffer;hw_render.get_proc_address=get_proc;

    next_program=MAX_UNIFORMS+500;
    assert(rglCreateProgram()==next_program);rglLinkProgram(next_program);
    gl_state.program=next_program; calls=0;
    uniform_set(0);assert(calls==12);
    next_program=1;assert(rglCreateProgram()==1);rglLinkProgram(1);gl_state.program=1;
    calls=0;uniform_set(-1);assert(calls==12&&last_location==-1);
    calls=0;uniform_set(MAX_UNIFORMS+900);assert(calls==12);
    calls=0;rglUniform1f(0,9);rglUniform1f(0,9);assert(calls==1);
    const GLfloat first[2]={9,3}, second[2]={9,8};
    rglUniform1fv(0,2,first);rglUniform1fv(0,2,second);assert(calls==3&&last_count==2);
    rglUniform1f(0,9);assert(calls==4); /* Array write must invalidate scalar cache. */
    rglLinkProgram(1);rglUniform1f(0,9);assert(calls==5&&last_float==9);
    rglUniform1fv(0,0,NULL);assert(calls==6&&last_count==0);

    host_framebuffer=MAX_FRAMEBUFFERS+50;glsm_state_setup();
    calls=0;rglFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,9,0);
    rglFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,7);
    assert(calls==2);
    /* Unknown in-range framebuffer and repeated setup cannot dereference NULL
     * or leak the host-owned descriptor overwritten by the next session. */
    host_framebuffer=9;glsm_state_setup();glsm_state_setup();
    gl_state.framebuf[0].desired_location=100;
    calls=0;rglFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,8,0);
    assert(calls==1);
    free(framebuffers[9]);framebuffers[9]=NULL;
    puts("PASS: opaque GL names/locations, scalar and array uniforms, FBO cache safety");
    return 0;
}



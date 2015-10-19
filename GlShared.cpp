#include "GlShared.hpp"
#include "GlTexture.hpp"

using namespace gfx;

void GlFramebuffer::attach(GLenum attachment, const GlTexture & tex)
{
    if(!handle)
        glGenFramebuffers(1, &handle);
    glBindFramebuffer(GL_FRAMEBUFFER, handle);
    glFramebufferTexture(GL_FRAMEBUFFER, attachment, tex.get_gl_handle(), 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    size = { (float) tex.get_size().x, (float) tex.get_size().x};
}

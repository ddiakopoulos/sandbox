#include "GlShared.hpp"
#include "GlTexture.hpp"

using namespace avl;

void GlFramebuffer::attach(GLenum attachment, const GlTexture & tex)
{
    if(!handle)
        glGenFramebuffers(1, &handle);
    glBindFramebuffer(GL_FRAMEBUFFER, handle);
    glFramebufferTexture(GL_FRAMEBUFFER, attachment, tex.get_gl_handle(), 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    size = float2(tex.get_size().x, tex.get_size().x);
}

void GlFramebuffer::attach(GLenum attachment, const GlRenderbuffer & rb)
{
    if(!handle)
        glGenFramebuffers(1, &handle);
    glBindFramebuffer(GL_FRAMEBUFFER, handle);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, rb.get_handle());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    size = float2(rb.get_size().x, rb.get_size().y);
}

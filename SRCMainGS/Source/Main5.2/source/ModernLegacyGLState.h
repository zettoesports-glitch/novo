#pragma once

#include "ModernGraphicsBootstrap.h"

#ifdef MU_ENABLE_DILIGENT

#include <algorithm>

// Captures the compatibility-OpenGL state that can be touched by a Diligent
// graphics draw and restores it when the modern pass ends. Main keeps its own
// logical GL state caches, so restoring the actual native state is essential:
// otherwise those caches may incorrectly skip a legacy glEnable/glBlendFunc/etc.
// call after a modern draw.
class CModernLegacyGLStateScope
{
public:
    CModernLegacyGLStateScope()
    {
        Capture();
    }

    ~CModernLegacyGLStateScope()
    {
        Restore();
    }

    CModernLegacyGLStateScope(const CModernLegacyGLStateScope&) = delete;
    CModernLegacyGLStateScope& operator=(const CModernLegacyGLStateScope&) = delete;

    void Restore()
    {
        if (!m_captured)
            return;

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(m_drawFramebuffer));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(m_readFramebuffer));

        glViewport(m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
        glDepthRange(m_depthRange[0], m_depthRange[1]);
        glScissor(m_scissorBox[0], m_scissorBox[1], m_scissorBox[2], m_scissorBox[3]);

        glUseProgram(static_cast<GLuint>(m_program));
        glBindVertexArray(static_cast<GLuint>(m_vertexArray));
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(m_arrayBuffer));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(m_elementArrayBuffer));

        for (GLint unit = 0; unit < m_textureUnitCount; ++unit)
        {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_texture2D[unit]));
            glBindSampler(static_cast<GLuint>(unit), static_cast<GLuint>(m_sampler[unit]));
            SetEnabled(GL_TEXTURE_2D, m_texture2DEnabled[unit] != GL_FALSE);
        }
        glActiveTexture(static_cast<GLenum>(m_activeTexture));

        glBlendFuncSeparate(static_cast<GLenum>(m_blendSrcRGB),
                            static_cast<GLenum>(m_blendDstRGB),
                            static_cast<GLenum>(m_blendSrcAlpha),
                            static_cast<GLenum>(m_blendDstAlpha));
        glBlendEquationSeparate(static_cast<GLenum>(m_blendEquationRGB),
                                static_cast<GLenum>(m_blendEquationAlpha));
        glBlendColor(m_blendColor[0], m_blendColor[1], m_blendColor[2], m_blendColor[3]);
        SetEnabled(GL_BLEND, m_blendEnabled);

        glColorMask(m_colorMask[0], m_colorMask[1], m_colorMask[2], m_colorMask[3]);
        glDepthMask(m_depthWriteMask);
        glDepthFunc(static_cast<GLenum>(m_depthFunc));
        SetEnabled(GL_DEPTH_TEST, m_depthTestEnabled);

        glCullFace(static_cast<GLenum>(m_cullFaceMode));
        glFrontFace(static_cast<GLenum>(m_frontFace));
        SetEnabled(GL_CULL_FACE, m_cullFaceEnabled);

        glStencilFuncSeparate(GL_FRONT,
                              static_cast<GLenum>(m_stencilFrontFunc),
                              m_stencilFrontRef,
                              static_cast<GLuint>(m_stencilFrontValueMask));
        glStencilMaskSeparate(GL_FRONT, static_cast<GLuint>(m_stencilFrontWriteMask));
        glStencilOpSeparate(GL_FRONT,
                            static_cast<GLenum>(m_stencilFrontFail),
                            static_cast<GLenum>(m_stencilFrontDepthFail),
                            static_cast<GLenum>(m_stencilFrontDepthPass));
        glStencilFuncSeparate(GL_BACK,
                              static_cast<GLenum>(m_stencilBackFunc),
                              m_stencilBackRef,
                              static_cast<GLuint>(m_stencilBackValueMask));
        glStencilMaskSeparate(GL_BACK, static_cast<GLuint>(m_stencilBackWriteMask));
        glStencilOpSeparate(GL_BACK,
                            static_cast<GLenum>(m_stencilBackFail),
                            static_cast<GLenum>(m_stencilBackDepthFail),
                            static_cast<GLenum>(m_stencilBackDepthPass));
        SetEnabled(GL_STENCIL_TEST, m_stencilEnabled);

        // Alpha test and fixed-function fog are compatibility-profile state used
        // by the legacy renderer. Phase 2 rejects core-only contexts, so these
        // queries/restores are valid on every active modern path.
        glAlphaFunc(static_cast<GLenum>(m_alphaFunc), m_alphaRef);
        SetEnabled(GL_ALPHA_TEST, m_alphaTestEnabled);
        SetEnabled(GL_FOG, m_fogEnabled);
        SetEnabled(GL_SCISSOR_TEST, m_scissorEnabled);

        glPolygonMode(GL_FRONT, static_cast<GLenum>(m_polygonMode[0]));
        glPolygonMode(GL_BACK, static_cast<GLenum>(m_polygonMode[1]));

        m_captured = false;
    }

private:
    static constexpr GLint MaxTrackedTextureUnits = 8;

    static void SetEnabled(GLenum capability, bool enabled)
    {
        if (enabled)
            glEnable(capability);
        else
            glDisable(capability);
    }

    void Capture()
    {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_drawFramebuffer);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_readFramebuffer);
        glGetIntegerv(GL_VIEWPORT, m_viewport);
        glGetDoublev(GL_DEPTH_RANGE, m_depthRange);
        glGetIntegerv(GL_SCISSOR_BOX, m_scissorBox);

        glGetIntegerv(GL_CURRENT_PROGRAM, &m_program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &m_vertexArray);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &m_arrayBuffer);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &m_elementArrayBuffer);

        glGetIntegerv(GL_ACTIVE_TEXTURE, &m_activeTexture);
        GLint maxTextureUnits = 0;
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
        m_textureUnitCount = (std::min)(MaxTrackedTextureUnits, (std::max)(0, maxTextureUnits));
        for (GLint unit = 0; unit < m_textureUnitCount; ++unit)
        {
            glActiveTexture(GL_TEXTURE0 + unit);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &m_texture2D[unit]);
            glGetIntegerv(GL_SAMPLER_BINDING, &m_sampler[unit]);
            m_texture2DEnabled[unit] = glIsEnabled(GL_TEXTURE_2D);
        }
        glActiveTexture(static_cast<GLenum>(m_activeTexture));

        m_blendEnabled = glIsEnabled(GL_BLEND) != GL_FALSE;
        glGetIntegerv(GL_BLEND_SRC_RGB, &m_blendSrcRGB);
        glGetIntegerv(GL_BLEND_DST_RGB, &m_blendDstRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &m_blendSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &m_blendDstAlpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &m_blendEquationRGB);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &m_blendEquationAlpha);
        glGetFloatv(GL_BLEND_COLOR, m_blendColor);
        glGetBooleanv(GL_COLOR_WRITEMASK, m_colorMask);

        m_depthTestEnabled = glIsEnabled(GL_DEPTH_TEST) != GL_FALSE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &m_depthWriteMask);
        glGetIntegerv(GL_DEPTH_FUNC, &m_depthFunc);

        m_cullFaceEnabled = glIsEnabled(GL_CULL_FACE) != GL_FALSE;
        glGetIntegerv(GL_CULL_FACE_MODE, &m_cullFaceMode);
        glGetIntegerv(GL_FRONT_FACE, &m_frontFace);

        m_stencilEnabled = glIsEnabled(GL_STENCIL_TEST) != GL_FALSE;
        glGetIntegerv(GL_STENCIL_FUNC, &m_stencilFrontFunc);
        glGetIntegerv(GL_STENCIL_REF, &m_stencilFrontRef);
        glGetIntegerv(GL_STENCIL_VALUE_MASK, &m_stencilFrontValueMask);
        glGetIntegerv(GL_STENCIL_WRITEMASK, &m_stencilFrontWriteMask);
        glGetIntegerv(GL_STENCIL_FAIL, &m_stencilFrontFail);
        glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &m_stencilFrontDepthFail);
        glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &m_stencilFrontDepthPass);
        glGetIntegerv(GL_STENCIL_BACK_FUNC, &m_stencilBackFunc);
        glGetIntegerv(GL_STENCIL_BACK_REF, &m_stencilBackRef);
        glGetIntegerv(GL_STENCIL_BACK_VALUE_MASK, &m_stencilBackValueMask);
        glGetIntegerv(GL_STENCIL_BACK_WRITEMASK, &m_stencilBackWriteMask);
        glGetIntegerv(GL_STENCIL_BACK_FAIL, &m_stencilBackFail);
        glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_FAIL, &m_stencilBackDepthFail);
        glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS, &m_stencilBackDepthPass);

        m_alphaTestEnabled = glIsEnabled(GL_ALPHA_TEST) != GL_FALSE;
        glGetIntegerv(GL_ALPHA_TEST_FUNC, &m_alphaFunc);
        glGetFloatv(GL_ALPHA_TEST_REF, &m_alphaRef);
        m_fogEnabled = glIsEnabled(GL_FOG) != GL_FALSE;
        m_scissorEnabled = glIsEnabled(GL_SCISSOR_TEST) != GL_FALSE;
        glGetIntegerv(GL_POLYGON_MODE, m_polygonMode);

        m_captured = true;
    }

    bool m_captured = false;
    GLint m_drawFramebuffer = 0;
    GLint m_readFramebuffer = 0;
    GLint m_viewport[4] = {0, 0, 0, 0};
    GLdouble m_depthRange[2] = {0.0, 1.0};
    GLint m_scissorBox[4] = {0, 0, 0, 0};

    GLint m_program = 0;
    GLint m_vertexArray = 0;
    GLint m_arrayBuffer = 0;
    GLint m_elementArrayBuffer = 0;

    GLint m_activeTexture = GL_TEXTURE0;
    GLint m_textureUnitCount = 0;
    GLint m_texture2D[MaxTrackedTextureUnits] = {};
    GLint m_sampler[MaxTrackedTextureUnits] = {};
    GLboolean m_texture2DEnabled[MaxTrackedTextureUnits] = {};

    bool m_blendEnabled = false;
    GLint m_blendSrcRGB = GL_ONE;
    GLint m_blendDstRGB = GL_ZERO;
    GLint m_blendSrcAlpha = GL_ONE;
    GLint m_blendDstAlpha = GL_ZERO;
    GLint m_blendEquationRGB = GL_FUNC_ADD;
    GLint m_blendEquationAlpha = GL_FUNC_ADD;
    GLfloat m_blendColor[4] = {};
    GLboolean m_colorMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};

    bool m_depthTestEnabled = false;
    GLboolean m_depthWriteMask = GL_TRUE;
    GLint m_depthFunc = GL_LESS;

    bool m_cullFaceEnabled = false;
    GLint m_cullFaceMode = GL_BACK;
    GLint m_frontFace = GL_CCW;

    bool m_stencilEnabled = false;
    GLint m_stencilFrontFunc = GL_ALWAYS;
    GLint m_stencilFrontRef = 0;
    GLint m_stencilFrontValueMask = ~0;
    GLint m_stencilFrontWriteMask = ~0;
    GLint m_stencilFrontFail = GL_KEEP;
    GLint m_stencilFrontDepthFail = GL_KEEP;
    GLint m_stencilFrontDepthPass = GL_KEEP;
    GLint m_stencilBackFunc = GL_ALWAYS;
    GLint m_stencilBackRef = 0;
    GLint m_stencilBackValueMask = ~0;
    GLint m_stencilBackWriteMask = ~0;
    GLint m_stencilBackFail = GL_KEEP;
    GLint m_stencilBackDepthFail = GL_KEEP;
    GLint m_stencilBackDepthPass = GL_KEEP;

    bool m_alphaTestEnabled = false;
    GLint m_alphaFunc = GL_ALWAYS;
    GLfloat m_alphaRef = 0.0f;
    bool m_fogEnabled = false;
    bool m_scissorEnabled = false;
    GLint m_polygonMode[2] = {GL_FILL, GL_FILL};
};

#endif // MU_ENABLE_DILIGENT

#include "renderer/passes/PostprocessingPass.h"

ux3d::renderer::PostprocessingPass::PostprocessingPass(uint32_t _id, const core::string& _name)
    : RenderPass(RenderPassType::Postprocessing, "Postprocessing")
{
}

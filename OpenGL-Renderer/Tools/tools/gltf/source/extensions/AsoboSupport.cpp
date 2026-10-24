// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "gltf/extensions/AsoboSupport.h"

namespace ux3d::gltf::asobo {

const std::vector<SupportInfo>& supportTable()
{
    static const std::vector<SupportInfo> table = {
        {"ASOBO_animation_retargeting", Support::Metadata, "A7", ""},
        {"ASOBO_asset_optimized", Support::Metadata, "A7", ""},
        {"ASOBO_collision_object", Support::NotYetSupported, "A7", ""},
        {"ASOBO_extra_occlusion", Support::NotYetSupported, "A1", ""},
        {"ASOBO_extra_occlusion_texture", Support::NotYetSupported, "A1", ""},
        {"ASOBO_gizmo_object", Support::NotYetSupported, "A7", ""},
        {"ASOBO_macro_light", Support::NotYetSupported, "A3", ""},
        {"ASOBO_material_SSS", Support::NotYetSupported, "A3", ""},
        {"ASOBO_material_UV_options", Support::NotYetSupported, "A5", ""},
        {"ASOBO_material_alphamode_dither", Support::NotYetSupported, "A1", ""},
        {"ASOBO_material_anisotropic", Support::NotYetSupported, "A7", ""},
        {"ASOBO_material_anisotropic_v2", Support::NotYetSupported, "A7", ""},
        {"ASOBO_material_antialiasing_options", Support::NotYetSupported, "A7", ""},
        {"ASOBO_material_blend_gbuffer", Support::NotYetSupported, "A2", ""},
        {"ASOBO_material_clear_coat", Support::NotYetSupported, "A7", ""},
        {"ASOBO_material_clear_coat_v2", Support::NotYetSupported, "A7", ""},
        {"ASOBO_material_detail_map", Support::NotYetSupported, "A5", ""},
        {"ASOBO_material_disable_motion_blur", Support::NotApplicable, "A7", "Glider has no motion blur"},
        {"ASOBO_material_draw_order", Support::Rendered, "A1.4", "decals, glass and windshield sort by drawOrderOffset"},
        {"ASOBO_material_emissive", Support::NotYetSupported, "A1", ""},
        {"ASOBO_material_environment_occluder", Support::NotYetSupported, "A2", ""},
        {"ASOBO_material_fake_terrain", Support::NotYetSupported, "A2", ""},
        {"ASOBO_material_fresnel_fade", Support::NotYetSupported, "A4", ""},
        {"ASOBO_material_geometry_decal", Support::NotYetSupported, "A2", ""},
        {"ASOBO_material_ghost_effect", Support::NotYetSupported, "A4", ""},
        {"ASOBO_material_glass_v2", Support::NotYetSupported, "A4", ""},
        {"ASOBO_material_invisible", Support::Rendered, "A1.1", "hidden; switch off in Viewport Settings > MSFS Extensions"},
        {"ASOBO_material_iridescent", Support::NotYetSupported, "A7", ""},
        {"ASOBO_material_parallax_window", Support::NotYetSupported, "A5", ""},
        {"ASOBO_material_pearlescent", Support::NotYetSupported, "A7", ""},
        {"ASOBO_material_sail", Support::NotYetSupported, "A7", ""},
        {"ASOBO_material_shadow_options", Support::NotYetSupported, "A3", ""},
        {"ASOBO_material_windshield", Support::NotYetSupported, "A4", ""},
        {"ASOBO_material_windshield_v3", Support::NotYetSupported, "A4", ""},
        {"ASOBO_normal_map_convention", Support::Rendered, "A1.2", "DirectX convention flips normal-map green"},
        {"ASOBO_occlusion_strength", Support::NotYetSupported, "A1", ""},
        {"ASOBO_property_animation", Support::NotYetSupported, "A7", ""},
        {"ASOBO_softbody_mesh", Support::Metadata, "A7", ""},
        {"ASOBO_tags", Support::Metadata, "A7", ""},
        {"ASOBO_unique_id", Support::Metadata, "A7", ""},
        {"MSFT_texture_dds", Support::NotYetSupported, "A1", ""},
    };
    return table;
}

SupportInfo supportOf(std::string_view extensionName)
{
    for (const SupportInfo& info : supportTable()) {
        if (info.name == extensionName) {
            return info;
        }
    }
    return {extensionName, Support::NotYetSupported, "", ""};
}

const char* supportLabel(Support status)
{
    switch (status) {
        case Support::Rendered: return "Rendered";
        case Support::Partial: return "Partial";
        case Support::ViewerSetting: return "Viewer setting";
        case Support::Metadata: return "Metadata";
        case Support::NotApplicable: return "Not applicable";
        case Support::NotYetSupported: return "Not yet supported";
    }
    return "Not yet supported";
}

} // namespace ux3d::gltf::asobo

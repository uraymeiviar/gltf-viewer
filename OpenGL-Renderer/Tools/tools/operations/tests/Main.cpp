
#include "Helpers.h"

ux3d::operations::Library g_library;

int main(int argc, char** argv)
{
    auto schemas = gltf_schema::merged();
    auto gltfSchema = ux3d::reactor::schema::resolveRefs(gltf_schema::merged(), "glTF.schema.json");
    auto linearizedSchema = ux3d::reactor::schema::linearize(gltfSchema);
    g_library.loadPlugin(ux3d::operations::gltfOperations(gltfSchema, linearizedSchema));

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
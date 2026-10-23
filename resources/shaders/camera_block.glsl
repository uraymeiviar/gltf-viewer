// Per-frame camera values, uploaded once per scene draw (P8.1b). The binding point
// is set by gl::Program (kCameraBlockBinding). Member names match the former uniforms.
layout(std140) uniform CameraBlock
{
    mat4 u_ViewMatrix;
    mat4 u_ProjectionMatrix;
    mat4 u_ViewProjectionMatrix;
    vec3 u_Camera;
};

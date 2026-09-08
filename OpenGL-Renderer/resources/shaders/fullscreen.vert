// Copyright (c) 2019 UX3D GmbH. All rights reserved.

out vec4 v_Position;
out vec2 v_outUV;

void main()
{
    float x = -1 + float((gl_VertexID & 1) << 2);
    float y = -1 + float((gl_VertexID & 2) << 1);	

    v_outUV.x = (x + 1) * 0.5;
    v_outUV.y = (y + 1) * 0.5;

    v_Position = vec4(x, y, 0, 1);
    gl_Position = vec4(x, y, 0, 1);
}

#version 460 core
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_ARB_bindless_texture : require

layout(location = 0) out vec4 FragColor;

in vec2 uv;
in flat uint64_t textureHandle;
in flat float textureIndex;
in flat uint colorTint;

in flat float radius;
in flat vec2 center;
in vec2 position;

void main() {
	#if defined(GL_ARB_bindless_texture)
        if (textureHandle != 0ul) {
            sampler2DArray s = sampler2DArray(textureHandle);
            FragColor = texture(s, vec3(uv, textureIndex)) * unpackUnorm4x8(colorTint);
        } else {
            //FragColor = unpackUnorm4x8(colorTint);

			float dist = distance(position, center);
    		//float alpha = 1.0 - smoothstep(radius * 0.989, radius * 1.01, dist);
			
			float delta = fwidth(dist);
			float alpha = 1.0 - smoothstep(radius - delta, radius + delta, dist);

			FragColor = unpackUnorm4x8(colorTint);
    		FragColor.w = alpha;
        }
    #else
        FragColor = unpackUnorm4x8(colorTint);
    #endif
}
#version 330

uniform float u_time;
uniform vec3 u_eye;

in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_eyeDir;
in vec4 v_uv_shadow;
in vec4 v_uv_gradient;

uniform sampler2D s_cookieTex;
uniform sampler2D s_gradientTex;

out vec4 f_color;

vec4 tex1Dproj( sampler1D sampler, vec2 texcoord ) { return textureProj( sampler, texcoord ); }
vec4 tex2Dproj( sampler2D sampler, vec3 texcoord ) { return textureProj( sampler, texcoord ); }
vec4 tex3Dproj( sampler3D sampler, vec4 texcoord ) { return textureProj( sampler, texcoord ); }

void main()
{

	vec4 texS = vec4(0);

	// in front? 
	//if (v_uv_shadow.w > 0)
	{

	    //vec2 ttc = vec2(v_uv_shadow.xy / v_uv_shadow.w);

	    //vec4 texS = tex2Dproj(s_cookieTex, vec3(v_uv_shadow.xyz)); // v_uv_shadow.xyw
	    texS = tex2Dproj(s_cookieTex, v_uv_shadow.xyw);

	    //texS = texture(s_cookieTex, ttc);

	    texS.a = 1.0 - texS.a;

	}

    //vec4 texF = tex2Dproj(s_gradientTex, vec3(v_uv_gradient.xyw));
    //vec4 res = mix(vec4(1,1,1,0), texS, texF.a);

    f_color = texS; //vec4(1, 0, 1, 1);
}
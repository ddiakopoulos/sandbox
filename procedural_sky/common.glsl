
vec3 textureNormal(sampler2DArray s, vec2 uv, float layer, float strength)
{
    vec2 xy = texture(s, vec3(uv, layer)).xy * 2 - 1;
    
    xy *= strength;
    
    return vec3(xy, sqrt(max(0, 1 - dot(xy, xy))));
}

vec3 degamma(vec3 v)
{
    return pow(v, vec3(2.2));
}

vec3 gamma(vec3 v)
{
    return pow(v, vec3(1 / 2.2));
}

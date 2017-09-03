#define TWO_CASCADES // HACK HACK include system doesn't handle defines in one include being used in another. hmm. 

#ifdef FOUR_CASCADES
vec4 get_cascade_weights(float depth, vec4 splitNear, vec4 splitFar)
{
    return (step(splitNear, vec4(depth))) * (step(depth, splitFar));
}

mat4 get_cascade_viewproj(vec4 weights, mat4 viewProj[4])
{
    return viewProj[0] * weights.x + viewProj[1] * weights.y + viewProj[2] * weights.z + viewProj[3] * weights.w;
}

float get_cascade_layer(vec4 weights) 
{
    return 0.0 * weights.x + 1.0 * weights.y + 2.0 * weights.z + 3.0 * weights.w;   
}

vec3 get_cascade_weighted_color(vec4 weights) 
{
    return vec3(1,0,0) * weights.x + vec3(0,1,0) * weights.y + vec3(0,0,1) * weights.z + vec3(1,0,1) * weights.w;
}
#endif

#ifdef TWO_CASCADES
vec2 get_cascade_weights(float depth, vec2 splitNear, vec2 splitFar)
{
    return (step(splitNear, vec2(depth))) * (step(depth, splitFar)); 
}

mat4 get_cascade_viewproj(vec2 weights, mat4 viewProj[2])
{
    return viewProj[0] * weights.x + viewProj[1] * weights.y;
}

float get_cascade_layer(vec2 weights) 
{
    return 0.0 * weights.x +  1.0 * weights.y; 
}

vec3 get_cascade_weighted_color(vec2 weights) 
{
    return vec3(1,0,0) * weights.x + vec3(0,1,0) * weights.y;
}
#endif


float calculate_csm_coefficient(sampler2DArray map, vec3 worldPos, vec3 viewPos, mat4 viewProjArray[NUM_CASCADES], vec4 splitPlanes[NUM_CASCADES], out vec3 weightedColor)
{
    #ifdef FOUR_CASCADES
    vec4 weights = get_cascade_weights(-viewPos.z,
        vec4(splitPlanes[0].x, splitPlanes[1].x, splitPlanes[2].x, splitPlanes[3].x),
        vec4(splitPlanes[0].y, splitPlanes[1].y, splitPlanes[2].y, splitPlanes[3].y)
    );
    #endif

    #ifdef TWO_CASCADES
    vec2 weights = get_cascade_weights(-viewPos.z,
        vec2(splitPlanes[0].x, splitPlanes[1].x),
        vec2(splitPlanes[0].y, splitPlanes[1].y));
    #endif

    // Get vertex position in light space
    mat4 lightViewProj = get_cascade_viewproj(weights, viewProjArray);
    vec4 vertexLightPostion = lightViewProj * vec4(worldPos, 1.0);

    // Compute perspective divide and transform to 0-1 range
    vec3 coords = (vertexLightPostion.xyz / vertexLightPostion.w) / 2.0 + 0.5;

    if (!(coords.z > 0.0 && coords.x > 0.0 && coords.y > 0.0 && coords.x <= 1.0 && coords.y <= 1.0)) return 0;

    float bias = 0.004;
    float currentDepth = coords.z;

    float shadowTerm = 0.0;

    // Non-PCF path, hard shadows
    float closestDepth = texture(map, vec3(coords.xy, get_cascade_layer(weights))).r;
    shadowTerm = currentDepth - bias > closestDepth ? 1.0 : 0.0;

    // Percentage-closer filtering
    vec2 texelSize = 1.0 / textureSize(map, 0).xy;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(map, vec3(coords.xy + vec2(x, y) * texelSize, get_cascade_layer(weights))).r;
            shadowTerm += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;
        }
    }
    shadowTerm /= 9.0;

    weightedColor = get_cascade_weighted_color(weights);

    /*
    // Exponential Shadow Filtering
    const float overshadowConstant = 140;
    float depth = (coords.z + bias);
    float occluderDepth = texture(map, vec3(coords.xy, get_cascade_layer(weights))).r;
    float occluder = exp(overshadowConstant * occluderDepth);
    float receiver = exp(-overshadowConstant * depth);
    shadowTerm = 1.0 - clamp(occluder * receiver, 0.0, 1.0);
    */

    return shadowTerm;
}

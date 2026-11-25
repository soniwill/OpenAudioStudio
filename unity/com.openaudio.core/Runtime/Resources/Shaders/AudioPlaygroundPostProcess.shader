Shader "Hidden/OpenAudio/PlaygroundPostProcess"
{
    Properties
    {
        _MainTex ("Source", 2D) = "white" {}
    }

    CGINCLUDE
    #include "UnityCG.cginc"

    sampler2D _MainTex;
    float4 _MainTex_TexelSize;
    sampler2D _BloomTex;

    float _BloomThreshold;
    float _BloomSoftKnee;
    float _BloomIntensity;
    float _Exposure;
    float _VignetteIntensity;
    float _VignetteSmoothness;
    float _ChromaticAberration;
    float _Contrast;
    float _Saturation;

    struct v2f
    {
        float4 pos : SV_POSITION;
        float2 uv : TEXCOORD0;
    };

    v2f vert(appdata_img v)
    {
        v2f o;
        o.pos = UnityObjectToClipPos(v.vertex);
        o.uv = v.texcoord;
        return o;
    }

    float3 ACESFilmic(float3 x)
    {
        const float a = 2.51;
        const float b = 0.03;
        const float c = 2.43;
        const float d = 0.59;
        const float e = 0.14;
        return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
    }

    float LuminanceRec709(float3 c)
    {
        return dot(c, float3(0.2126, 0.7152, 0.0722));
    }

    half4 fragBright(v2f i) : SV_Target
    {
        float3 c = tex2D(_MainTex, i.uv).rgb;
        c = min(c, 12.0);
        float br = max(c.r, max(c.g, c.b));
        float knee = _BloomThreshold * _BloomSoftKnee + 1e-5;
        float soft = clamp(br - _BloomThreshold + knee, 0.0, 2.0 * knee);
        soft = (soft * soft) / (4.0 * knee + 1e-5);
        float contrib = max(soft, br - _BloomThreshold) / max(br, 1e-5);
        return half4(c * contrib, 1.0);
    }

    half4 fragBlurH(v2f i) : SV_Target
    {
        float2 stepUv = float2(_MainTex_TexelSize.x * 1.5, 0.0);
        float3 sum = tex2D(_MainTex, i.uv).rgb * 0.227027;
        sum += (tex2D(_MainTex, i.uv + stepUv * 1.0).rgb + tex2D(_MainTex, i.uv - stepUv * 1.0).rgb) * 0.1945946;
        sum += (tex2D(_MainTex, i.uv + stepUv * 2.0).rgb + tex2D(_MainTex, i.uv - stepUv * 2.0).rgb) * 0.1216216;
        sum += (tex2D(_MainTex, i.uv + stepUv * 3.0).rgb + tex2D(_MainTex, i.uv - stepUv * 3.0).rgb) * 0.054054;
        sum += (tex2D(_MainTex, i.uv + stepUv * 4.0).rgb + tex2D(_MainTex, i.uv - stepUv * 4.0).rgb) * 0.016216;
        return half4(sum, 1.0);
    }

    half4 fragBlurV(v2f i) : SV_Target
    {
        float2 stepUv = float2(0.0, _MainTex_TexelSize.y * 1.5);
        float3 sum = tex2D(_MainTex, i.uv).rgb * 0.227027;
        sum += (tex2D(_MainTex, i.uv + stepUv * 1.0).rgb + tex2D(_MainTex, i.uv - stepUv * 1.0).rgb) * 0.1945946;
        sum += (tex2D(_MainTex, i.uv + stepUv * 2.0).rgb + tex2D(_MainTex, i.uv - stepUv * 2.0).rgb) * 0.1216216;
        sum += (tex2D(_MainTex, i.uv + stepUv * 3.0).rgb + tex2D(_MainTex, i.uv - stepUv * 3.0).rgb) * 0.054054;
        sum += (tex2D(_MainTex, i.uv + stepUv * 4.0).rgb + tex2D(_MainTex, i.uv - stepUv * 4.0).rgb) * 0.016216;
        return half4(sum, 1.0);
    }

    half4 fragComposite(v2f i) : SV_Target
    {
        float2 centered = i.uv - 0.5;
        float distSq = dot(centered, centered);
        float2 caOffset = centered * (_ChromaticAberration * distSq);

        float3 col;
        col.r = tex2D(_MainTex, i.uv + caOffset).r;
        col.g = tex2D(_MainTex, i.uv).g;
        col.b = tex2D(_MainTex, i.uv - caOffset).b;

        float3 bloom = tex2D(_BloomTex, i.uv).rgb;
        col += bloom * _BloomIntensity;

        col *= _Exposure;

        float lumaPre = LuminanceRec709(col);
        float3 coolShadow = float3(0.92, 0.96, 1.06);
        float3 warmHighlight = float3(1.04, 1.00, 0.94);
        col *= lerp(coolShadow, warmHighlight, saturate(lumaPre * 0.85));

        col = ACESFilmic(col);

        float lumaPost = LuminanceRec709(col);
        col = lerp(lumaPost.xxx, col, _Saturation);
        col = saturate((col - 0.5) * _Contrast + 0.5);

        float vigDist = length(centered * float2(1.15, 1.0));
        float vignette = smoothstep(0.85, 0.85 - _VignetteSmoothness, vigDist);
        col *= lerp(1.0, vignette, _VignetteIntensity);

        float dither = frac(sin(dot(i.uv * _ScreenParams.xy, float2(12.9898, 78.233))) * 43758.5453);
        col += (dither - 0.5) / 255.0;

        return half4(saturate(col), 1.0);
    }
    ENDCG

    SubShader
    {
        Cull Off ZWrite Off ZTest Always

        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment fragBright
            ENDCG
        }

        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment fragBlurH
            ENDCG
        }

        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment fragBlurV
            ENDCG
        }

        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment fragComposite
            ENDCG
        }
    }
}

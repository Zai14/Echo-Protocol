#version 330

// Echo Protocol — darkness overlay shader.
//
// Crushes the rendered scene to near-black, keeping only a faint
// personal-visibility bubble around the player as they navigate
// by sonar.
//
// The atmosphere is built from:
//   - Strong vignette that tightens around the player
//   - Subtle CRT interference bands that drift vertically
//   - Increased noise/static, especially at screen edges
//   - A slight blue-green colour shift at the periphery (CRT phosphor)
//   - A slow, subtle flicker to make the world feel electrically alive
//   - Sonar reveal pulses + ring-edge glow for the signature mechanic
//   - Brief screen-wide flash on pulse emit
//
// The result: empty corridors feel oppressive even when nothing is
// happening. The darkness itself is a character.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;

uniform vec2  resolution;
uniform vec2  playerPos;
uniform float visibilityRadius;
uniform float time;

#define MAX_SONAR_PULSES 8
uniform int  sonarPulseCount;
uniform vec4 sonarPulses[MAX_SONAR_PULSES];

uniform float sonarFlash;

out vec4 finalColor;

/* ------------------------------------------------------------------ */
/*  Noise helpers                                                      */
/* ------------------------------------------------------------------ */

float Hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float Hash1(float n)
{
    return fract(sin(n * 127.1 + 311.7) * 43758.5453);
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

void main()
{
    vec4 sceneColor = texture(texture0, fragTexCoord) * fragColor;

    vec2 pixelPos = fragTexCoord * resolution;
    vec2 ndc      = fragTexCoord * 2.0 - 1.0;

    // ----- Darkness composition -----
    float darkness = 0.030;  // baseline is almost completely black

    // Personal visibility bubble around the player.
    float distToPlayer = length(pixelPos - playerPos);
    float personalGlow = 1.0 - smoothstep(0.0, visibilityRadius, distToPlayer);
    darkness += personalGlow * 0.22;

    // Sonar reveal pulses.
    for (int i = 0; i < MAX_SONAR_PULSES; i++)
    {
        if (i >= sonarPulseCount) break;
        vec4 pulse = sonarPulses[i];
        float d = length(pixelPos - pulse.xy);
        float reveal = 1.0 - smoothstep(0.0, pulse.z, d);
        darkness += reveal * pulse.w;

        // Ring-edge glow at the wavefront.
        float ringDist = abs(d - pulse.z);
        float ringEdge = 1.0 - smoothstep(0.0, 16.0, ringDist);
        darkness += ringEdge * pulse.w * 0.40;
    }
    darkness = clamp(darkness, 0.0, 1.0);

    // ----- Global flash on pulse emit (negative = power failure darken) -----
    if (sonarFlash >= 0.0) {
        darkness += sonarFlash * 0.50;
    } else {
        // Power failure: crush brightness toward zero
        darkness *= (1.0 + sonarFlash) * 2.0;  // sonarFlash = -0.4 → * 1.2 dim
        darkness = clamp(darkness + sonarFlash * 0.30, 0.0, 1.0);
    }
    darkness = clamp(darkness, 0.0, 1.0);

    // ----- Vignette (tighter, more oppressive) -----
    float vigPow = 0.72;  // higher = stronger darkening at edges
    float vignette = 1.0 - dot(ndc, ndc) * vigPow;
    vignette = clamp(vignette, 0.0, 1.0);

    // ----- CRT interference bands (slowly drifting horizontal lines) -----
    float scanA = sin(fragTexCoord.y * resolution.y * 0.08 + time * 0.4) * 0.5 + 0.5;
    float scanB = sin(fragTexCoord.y * resolution.y * 0.13 + time * 0.6 + 1.2) * 0.5 + 0.5;
    float crtBand = (scanA * 0.12 + scanB * 0.08);
    // Bands are darker (reduce darkness), creating interference lines.
    darkness -= crtBand * 0.06;
    darkness  = clamp(darkness, 0.0, 1.0);

    // ----- Edge static (more noise near screen borders) -----
    float edgeFactor = 1.0 - smoothstep(0.1, 0.5, length(ndc));
    float noiseSeed = Hash12(pixelPos * 0.7 + vec2(time * 53.0, time * 41.0));
    float edgeNoise = edgeFactor * (noiseSeed - 0.5) * 0.06;

    // ----- Subtle flicker (global brightness variation) -----
    float flicker = 1.0 + sin(time * 3.7 + Hash1(floor(time * 0.5))) * 0.015;
    flicker += sin(time * 7.1) * 0.005;

    // ----- Central noise (fine-grain texture, like CRT phosphor) -----
    float centralNoise = Hash12(pixelPos * 0.75 + vec2(time * 60.0, time * 37.0));
    centralNoise = (centralNoise - 0.5) * 0.030;

    // ----- Slight blue-green colour shift at edges (CRT phosphor glow) -----
    float colourShift = 1.0 - vignette;
    vec3 edgeTint = vec3(0.85, 1.0, 1.05);  // slight cyan/green bias

    // ----- Composite -----
    vec3 litColor = sceneColor.rgb * darkness * vignette * flicker;
    litColor += centralNoise + edgeNoise;

    // Apply edge colour tint.
    litColor = mix(litColor, litColor * edgeTint, colourShift * 0.25);

    finalColor = vec4(clamp(litColor, 0.0, 1.0), 1.0);
}

varying vec2 uv;

uniform float opacity;
uniform sampler2D tex;

float clamp_to_border_factor(vec2 coords) {
    bvec2 out1 = greaterThan(coords, vec2(1, 1));
    bvec2 out2 = lessThan(coords, vec2(0,0));
    bool do_clamp = (any(out1) || any(out2));

    return float(!do_clamp);
}

void main() {
   vec4 pixel = texture2D(tex, uv);

   gl_FragColor = vec4(pixel.rgb, pixel.a * opacity * clamp_to_border_factor(uv));
 }

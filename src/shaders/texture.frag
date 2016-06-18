varying vec2 uv;
varying float outopacity;

uniform sampler2D tex;

void main() {
   vec4 pixel = texture2D(tex, uv);

   gl_FragColor = vec4(pixel.rgb, pixel.a * outopacity);
 }

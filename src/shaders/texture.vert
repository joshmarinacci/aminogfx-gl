uniform mat4 mvp;
uniform mat4 trans;

attribute vec4 pos;
attribute vec2 texcoords;

varying vec2 uv;

void main() {
   gl_Position = mvp * trans * pos;
   uv = texcoords;
}

uniform mat4 mvp;
uniform mat4 trans;

attribute vec4 pos;

void main() {
   gl_Position = mvp * trans * pos;
}

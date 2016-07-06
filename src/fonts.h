#ifndef _AMINOFONTS_H
#define _AMINOFONTS_H

#include "freetype-gl.h"
#include "vertex-buffer.h"

#include <map>

#include "gfx.h"

/**
 * Font class.
 */
class AminoFont {
public:
    int id;

    //font
    texture_atlas_t *atlas;
    std::map<int, texture_font_t *> fonts; //by font size
    const char *filename;

    //shader
    GLuint shader;
    GLint texuni;
    GLint mvpuni;
    GLint transuni;
    GLint coloruni;

    AminoFont() {
        texuni = -1;
    }

    virtual ~AminoFont() {
        //destroy (if not called before)
        destroy();
    }

    /**
     * Free all resources.
     */
    virtual void destroy() {
        //textures
        for (std::map<int, texture_font_t *>::iterator it = fonts.begin(); it != fonts.end(); it++) {
            if (DEBUG_RESOURCES) {
                printf("freeing font texture\n");
            }

            texture_font_delete(it->second);
        }

        fonts.clear();

        //atlas
        if (atlas) {
            if (DEBUG_RESOURCES) {
                printf("freeing font\n");
            }

            texture_atlas_delete(atlas);
            atlas = NULL;
        }
    }
};

#endif
#ifndef _AMINO_RPI_H
#define _AMINO_RPI_H

#include "base.h"
#include "renderer.h"

class AminoGfxRPiFactory : public AminoJSObjectFactory {
public:
    AminoGfxRPiFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

class AminoOmxVideoPlayer : public AminoVideoPlayer {
public:
    AminoOmxVideoPlayer(AminoTexture *texture, AminoVideo *video);

    void init();
};

#endif

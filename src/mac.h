#ifndef _AMINO_MAC_H
#define _AMINO_MAC_H

#include "base.h"
#include "renderer.h"

class AminoGfxMacFactory : public AminoJSObjectFactory {
public:
    AminoGfxMacFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

class AminoMacVideoPlayer : public AminoVideoPlayer {
public:
    AminoMacVideoPlayer(AminoTexture *texture, AminoVideo *video);

    void init();
};

#endif

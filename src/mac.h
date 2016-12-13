#ifndef _AMINO_MAC_H
#define _AMINO_MAC_H

#include "base.h"
#include "renderer.h"

/**
 * AminoGfxMac factory.
 */
class AminoGfxMacFactory : public AminoJSObjectFactory {
public:
    AminoGfxMacFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * Mac video player.
 */
class AminoMacVideoPlayer : public AminoVideoPlayer {
public:
    AminoMacVideoPlayer(AminoTexture *texture, AminoVideo *video);

    bool initStream() override;
    void init() override;
    void initVideoTexture() override;

private:
    std::string filename;
};

#endif

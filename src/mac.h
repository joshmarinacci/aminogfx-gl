#ifndef _AMINO_MAC_H
#define _AMINO_MAC_H

#include "base.h"
#include "SimpleRenderer.h"

class AminoGfxMacFactory : public AminoJSObjectFactory {
public:
    AminoGfxMacFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

#endif

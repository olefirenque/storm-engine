#pragma once

#include "dx9render.h"
#include "v_module_api.h"

class xiBaseVideo : public Entity
{
  public:
    virtual void SetShowVideo(bool bShowVideo)
    {
    }

    virtual IDirect3DTexture9 *GetCurrentVideoTexture()
    {
        return nullptr;
    }
};

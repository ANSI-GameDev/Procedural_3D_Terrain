#pragma once

#include "AppBase.h"
#include "GrassModel.h"

namespace ansi {

class GrassDemo : public AppBase {
  public:
    GrassDemo();

    virtual bool InitScene() override;
    virtual void UpdateGUI() override;
    virtual void Update(float dt) override;
    virtual void Render() override;

  protected:
    int m_volumeWidth = 200;
    int m_volumeHeight = 200;
    int m_volumeDepth = 200;
    int m_lightWidth = 200 / 4; // ∂Û¿Ã∆Æ∏ ¿∫ ≥∑¿∫ «ÿªÛµµ
    int m_lightHeight = 200 / 4;
    int m_lightDepth = 200 / 4;

    shared_ptr<Model> m_volumeModel;

    ComPtr<ID3D11ComputeShader> m_cloudDensityCS;
    ComPtr<ID3D11ComputeShader> m_cloudLightingCS;

    VolumeConsts m_volumeConstsCpu;
    ComPtr<ID3D11Buffer> m_volumeConstsGpu;

    shared_ptr<Model> m_ground;
    shared_ptr<GrassModel> m_grass;
    float m_windStrength = 1.0f;
};

} // namespace 

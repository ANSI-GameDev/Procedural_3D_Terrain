#include "CloudDemo.h"
#include "GeometryGenerator.h"
#include "GraphicsCommon.h"
#include "Noise.h"

#include <glm/glm.hpp>
#include <glm/simd/common.h>

namespace ansi {

using namespace std;
using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace glm;

CloudDemo::CloudDemo() : AppBase() {}

bool CloudDemo::InitScene() {

    cout << "CloudDemo::InitScene()" << endl;

    AppBase::m_camera.Reset(Vector3(0.0f, 0.0f, -2.5f), 0.0f, 0.0f);

    // https://polyhaven.com/a/syferfontein_18d_clear_puresky
    AppBase::InitCubemaps(
        L"../Assets/Textures/Cubemaps/HDRI/", L"clear_pureskyEnvHDR.dds",
        L"clear_pureskySpecularHDR.dds", L"clear_pureskyDiffuseHDR.dds",
        L"clear_pureskyBrdf.dds");

    AppBase::InitScene();

    //// �ٴ�(�ſ�)
    //{
    //    // https://freepbr.com/materials/stringy-marble-pbr/
    //    auto mesh = GeometryGenerator::MakeSquare(5.0, {10.0f, 10.0f});
    //    string path = "../Assets/Textures/a/";
    //    mesh.albedoTextureFilename = path + "grid.png";
    //    mesh.emissiveTextureFilename = "";
    //    mesh.aoTextureFilename = path + "grid.png";
    //    mesh.metallicTextureFilename = path + "grid.png";
    //    mesh.normalTextureFilename = path + "grid.png";
    //    mesh.roughnessTextureFilename = path + "grid.png";

    //    m_ground = make_shared<Model>(m_device, m_context, vector{mesh});
    //    m_ground->m_materialConsts.GetCpu().albedoFactor = Vector3(0.7f);
    //    m_ground->m_materialConsts.GetCpu().emissionFactor = Vector3(0.0f);
    //    m_ground->m_materialConsts.GetCpu().metallicFactor = 0.5f;
    //    m_ground->m_materialConsts.GetCpu().roughnessFactor = 0.3f;
    //    m_ground->m_name = "Ground";

    //    Vector3 position = Vector3(0.0f, -0.5f, 2.0f);
    //    m_ground->UpdateWorldRow(Matrix::CreateRotationX(3.141592f * 0.5f) *
    //                             Matrix::CreateTranslation(position));

    //    //m_mirrorPlane = SimpleMath::Plane(position, Vector3(0.0f, 1.0f, 0.0f));
    //    //m_mirror = m_ground; // �ٴڿ� �ſ�ó�� �ݻ� ����

    //    m_basicList.push_back(m_ground); // �ſ��� ����Ʈ�� ��� X
    //}

    // Volume ���̴��� ������ �ʱ�ȭ
    Graphics::InitVolumeShaders(m_device);

    m_globalConstsCPU.strengthIBL = 0.3f;

    Vector3 center(0.0f, 60.0f, 0.0f);
    m_volumeModel = make_shared<Model>(
        m_device, m_context, vector{GeometryGenerator::MakeBox(1.0f)});
    m_volumeModel->UpdateWorldRow(Matrix::CreateScale(60.0f, 20.0f, 60.0f) *
                                  Matrix::CreateTranslation(center));
    m_volumeModel->UpdateConstantBuffers(m_device, m_context);

    m_volumeModel->m_meshes.front()->densityTex.Initialize(
        m_device, m_volumeWidth, m_volumeHeight, m_volumeDepth,
        DXGI_FORMAT_R16_FLOAT, {});
    m_volumeModel->m_meshes.front()->lightingTex.Initialize(
        m_device, m_lightWidth, m_lightHeight, m_lightDepth,
        DXGI_FORMAT_R16_FLOAT, {});

    // Generate cloud volume data
    m_volumeConstsCpu.uvwOffset = Vector3(0.0f);
    D3D11Utils::CreateConstBuffer(m_device, m_volumeConstsCpu,
                                  m_volumeConstsGpu);
    D3D11Utils::CreateComputeShader(m_device, L"CloudDensityCS.hlsl",
                                    m_cloudDensityCS);
    D3D11Utils::CreateComputeShader(m_device, L"CloudLightingCS.hlsl",
                                    m_cloudLightingCS);

    m_context->CSSetConstantBuffers(0, 1, m_volumeConstsGpu.GetAddressOf());
    m_context->CSSetUnorderedAccessViews(
        0, 1, m_volumeModel->m_meshes.front()->densityTex.GetAddressOfUAV(),
        NULL);
    m_context->CSSetShader(m_cloudDensityCS.Get(), 0, 0);
    m_context->Dispatch(UINT(ceil(m_volumeWidth / 16.0f)),
                        UINT(ceil(m_volumeHeight / 16.0f)),
                        UINT(ceil(m_volumeDepth / 4.0f)));
    AppBase::ComputeShaderBarrier();

    m_context->CSSetShaderResources(
        0, 1, m_volumeModel->m_meshes.front()->densityTex.GetAddressOfSRV());
    m_context->CSSetUnorderedAccessViews(
        0, 1, m_volumeModel->m_meshes.front()->lightingTex.GetAddressOfUAV(),
        NULL);
    m_context->CSSetShader(m_cloudLightingCS.Get(), 0, 0);
    m_context->Dispatch(UINT(ceil(m_lightWidth / 16.0f)),
                        UINT(ceil(m_lightHeight / 16.0f)),
                        UINT(ceil(m_lightDepth / 4.0f)));
    AppBase::ComputeShaderBarrier();

    return true;
}

void CloudDemo::Update(float dt) {

    AppBase::Update(dt);

    m_volumeModel->UpdateConstantBuffers(m_device, m_context);

    static float offset = 0.0f;

    offset += 0.0005f; // �ִϸ��̼� ȿ��

    m_volumeConstsCpu.uvwOffset.z = offset;
    D3D11Utils::UpdateBuffer(m_context, m_volumeConstsCpu, m_volumeConstsGpu);

    m_context->CSSetConstantBuffers(0, 1, m_volumeConstsGpu.GetAddressOf());
    m_context->CSSetUnorderedAccessViews(
        0, 1, m_volumeModel->m_meshes.front()->densityTex.GetAddressOfUAV(),
        NULL);
    m_context->CSSetShader(m_cloudDensityCS.Get(), 0, 0);
    m_context->Dispatch(UINT(ceil(m_volumeWidth / 16.0f)),
                        UINT(ceil(m_volumeHeight / 16.0f)),
                        UINT(ceil(m_volumeDepth / 4.0f)));
    AppBase::ComputeShaderBarrier();

    m_context->CSSetShaderResources(
        0, 1, m_volumeModel->m_meshes.front()->densityTex.GetAddressOfSRV());
    m_context->CSSetUnorderedAccessViews(
        0, 1, m_volumeModel->m_meshes.front()->lightingTex.GetAddressOfUAV(),
        NULL);
    m_context->CSSetShader(m_cloudLightingCS.Get(), 0, 0);
    m_context->Dispatch(UINT(ceil(m_lightWidth / 16.0f)),
                        UINT(ceil(m_lightHeight / 16.0f)),
                        UINT(ceil(m_lightDepth / 4.0f)));
    AppBase::ComputeShaderBarrier();
}

void CloudDemo::Render() {
    AppBase::Render();

    AppBase::SetPipelineState(Graphics::volumeSmokePSO);
    m_context->PSSetConstantBuffers(3, 1, m_volumeConstsGpu.GetAddressOf());
    m_volumeModel->Render(m_context);

    AppBase::PostRender();
}

void CloudDemo::UpdateGUI() {
    AppBase::UpdateGUI();

    static float lightAngle = 90.0f;
    if (ImGui::SliderFloat("LightAngle", &lightAngle, 0.0f, 180.0f)) {
        m_volumeConstsCpu.lightDir.x = glm::cos(glm::radians(lightAngle));
        m_volumeConstsCpu.lightDir.y = glm::sin(glm::radians(lightAngle));
    }

    ImGui::SliderFloat("LightAbsorption", &m_volumeConstsCpu.lightAbsorption,
                       0.0f, 100.0f);

    ImGui::SliderFloat("DensityAbsorption",
                       &m_volumeConstsCpu.densityAbsorption, 0.0f, 100.0f);

    static float lightScale = 40.0f;
    if (ImGui::SliderFloat("lightScale", &lightScale, 0.0f, 400.0f)) {
        m_volumeConstsCpu.lightColor = Vector3(1.0f) * lightScale;
    }

    ImGui::SliderFloat("Aniso", &m_volumeConstsCpu.aniso, 0.0f, 1.0f);
}

} // namespace 

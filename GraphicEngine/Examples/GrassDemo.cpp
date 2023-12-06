#include "GrassDemo.h"
#include "GeometryGenerator.h"
#include "GraphicsCommon.h"

#include <random>

namespace ansi {

using namespace std;
using namespace DirectX;
using namespace DirectX::SimpleMath;

GrassDemo::GrassDemo() : AppBase() {}

bool GrassDemo::InitScene() {

    cout << "GrassDemo::InitScene()" << endl;

    AppBase::m_camera.Reset(Vector3(0.0f, 1.0f, -2.5f), 0.0f, 0.0f);

    AppBase::InitCubemaps(
        L"../Assets/Textures/Cubemaps/HDRI/", L"clear_pureskyEnvHDR.dds",
        L"clear_pureskySpecularHDR.dds", L"clear_pureskyDiffuseHDR.dds",
        L"clear_pureskyBrdf.dds");

    AppBase::m_globalConstsCPU.strengthIBL = 1.0f;

    AppBase::InitScene();

    // �ٴ�(�ſ�)
    {
        // https://freepbr.com/materials/stringy-marble-pbr/
        auto mesh = GeometryGenerator::MakeSquare(10.0);
        string path = "../Assets/Textures/PBR/stringy-marble-ue/";
        mesh.albedoTextureFilename = path + "stringy_marble_albedo.png";
        mesh.emissiveTextureFilename = "";
        mesh.aoTextureFilename = path + "stringy_marble_ao.png";
        mesh.metallicTextureFilename = path + "stringy_marble_Metallic.png";
        mesh.normalTextureFilename = path + "stringy_marble_Normal-dx.png";
        mesh.roughnessTextureFilename = path + "stringy_marble_Roughness.png";

        m_ground = make_shared<Model>(m_device, m_context, vector{mesh});
        m_ground->m_materialConsts.GetCpu().albedoFactor = Vector3(0.2f);
        m_ground->m_materialConsts.GetCpu().emissionFactor = Vector3(0.0f);
        m_ground->m_materialConsts.GetCpu().metallicFactor = 0.5f;
        m_ground->m_materialConsts.GetCpu().roughnessFactor = 0.3f;
        m_ground->m_castShadow = false;

        Vector3 position = Vector3(0.0f, 0.0f, 2.0f);
        m_ground->UpdateWorldRow(Matrix::CreateRotationX(3.141592f * 0.5f) *
                                 Matrix::CreateTranslation(position));

        m_mirrorPlane = SimpleMath::Plane(position, Vector3(0.0f, 1.0f, 0.0f));
        m_mirror = m_ground; // �ٴڿ� �ſ�ó�� �ݻ� ����

        // m_basicList.push_back(m_ground); // �ſ��� ����Ʈ�� ��� X
    }

    // Grass object
    {
        m_grass = make_shared<GrassModel>();
        m_basicList.push_back(m_grass);

        // Instances �����

        std::mt19937 gen(0);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        vector<GrassInstance> &grassInstances = m_grass->m_instancesCpu;

        for (int i = 0; i < 400000; i++) {
            const float lengthScale = dist(gen) * 0.7f + 0.3f;
            const float widthScale = 0.5f + dist(gen) * 0.5f;
            const Vector3 pos = Vector3(dist(gen) * 2.0f - 1.0f, 0.0f,
                                        dist(gen) * 2.0f - 1.0f) *
                                20.0f;
            const float angle = dist(gen) * 3.141592f; // �ٶ󺸴� ����
            const float slope =
                (dist(gen) - 0.5f) * 2.0f * 3.141592f * 0.2f; // �⺻ ����

            GrassInstance gi;
            gi.instanceWorld =
                Matrix::CreateRotationX(slope) *
                Matrix::CreateRotationY(angle) *
                Matrix::CreateScale(widthScale, lengthScale, 1.0f) *
                Matrix::CreateTranslation(pos);
            gi.windStrength = m_windStrength;

            grassInstances.push_back(gi);
        }

        // ���̴��� ������ ���� transpose
        for (auto &i : grassInstances) {
            i.instanceWorld = i.instanceWorld.Transpose();
        }

        m_grass->Initialize(m_device, m_context);
        m_grass->UpdateWorldRow(Matrix::CreateScale(0.5f) *
                                Matrix::CreateTranslation(0.0f, 0.0f, 2.0f));
    }

    // ����
    // Volume ���̴��� ������ �ʱ�ȭ
    Graphics::InitVolumeShaders(m_device);

    m_globalConstsCPU.strengthIBL = 0.3f;

    Vector3 center(-30.0f, 50.0f, -60.0f);
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

void GrassDemo::Update(float dt) {
    AppBase::Update(dt);

    for (auto &i : m_grass->m_instancesCpu) {
        i.windStrength = m_windStrength;
    }

    D3D11Utils::UpdateBuffer(m_context, m_grass->m_instancesCpu,
                             m_grass->m_instancesGpu);

    // ����
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

void GrassDemo::Render() {
    AppBase::Render();

    // ����
    AppBase::SetPipelineState(Graphics::volumeSmokePSO);
    m_context->PSSetConstantBuffers(3, 1, m_volumeConstsGpu.GetAddressOf());
    m_volumeModel->Render(m_context);

    AppBase::PostRender();
}

void GrassDemo::UpdateGUI() {
    AppBase::UpdateGUI();

    ImGui::SliderFloat("Wind", &m_windStrength, -3.0f, 3.0f);

    // if (ImGui::SliderFloat("WindTrunk",
    //                        &m_leaves->m_meshConsts.GetCpu().windTrunk, 0.0f,
    //                        0.2f)) {
    //     m_trunk->m_meshConsts.GetCpu().windTrunk =
    //         m_leaves->m_meshConsts.GetCpu().windTrunk;
    // }
    // if (ImGui::SliderFloat("WindLeaves",
    //                        &m_leaves->m_meshConsts.GetCpu().windLeaves, 0.0f,
    //                        0.2f)) {
    // }

    ImGui::SetNextItemOpen(false, ImGuiCond_Once);
    if (ImGui::TreeNode("General")) {
        ImGui::Checkbox("Use FPV", &m_camera.m_useFirstPersonView);
        ImGui::Checkbox("Wireframe", &m_drawAsWire);
        if (ImGui::Checkbox("MSAA ON", &m_useMSAA)) {
            CreateBuffers();
        }
        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Skybox")) {
        ImGui::SliderFloat("Strength", &m_globalConstsCPU.strengthIBL, 0.0f,
                           0.5f);
        ImGui::RadioButton("Env", &m_globalConstsCPU.textureToDraw, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Specular", &m_globalConstsCPU.textureToDraw, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Irradiance", &m_globalConstsCPU.textureToDraw, 2);
        ImGui::SliderFloat("EnvLodBias", &m_globalConstsCPU.envLodBias, 0.0f,
                           10.0f);
        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Post Effects")) {
        int flag = 0;
        flag += ImGui::RadioButton("Render", &m_postEffectsConstsCPU.mode, 1);
        ImGui::SameLine();
        flag += ImGui::RadioButton("Depth", &m_postEffectsConstsCPU.mode, 2);
        flag += ImGui::SliderFloat(
            "DepthScale", &m_postEffectsConstsCPU.depthScale, 0.0, 1.0);
        flag += ImGui::SliderFloat("Fog", &m_postEffectsConstsCPU.fogStrength,
                                   0.0, 10.0);

        if (flag)
            D3D11Utils::UpdateBuffer(m_context, m_postEffectsConstsCPU,
                                     m_postEffectsConstsGPU);

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Post Processing")) {
        int flag = 0;
        flag += ImGui::SliderFloat(
            "Bloom Strength",
            &m_postProcess.m_combineFilter.m_constData.strength, 0.0f, 1.0f);
        flag += ImGui::SliderFloat(
            "Exposure", &m_postProcess.m_combineFilter.m_constData.option1,
            0.0f, 10.0f);
        flag += ImGui::SliderFloat(
            "Gamma", &m_postProcess.m_combineFilter.m_constData.option2, 0.1f,
            5.0f);
        // ���ǻ� ����� �Է��� �νĵǸ� �ٷ� GPU ���۸� ������Ʈ
        if (flag) {
            m_postProcess.m_combineFilter.UpdateConstantBuffers(m_context);
        }
        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Mirror")) {

        ImGui::SliderFloat("Alpha", &m_mirrorAlpha, 0.0f, 1.0f);
        const float blendColor[4] = {m_mirrorAlpha, m_mirrorAlpha,
                                     m_mirrorAlpha, 1.0f};
        if (m_drawAsWire)
            Graphics::mirrorBlendWirePSO.SetBlendFactor(blendColor);
        else
            Graphics::mirrorBlendSolidPSO.SetBlendFactor(blendColor);

        ImGui::SliderFloat("Metallic",
                           &m_mirror->m_materialConsts.GetCpu().metallicFactor,
                           0.0f, 1.0f);
        ImGui::SliderFloat("Roughness",
                           &m_mirror->m_materialConsts.GetCpu().roughnessFactor,
                           0.0f, 1.0f);

        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Light")) {
        // ImGui::SliderFloat3("Position",
        // &m_globalConstsCPU.lights[0].position.x,
        //                     -5.0f, 5.0f);
        ImGui::SliderFloat("Halo Radius",
                           &m_globalConstsCPU.lights[1].haloRadius, 0.0f, 2.0f);
        ImGui::SliderFloat("Halo Strength",
                           &m_globalConstsCPU.lights[1].haloStrength, 0.0f,
                           1.0f);
        ImGui::SliderFloat("Radius", &m_globalConstsCPU.lights[1].radius, 0.0f,
                           0.5f);
        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Material")) {
        ImGui::SliderFloat("LodBias", &m_globalConstsCPU.lodBias, 0.0f, 10.0f);

        int flag = 0;
        if (m_pickedModel) {
            flag += ImGui::SliderFloat(
                "Metallic",
                &m_pickedModel->m_materialConsts.GetCpu().metallicFactor, 0.0f,
                1.0f);
            flag += ImGui::SliderFloat(
                "Roughness",
                &m_pickedModel->m_materialConsts.GetCpu().roughnessFactor, 0.0f,
                1.0f);
            flag += ImGui::CheckboxFlags(
                "AlbedoTexture",
                &m_pickedModel->m_materialConsts.GetCpu().useAlbedoMap, 1);
            flag += ImGui::CheckboxFlags(
                "EmissiveTexture",
                &m_pickedModel->m_materialConsts.GetCpu().useEmissiveMap, 1);
            flag += ImGui::CheckboxFlags(
                "Use NormalMapping",
                &m_pickedModel->m_materialConsts.GetCpu().useNormalMap, 1);
            flag += ImGui::CheckboxFlags(
                "Use AO", &m_pickedModel->m_materialConsts.GetCpu().useAOMap,
                1);
            flag += ImGui::CheckboxFlags(
                "Use HeightMapping",
                &m_pickedModel->m_meshConsts.GetCpu().useHeightMap, 1);
            flag += ImGui::SliderFloat(
                "HeightScale",
                &m_pickedModel->m_meshConsts.GetCpu().heightScale, 0.0f, 0.1f);
            flag += ImGui::CheckboxFlags(
                "Use MetallicMap",
                &m_pickedModel->m_materialConsts.GetCpu().useMetallicMap, 1);
            flag += ImGui::CheckboxFlags(
                "Use RoughnessMap",
                &m_pickedModel->m_materialConsts.GetCpu().useRoughnessMap, 1);
            if (flag) {
                m_pickedModel->UpdateConstantBuffers(m_device, m_context);
            }
            ImGui::Checkbox("Draw Normals", &m_pickedModel->m_drawNormals);
        }

        ImGui::TreePop();
    }
}

} // namespace ansi
#include "ship_camera.h"
#include "sd2_h/save_load.h"
#include "core.h"

#define SCMR_BOXSCALE_X 1.6f
#define SCMR_BOXSCALE_Y 1.3f
#define SCMR_BOXSCALE_Z 1.4f

SHIP_CAMERA::SHIP_CAMERA()
{
    SetOn(false);
    SetActive(false);

    ZERO(vAng);
    pRS = nullptr;
    pSea = nullptr;
    pIsland = nullptr;
    lIlsInitCnt = 0;

    fDistanceDlt = 0.0f;
    fAngleXDlt = fAngleYDlt = 0.0f;
    fModelAy = 0.0f;

    fDistanceInertia = 15.0f;
    fAngleXInertia = 12.0f;
    fAngleYInertia = 10.0f;
}

SHIP_CAMERA::~SHIP_CAMERA()
{
}

bool SHIP_CAMERA::Init()
{
    // core.SystemMessages(GetId(),true);

    iLockX = 0;
    iLockY = 0;

    SetDevices();

    return true;
}

void SHIP_CAMERA::SetDevices()
{
    pRS = static_cast<VDX9RENDER *>(core.GetService("dx9render"));
    Assert(pRS);

    pSea = static_cast<SEA_BASE *>(EntityManager::GetEntityPointer(EntityManager::GetEntityId("sea")));
    // Assert(pSea);
}

void SHIP_CAMERA::Execute(uint32_t dwDeltaTime)
{
    if (!isOn())
        return;
    if (!FindShip())
        return;

    SetPerspective(AttributesPointer->GetAttributeAsFloat("Perspective"));

    const auto fDeltaTime = 0.001f * static_cast<float>(core.GetDeltaTime());

    auto *pModel = GetModelPointer();
    Assert(pModel);
    auto *const mtx = &pModel->mtx;
    vCenter = mtx->Pos();

    fModelAy = static_cast<float>(atan2(mtx->Vz().x, mtx->Vz().z));

    Move(fDeltaTime);
}

void SHIP_CAMERA::Move(float fDeltaTime)
{
    if (!pSea)
        return;
    if (!isActive())
        return;

    const auto fSpeed = fDeltaTime;
    auto fTempHeight = 0.0f;

    CONTROL_STATE cs;

    // Distance
    auto fSensivityDistanceDlt = 0.0f;
    core.Controls->GetControlState("ShipCamera_Forward", cs);
    if (cs.state == CST_ACTIVE || cs.state == CST_ACTIVATED)
        fSensivityDistanceDlt -= fSensivityDistance;
    core.Controls->GetControlState("ShipCamera_Backward", cs);
    if (cs.state == CST_ACTIVE || cs.state == CST_ACTIVATED)
        fSensivityDistanceDlt += fSensivityDistance;

    auto fKInert = fDistanceInertia * fSpeed;
    if (fKInert < 0.0f)
        fKInert = 0.0f;
    if (fKInert > 1.0f)
        fKInert = 1.0f;
    fSensivityDistance = 1.0f;
    fDistanceDlt += (fSensivityDistanceDlt - fDistanceDlt) * fKInert;
    fDistance += fSpeed * fDistanceDlt;
    if (fDistance > 1.0f)
        fDistance = 1.0f;
    if (fDistance < 0.0f)
        fDistance = 0.0f;

    // Rotate
    core.Controls->GetControlState("ShipCamera_Turn_H", cs);

    auto fValue = fInvertMouseX * 2.0f * (cs.fValue) * fSensivityAzimuthAngle;
    fKInert = fAngleYInertia * fSpeed;
    if (fKInert < 0.0f)
        fKInert = 0.0f;
    if (fKInert > 1.0f)
        fKInert = 1.0f;
    fAngleYDlt += (fValue - fAngleYDlt) * fKInert;
    vAng.y += fSpeed * fAngleYDlt;

    core.Controls->GetControlState("ShipCamera_Turn_V", cs);

    fValue = fInvertMouseY * 3.0f * (cs.fValue) * fSensivityHeightAngle;
    fKInert = fAngleXInertia * fSpeed;
    if (fKInert < 0.0f)
        fKInert = 0.0f;
    if (fKInert > 1.0f)
        fKInert = 1.0f;
    fAngleXDlt += (fValue - fAngleXDlt) * fKInert;
    vAng.x += fSpeed * fAngleXDlt;

    if (vAng.x < -2.0f * PI)
        vAng.x += 2.0f * PI;
    if (vAng.x > 2.0f * PI)
        vAng.x -= 2.0f * PI;
    if (vAng.x < fMinAngleX)
        vAng.x = fMinAngleX;
    if (vAng.x > fMaxAngleX)
        vAng.x = fMaxAngleX;

    // Current distance
    auto boxSize =
        GetAIObj()->GetBoxsize() * CVECTOR(SCMR_BOXSCALE_X * 0.5f, SCMR_BOXSCALE_Y * 0.5f, SCMR_BOXSCALE_Z * 0.5f);
    const auto realBoxSize = GetAIObj()->GetRealBoxsize();
    boxSize.x += realBoxSize.y;
    boxSize.z += realBoxSize.y;
    const auto maxRad = boxSize.z * 2.0f;
    // Semi-axes of the ellipsoid along which the camera moves
    const auto a = boxSize.x * 1.2f + fDistance * (maxRad - boxSize.x * 1.2f); // x
    const auto b = boxSize.y * 1.5f + fDistance * (70.0f - boxSize.y * 1.5f);  // y
    const auto c = boxSize.z * 1.2f + fDistance * (maxRad - boxSize.z * 1.2f); // z
    // Find the position of the camera on the ellipsoid
    vCenter.y += 0.5f * boxSize.y;
    CVECTOR vPos;
    if (vAng.x <= 0.0f)
    {
        // Above 0 driving on an ellipsoid
        vPos.x = a * cosf(-vAng.x) * sinf(vAng.y);
        vPos.y = b * sinf(-vAng.x);
        vPos.z = c * cosf(-vAng.x) * cosf(vAng.y);
    }
    else
    {
        // Below 0 driving on an elliptical cylinder
        vPos.x = a * sinf(vAng.y);
        vPos.y = 0.0f; // b*sinf(-vAng.x);
        vPos.z = c * cosf(vAng.y);
    }
    vPos = CMatrix(CVECTOR(0.0f, fModelAy, 0.0f), vCenter) * vPos;
    if (vAng.x > 0.0f)
        vCenter.y += boxSize.z * vAng.x * 6.0f;
    // Limit the height from the bottom
    const auto fWaveY = pSea->WaveXZ(vPos.x, vPos.z);
    if (vPos.y - fWaveY < fMinHeightOnSea)
        vPos.y = fWaveY + fMinHeightOnSea;
    const auto oldPosY = vPos.y;
    // Ships collision
    ShipsCollision(vPos);
    // Island collision
    if (IslandCollision(vPos))
    {
        ShipsCollision(vPos);
        IslandCollision(vPos);
    }
    if (vPos.y > oldPosY)
        vCenter.y += vPos.y - oldPosY;
    // Set new camera
    vCenter.y += fDistance * 2.0f * boxSize.y;
    pRS->SetCamera(vPos, vCenter, CVECTOR(0.0f, 1.0f, 0.0f));
    pRS->SetPerspective(GetPerspective());
}

void SHIP_CAMERA::Realize(uint32_t dwDeltaTime)
{
}

void SHIP_CAMERA::SetCharacter(ATTRIBUTES *_pACharacter)
{
    entid_t eidTemp;

    pACharacter = _pACharacter;
}

uint32_t SHIP_CAMERA::AttributeChanged(ATTRIBUTES *pAttr)
{
    if (*pAttr == "SensivityDistance")
        fSensivityDistance = pAttr->GetAttributeAsFloat();
    if (*pAttr == "SensivityAzimuthAngle")
        fSensivityAzimuthAngle = pAttr->GetAttributeAsFloat();
    if (*pAttr == "SensivityHeightAngle")
        fSensivityHeightAngle = pAttr->GetAttributeAsFloat();
    if (*pAttr == "SensivityHeightAngleOnShip")
        fSensivityHeightAngleOnShip = pAttr->GetAttributeAsFloat();
    if (*pAttr == "MaxAngleX")
        fMaxAngleX = pAttr->GetAttributeAsFloat();
    if (*pAttr == "MinAngleX")
        fMinAngleX = pAttr->GetAttributeAsFloat();
    if (*pAttr == "MaxHeightOnShip")
        fMaxHeightOnShip = pAttr->GetAttributeAsFloat();
    if (*pAttr == "MinHeightOnSea")
        fMinHeightOnSea = pAttr->GetAttributeAsFloat();
    if (*pAttr == "MaxDistance")
        fMaxDistance = pAttr->GetAttributeAsFloat();
    if (*pAttr == "MinDistance")
        fMinDistance = pAttr->GetAttributeAsFloat();
    if (*pAttr == "Distance")
        fDistance = pAttr->GetAttributeAsFloat();
    if (*pAttr == "InvertMouseX")
        fInvertMouseX = pAttr->GetAttributeAsFloat();
    if (*pAttr == "InvertMouseY")
        fInvertMouseY = pAttr->GetAttributeAsFloat();

    return 0;
}

void SHIP_CAMERA::ShipsCollision(CVECTOR &pos)
{
    CVECTOR p;
    const auto &entities = EntityManager::GetEntityIdVector("ship");
    for (auto ent : entities)
    {
        // Object pointer
        auto *ship = static_cast<VAI_OBJBASE *>(EntityManager::GetEntityPointer(ent));
        if (!ship)
            break;
        if (ship == GetAIObj())
            continue;
        // Camera position in the ship system
        Assert(ship->GetMatrix());
        ship->GetMatrix()->MulToInv(pos, p);
        // Check if hitting the box
        auto s = ship->GetBoxsize() * CVECTOR(SCMR_BOXSCALE_X * 0.5f, SCMR_BOXSCALE_Y * 0.5f, SCMR_BOXSCALE_Z * 0.5f);
        if (s.x <= 0.0f || s.y <= 0.0f || s.z <= 0.0f)
            continue;
        // Building an ellipsoid
        const auto a = s.z + s.y; // z
        const auto b = s.x + s.y; // x
        auto k1 = s.z / a;
        auto k2 = s.x / b;
        const auto c = s.y / sqrtf(1.0f - k1 * k1 - k2 * k2); // y
        // Calculate height
        k1 = p.z / a;
        k2 = p.x / b;
        auto h = (1.0f - k1 * k1 - k2 * k2);
        if (h <= 0.0f)
            continue;
        h = b * b * h; //^2
        h = sqrtf(h);
        if (h > s.y)
            h = s.y * (1.0f + 0.1f * (h - s.y) / (c - s.y));
        if (p.y < h)
            p.y = h;
        s = ship->GetMatrix()[0] * p;
        if (pos.y < s.y)
            pos.y = s.y;
    }
}

bool SHIP_CAMERA::IslandCollision(CVECTOR &pos)
{
    const auto camRadius = 0.4f;
    // Island
    if (pIsland == nullptr)
    {
        if (lIlsInitCnt < 10)
        {
            if (const auto island_id = EntityManager::GetEntityId("island"))
                pIsland = static_cast<ISLAND_BASE *>(EntityManager::GetEntityPointer(island_id));
            lIlsInitCnt++;
            if (pIsland == nullptr)
                return false;
        }
        else
            return false;
    }
    // Model
    auto *mdl = static_cast<MODEL *>(EntityManager::GetEntityPointer(pIsland->GetModelEID()));
    if (mdl == nullptr)
        return false;
    // Find direction, distance
    auto dir = pos - vCenter;
    auto dist = ~dir;
    if (dist <= 0.0f)
        return false;
    dist = sqrtf(dist);
    dir *= 1.0f / dist;
    const auto dr = dir * (dist + camRadius);
    // First check
    float k[5];
    k[0] = mdl->Trace(vCenter, vCenter + dr);
    // Basis
    auto left = dir ^ CVECTOR(0.0f, 1.0f, 0.0f);
    const auto l = ~left;
    if (l <= 0.0f)
    {
        if (k[0] < 1.0f)
            pos = vCenter + (pos - vCenter) * k[0] - dir * camRadius;
        return k[0] < 1.0f;
    }
    left *= 1.0f / sqrtf(l);
    const auto up = dir ^ left;
    // Find nearest distanse
    CVECTOR src;
    src = vCenter + left * camRadius;
    k[1] = mdl->Trace(src, src + dr);
    src = vCenter - left * camRadius;
    k[2] = mdl->Trace(src, src + dr);
    src = vCenter + up * camRadius;
    k[3] = mdl->Trace(src, src + dr);
    src = vCenter - up * camRadius;
    k[4] = mdl->Trace(src, src + dr);
    auto kRes = 2.0f;
    for (int32_t i = 0; i < 5; i++)
        if (kRes > k[i])
            kRes = k[i];
    if (kRes < 1.0f)
        pos = vCenter + (pos - vCenter) * kRes - dir * camRadius;
    return kRes < 1.0f;
}

void SHIP_CAMERA::Save(CSaveLoad *pSL)
{
    pSL->SaveLong(iLockX);
    pSL->SaveLong(iLockY);
    pSL->SaveFloat(fMinHeightOnSea);
    pSL->SaveFloat(fMaxHeightOnShip);
    pSL->SaveFloat(fDistance);
    pSL->SaveFloat(fMaxDistance);
    pSL->SaveFloat(fMinDistance);
    pSL->SaveFloat(fDistanceDlt);
    pSL->SaveFloat(fDistanceInertia);
    pSL->SaveFloat(fMinAngleX);
    pSL->SaveFloat(fMaxAngleX);
    pSL->SaveFloat(fAngleXDlt);
    pSL->SaveFloat(fAngleXInertia);
    pSL->SaveFloat(fAngleYDlt);
    pSL->SaveFloat(fAngleYInertia);
    pSL->SaveFloat(fSensivityDistance);
    pSL->SaveFloat(fSensivityAzimuthAngle);
    pSL->SaveFloat(fSensivityHeightAngle);
    pSL->SaveFloat(fSensivityHeightAngleOnShip);
    pSL->SaveFloat(fInvertMouseX);
    pSL->SaveFloat(fInvertMouseY);
    pSL->SaveVector(vCenter);
    pSL->SaveVector(vAng);
    pSL->SaveFloat(fModelAy);
    pSL->SaveLong(lIlsInitCnt);

    pSL->SaveDword(isOn());
    pSL->SaveDword(isActive());
    pSL->SaveFloat(GetPerspective());
    pSL->SaveAPointer("character", pACharacter);
}

void SHIP_CAMERA::Load(CSaveLoad *pSL)
{
    iLockX = pSL->LoadLong();
    iLockY = pSL->LoadLong();
    fMinHeightOnSea = pSL->LoadFloat();
    fMaxHeightOnShip = pSL->LoadFloat();
    fDistance = pSL->LoadFloat();
    fMaxDistance = pSL->LoadFloat();
    fMinDistance = pSL->LoadFloat();
    fDistanceDlt = pSL->LoadFloat();
    fDistanceInertia = pSL->LoadFloat();
    fMinAngleX = pSL->LoadFloat();
    fMaxAngleX = pSL->LoadFloat();
    fAngleXDlt = pSL->LoadFloat();
    fAngleXInertia = pSL->LoadFloat();
    fAngleYDlt = pSL->LoadFloat();
    fAngleYInertia = pSL->LoadFloat();
    fSensivityDistance = pSL->LoadFloat();
    fSensivityAzimuthAngle = pSL->LoadFloat();
    fSensivityHeightAngle = pSL->LoadFloat();
    fSensivityHeightAngleOnShip = pSL->LoadFloat();
    fInvertMouseX = pSL->LoadFloat();
    fInvertMouseY = pSL->LoadFloat();
    vCenter = pSL->LoadVector();
    vAng = pSL->LoadVector();
    fModelAy = pSL->LoadFloat();
    lIlsInitCnt = pSL->LoadLong();

    SetOn(pSL->LoadDword() != 0);
    SetActive(pSL->LoadDword() != 0);
    SetPerspective(pSL->LoadFloat());
    SetCharacter(pSL->LoadAPointer("character"));
}

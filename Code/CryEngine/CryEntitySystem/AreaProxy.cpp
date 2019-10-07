// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AreaProxy.h"
#include "Entity.h"
#include <CryNetwork/ISerialize.h>

std::vector<Vec3> CAreaProxy::s_tmpWorldPoints;

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::ResetTempState()
{
	stl::free_container(s_tmpWorldPoints);
}

//////////////////////////////////////////////////////////////////////////
CAreaProxy::CAreaProxy()
	: m_pEntity(nullptr)
	, m_nFlags(0)
	, m_pArea(nullptr)
	, m_fRadius(0.0f)
	, m_fGravity(0.0f)
	, m_fFalloff(0.0f)
	, m_fDamping(0.0f)
	, m_bDontDisableInvisible(0.0f)
	, m_bIsEnable(false)
	, m_bIsEnableInternal(false)
	, m_lastFrameTime(0.0f)
{
}

//////////////////////////////////////////////////////////////////////////
CAreaProxy::~CAreaProxy()
{
	SAFE_RELEASE(m_pArea);
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::Initialize(const SComponentInitializer& init)
{
	m_pEntity = (CEntity*)init.m_pEntity;

	m_pArea = static_cast<CAreaManager*>(m_pEntity->GetCEntitySystem()->GetAreaManager())->CreateArea();
	m_pArea->SetEntityID(m_pEntity->GetId());

	Reset();
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::Reload(IEntity* pEntity, SEntitySpawnParams& params)
{
	m_pEntity = (CEntity*)pEntity;

	CRY_ASSERT(m_pArea);
	if (m_pArea)
	{
		m_pArea->SetEntityID(pEntity->GetId());
	}

	Reset();
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::Release()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::Reset()
{
	m_nFlags = 0;

	m_vCenter = ZERO;
	m_fRadius = 0.0f;
	m_fGravity = 0.0f;
	m_fFalloff = 0.8f;
	m_fDamping = 1.0f;
	m_bDontDisableInvisible = false;
	m_bIsEnable = true;
	m_bIsEnableInternal = true;
	m_lastFrameTime = 0.0f;
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::OnMove()
{
	if ((m_nFlags & FLAG_NOT_UPDATE_AREA) == 0)
	{
		Matrix34 const& worldTM = m_pEntity->GetWorldTM();

		switch (m_pArea->GetAreaType())
		{
		case ENTITY_AREA_TYPE_SHAPE:
			{
				size_t const numLocalPoints = m_localPoints.size();
				s_tmpWorldPoints.resize(numLocalPoints);

				for (size_t i = 0; i < numLocalPoints; i++)
				{
					s_tmpWorldPoints[i] = worldTM.TransformPoint(m_localPoints[i]);
				}

				if (!s_tmpWorldPoints.empty())
				{
					m_pArea->MovePoints(&s_tmpWorldPoints[0], numLocalPoints);
					s_tmpWorldPoints.clear();
				}
				else
				{
#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
					CryFatalError("An area shape without points cannot be moved.\nVerify that it is properly initialized.");
#endif    // INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE
				}
			}
			break;
		case ENTITY_AREA_TYPE_BOX:
			m_pArea->SetMatrix(worldTM);
			break;
		case ENTITY_AREA_TYPE_SPHERE:
			m_pArea->SetSphere(worldTM.TransformPoint(m_vCenter), m_fRadius);
			break;
		case ENTITY_AREA_TYPE_GRAVITYVOLUME:
			break;
		case ENTITY_AREA_TYPE_SOLID:
			break;
		default:
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::OnEnable(bool bIsEnable, bool bIsCallScript)
{
	m_bIsEnable = bIsEnable;
	if (m_pArea->GetAreaType() == ENTITY_AREA_TYPE_GRAVITYVOLUME)
	{
		SEntityPhysicalizeParams physparams;
		if (bIsEnable && m_bIsEnableInternal)
		{
			physparams.pAreaDef = &m_areaDefinition;
			m_areaDefinition.areaType = SEntityPhysicalizeParams::AreaDefinition::AREA_SPLINE;
			m_bezierPointsTmp.resize(m_bezierPoints.size());
			memcpy(&m_bezierPointsTmp[0], &m_bezierPoints[0], m_bezierPoints.size() * sizeof(Vec3));
			m_areaDefinition.pPoints = &m_bezierPointsTmp[0];
			m_areaDefinition.nNumPoints = m_bezierPointsTmp.size();
			m_areaDefinition.fRadius = m_fRadius;
			m_gravityParams.gravity = Vec3(0, 0, m_fGravity);
			m_gravityParams.falloff0 = m_fFalloff;
			m_gravityParams.damping = m_fDamping;
			physparams.type = PE_AREA;
			m_areaDefinition.pGravityParams = &m_gravityParams;

			m_pEntity->SetTimer(0, 11000);
		}
		m_pEntity->Physicalize(physparams);

		if (bIsCallScript)
		{
			//call the OnEnable function in the script, to set game flags for this entity and such.
			IScriptTable* pScriptTable = m_pEntity->GetScriptTable();
			if (pScriptTable)
			{
				HSCRIPTFUNCTION scriptFunc(NULL);
				pScriptTable->GetValue("OnEnable", scriptFunc);

				if (scriptFunc)
					Script::Call(gEnv->pScriptSystem, scriptFunc, pScriptTable, bIsEnable);

				gEnv->pScriptSystem->ReleaseFunc(scriptFunc);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::ProcessEvent(SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_XFORM:
		OnMove();
		break;
	case ENTITY_EVENT_SCRIPT_EVENT:
		{
			const char* pName = (const char*)event.nParam[0];
			if (!stricmp(pName, "Enable"))
				OnEnable(true);
			else if (!stricmp(pName, "Disable"))
				OnEnable(false);
		}
		break;
	case ENTITY_EVENT_RENDER:
		{
			if (m_pArea->GetAreaType() == ENTITY_AREA_TYPE_GRAVITYVOLUME)
			{
				if (!m_bDontDisableInvisible)
				{
					m_lastFrameTime = gEnv->pTimer->GetCurrTime();
				}
				if (!m_bIsEnableInternal)
				{
					m_bIsEnableInternal = true;
					OnEnable(m_bIsEnable, false);
					m_pEntity->SetTimer(0, 11000);
				}
			}
		}
		break;
	case ENTITY_EVENT_TIMER:
		{
			if (m_pArea->GetAreaType() == ENTITY_AREA_TYPE_GRAVITYVOLUME)
			{
				if (!m_bDontDisableInvisible)
				{
					bool bOff = false;
					if (gEnv->pTimer->GetCurrTime() - m_lastFrameTime > 10.0f)
					{
						bOff = true;
						IEntityRenderProxy* pEntPr = (IEntityRenderProxy*)m_pEntity->GetProxy(ENTITY_PROXY_RENDER);
						if (pEntPr)
						{
							IRenderNode* pRendNode = pEntPr->GetRenderNode();
							if (pRendNode)
							{
								if (pEntPr->IsRenderProxyVisAreaVisible())
									bOff = false;
							}
						}
						if (bOff)
						{
							m_bIsEnableInternal = false;
							OnEnable(m_bIsEnable, false);
						}
					}
					if (!bOff)
						m_pEntity->SetTimer(0, 11000);
				}
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::SerializeXML(XmlNodeRef& entityNode, bool bLoading)
{
	if (m_nFlags & FLAG_NOT_SERIALIZE)
		return;

	if (bLoading)
	{
		XmlNodeRef areaNode = entityNode->findChild("Area");
		if (!areaNode)
			return;

		int nId = 0, nGroup = 0, nPriority = 0;
		float fProximity = 0;
		float fHeight = 0;
		float innerFadeDistance = 0.0f;

		areaNode->getAttr("Id", nId);
		areaNode->getAttr("Group", nGroup);
		areaNode->getAttr("Proximity", fProximity);
		areaNode->getAttr("Priority", nPriority);
		areaNode->getAttr("InnerFadeDistance", innerFadeDistance);
		m_pArea->SetID(nId);
		m_pArea->SetGroup(nGroup);
		m_pArea->SetProximity(fProximity);
		m_pArea->SetPriority(nPriority);
		m_pArea->SetInnerFadeDistance(innerFadeDistance);
		const char* token(0);

		XmlNodeRef pointsNode = areaNode->findChild("Points");

		if (pointsNode)
		{
			bool bObstructSound = false;
			int const numPoints = pointsNode->getChildCount();
			CRY_ASSERT(s_tmpWorldPoints.empty());
			const Matrix34& tm = m_pEntity->GetWorldTM();

			for (int i = 0; i < numPoints; i++)
			{
				XmlNodeRef pntNode = pointsNode->getChild(i);
				Vec3 pos(ZERO);
				pntNode->getAttr("Pos", pos);
				m_localPoints.push_back(pos);
				s_tmpWorldPoints.push_back(tm.TransformPoint(pos));

				// Get sound obstruction
				pntNode->getAttr("ObstructSound", bObstructSound);
				m_abObstructSound.push_back(bObstructSound);
			}

			// Getting "Roof" and "Floor"
			XmlNodeRef const roofNode = areaNode->findChild("Roof");

			if (roofNode)
			{
				roofNode->getAttr("ObstructSound", bObstructSound);
			}
			else
			{
				bObstructSound = false;
			}

			m_abObstructSound.push_back(bObstructSound);
			XmlNodeRef const floorNode = areaNode->findChild("Floor");

			if (floorNode)
			{
				floorNode->getAttr("ObstructSound", bObstructSound);
			}
			else
			{
				bObstructSound = false;
			}

			m_abObstructSound.push_back(bObstructSound);
			m_pArea->SetAreaType(ENTITY_AREA_TYPE_SHAPE);

			areaNode->getAttr("Height", fHeight);
			m_pArea->SetHeight(fHeight);

			size_t const numLocalPoints = s_tmpWorldPoints.size();
			size_t const numAudioPoints = numLocalPoints + 2; // Adding "Roof" and "Floor"
			bool* const pbObstructSound = new bool[numAudioPoints];

			for (size_t i = 0; i < numAudioPoints; ++i)
			{
				// Here we "un-pack" the data! (1 bit*numLocalPoints to 1 byte*numLocalPoints)
				pbObstructSound[i] = m_abObstructSound[i];
			}

			m_pArea->SetPoints(&s_tmpWorldPoints[0], &pbObstructSound[0], numLocalPoints);
			s_tmpWorldPoints.clear();
			delete[] pbObstructSound;

			size_t index = 0;
			tSoundObstruction::const_iterator Iter(m_abObstructSound.begin());
			tSoundObstruction::const_iterator const IterEnd(m_abObstructSound.end());

			for (; Iter != IterEnd; ++Iter)
			{
				m_pArea->SetSoundObstructionOnAreaFace(index++, static_cast<bool>(*Iter));
			}
		}
		else if (areaNode->getAttr("SphereRadius", m_fRadius))
		{
			// Sphere.
			areaNode->getAttr("SphereCenter", m_vCenter);
			m_pArea->SetSphere(m_pEntity->GetWorldTM().TransformPoint(m_vCenter), m_fRadius);
		}
		else if (areaNode->getAttr("VolumeRadius", m_fRadius))
		{
			areaNode->getAttr("Gravity", m_fGravity);
			areaNode->getAttr("DontDisableInvisible", m_bDontDisableInvisible);

			AABB box;
			box.Reset();

			// Bezier Volume.
			pointsNode = areaNode->findChild("BezierPoints");
			if (pointsNode)
			{
				for (int i = 0; i < pointsNode->getChildCount(); i++)
				{
					XmlNodeRef pntNode = pointsNode->getChild(i);
					Vec3 pt;
					if (pntNode->getAttr("Pos", pt))
					{
						m_bezierPoints.push_back(pt);
						box.Add(pt);
					}
				}
			}
			m_pArea->SetAreaType(ENTITY_AREA_TYPE_GRAVITYVOLUME);
			if (!m_pEntity->GetRenderProxy())
			{
				IEntityRenderProxyPtr pRenderProxy = crycomponent_cast<IEntityRenderProxyPtr>(m_pEntity->CreateProxy(ENTITY_PROXY_RENDER));
				m_pEntity->SetFlags(m_pEntity->GetFlags() | ENTITY_FLAG_SEND_RENDER_EVENT);

				if (box.min.x > box.max.x)
					box.min = box.max = Vec3(0, 0, 0);
				box.min -= Vec3(m_fRadius, m_fRadius, m_fRadius);
				box.max += Vec3(m_fRadius, m_fRadius, m_fRadius);

				box.SetTransformedAABB(m_pEntity->GetWorldTM().GetInverted(), box);

				pRenderProxy->SetLocalBounds(box, true);
			}

			OnEnable(m_bIsEnable);
		}
		else if (areaNode->getAttr("AreaSolidFileName", &token))
		{
			CCryFile file;

			int nAliasLen = sizeof("%level%") - 1;
			const char* areaSolidFileName;
			if (strncmp(token, "%level%", nAliasLen) == 0)
				areaSolidFileName = GetIEntitySystem()->GetSystem()->GetI3DEngine()->GetLevelFilePath(token + nAliasLen);
			else
				areaSolidFileName = token;

			if (file.Open(areaSolidFileName, "rb"))
			{
				int numberOfClosedPolygon = 0;
				int numberOfOpenPolygon = 0;

				m_pArea->BeginSettingSolid(m_pEntity->GetWorldTM());

				file.ReadType(&numberOfClosedPolygon);
				file.ReadType(&numberOfOpenPolygon);

				ReadPolygonsForAreaSolid(file, numberOfClosedPolygon, true);
				ReadPolygonsForAreaSolid(file, numberOfOpenPolygon, false);

				m_pArea->EndSettingSolid();
			}
		}
		else
		{
			// Box.
			Vec3 bmin(ZERO), bmax(ZERO);
			areaNode->getAttr("BoxMin", bmin);
			areaNode->getAttr("BoxMax", bmax);
			m_pArea->SetBox(bmin, bmax, m_pEntity->GetWorldTM());

			// Get sound obstruction
			XmlNodeRef const pNodeSoundData = areaNode->findChild("SoundData");

			if (pNodeSoundData)
			{
				CRY_ASSERT(m_abObstructSound.size() == 0);

				for (int i = 0; i < pNodeSoundData->getChildCount(); ++i)
				{
					XmlNodeRef const pNodeSide = pNodeSoundData->getChild(i);

					if (pNodeSide)
					{
						bool bObstructSound = false;
						pNodeSide->getAttr("ObstructSound", bObstructSound);
						m_abObstructSound.push_back(bObstructSound);
						m_pArea->SetSoundObstructionOnAreaFace(i, bObstructSound);
					}
				}

				CRY_ASSERT(m_abObstructSound.size() == 6);
			}
		}

		m_pArea->RemoveEntities();
		XmlNodeRef entitiesNode = areaNode->findChild("Entities");
		// Export Entities.
		if (entitiesNode)
		{
			for (int i = 0; i < entitiesNode->getChildCount(); i++)
			{
				XmlNodeRef entNode = entitiesNode->getChild(i);
				EntityId entityId;
				EntityGUID entityGuid;
				if (gEnv->pEntitySystem->EntitiesUseGUIDs())
				{
					if (entNode->getAttr("Guid", entityGuid))
						m_pArea->AddEntity(entityGuid);
				}
				else
				{
					if (entNode->getAttr("Id", entityId) && (entityId != INVALID_ENTITYID))
					{
						m_pArea->AddEntity(entityId);
					}
				}
			}
		}
	}
	else
	{
		// Save points.
		XmlNodeRef const areaNode = entityNode->newChild("Area");
		areaNode->setAttr("Id", m_pArea->GetID());
		areaNode->setAttr("Group", m_pArea->GetGroup());
		areaNode->setAttr("Proximity", m_pArea->GetProximity());
		areaNode->setAttr("Priority", m_pArea->GetPriority());
		areaNode->setAttr("InnerFadeDistance", m_pArea->GetInnerFadeDistance());

		EEntityAreaType const type = m_pArea->GetAreaType();

		if (type == ENTITY_AREA_TYPE_SHAPE)
		{
			// AreaShapes must consist out of at least 2 points.
			size_t const numLocalPoints = m_localPoints.size();

#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
			if ((numLocalPoints + 2) != m_abObstructSound.size())
			{
				stack_string temp;
				temp.Format("Trying to save an AreaShape with mismatching points and sound obstruction count. (%u and %u)", numLocalPoints + 2, m_abObstructSound.size());
				CRY_ASSERT_MESSAGE(false, temp.c_str());
				m_abObstructSound.resize(numLocalPoints + 2);
			}
#endif //INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE

			if (numLocalPoints > 1)
			{
				XmlNodeRef const pointsNode = areaNode->newChild("Points");

				for (size_t i = 0; i < numLocalPoints; ++i)
				{
					XmlNodeRef const pntNode = pointsNode->newChild("Point");
					pntNode->setAttr("Pos", m_localPoints[i]);
					pntNode->setAttr("ObstructSound", m_abObstructSound[i]);
				}

				// Adding "Roof" and "Floor"
				XmlNodeRef const roofNode = areaNode->newChild("Roof");
				roofNode->setAttr("ObstructSound", m_abObstructSound[numLocalPoints]);
				XmlNodeRef const floorNode = areaNode->newChild("Floor");
				floorNode->setAttr("ObstructSound", m_abObstructSound[numLocalPoints + 1]);

				areaNode->setAttr("Height", m_pArea->GetHeight());
			}
#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
			else
			{
				stack_string temp;
				Vec2 min, max;
				m_pArea->GetBBox(min, max);
				temp.Format("Trying to save an AreaShape with less than 2 points! (Name: %s) (MinX: %.2f MinY: %.2f MaxX: %.2f MaxY: %.2f)", m_pArea->GetAreaEntityName(), min.x, min.y, max.x, max.y);
				CRY_ASSERT_MESSAGE(false, temp.c_str());
			}
#endif //INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE
		}
		else if (type == ENTITY_AREA_TYPE_SPHERE)
		{
			// Sphere.
			areaNode->setAttr("SphereCenter", m_vCenter);
			areaNode->setAttr("SphereRadius", m_fRadius);
		}
		else if (type == ENTITY_AREA_TYPE_BOX)
		{
			// Box.
			Vec3 bmin, bmax;
			m_pArea->GetBox(bmin, bmax);
			areaNode->setAttr("BoxMin", bmin);
			areaNode->setAttr("BoxMax", bmax);

			// Set sound obstruction
			XmlNodeRef const pNodeSoundData = areaNode->newChild("SoundData");

			if (pNodeSoundData)
			{
				CRY_ASSERT(m_abObstructSound.size() == 6);
				size_t nIndex = 0;

				for (bool const bObstructed : m_abObstructSound)
				{
					CryFixedStringT<16> sTemp;
					sTemp.Format("Side%" PRISIZE_T, ++nIndex);

					XmlNodeRef const pNodeSide = pNodeSoundData->newChild(sTemp.c_str());
					pNodeSide->setAttr("ObstructSound", bObstructed);
				}
			}
		}
		else if (type == ENTITY_AREA_TYPE_GRAVITYVOLUME)
		{
			areaNode->setAttr("VolumeRadius", m_fRadius);
			areaNode->setAttr("Gravity", m_fGravity);
			areaNode->setAttr("DontDisableInvisible", m_bDontDisableInvisible);
			XmlNodeRef pointsNode = areaNode->newChild("BezierPoints");
			for (unsigned int i = 0; i < m_bezierPoints.size(); i++)
			{
				XmlNodeRef pntNode = pointsNode->newChild("Point");
				pntNode->setAttr("Pos", m_bezierPoints[i]);
			}
		}

#ifdef SW_ENTITY_ID_USE_GUID
		const std::vector<EntityGUID>& entGUIDs = *m_pArea->GetEntitiesGuid();
		// Export Entities.
		if (!entGUIDs.empty())
		{
			XmlNodeRef nodes = areaNode->newChild("Entities");
			for (uint32 i = 0; i < entGUIDs.size(); i++)
			{
				EntityGUID guid = entGUIDs[i];
				XmlNodeRef entNode = nodes->newChild("Entity");
				entNode->setAttr("Guid", guid);
				entNode->setAttr("Id", gEnv->pEntitySystem->GenerateEntityIdFromGuid(guid));
			}
		}
#else
		const std::vector<EntityId>& entIDs = *m_pArea->GetEntities();
		// Export Entities.
		if (!entIDs.empty())
		{
			XmlNodeRef nodes = areaNode->newChild("Entities");
			for (uint32 i = 0; i < entIDs.size(); i++)
			{
				int entityId = entIDs[i];
				XmlNodeRef entNode = nodes->newChild("Entity");
				entNode->setAttr("Id", entityId);
			}
		}
#endif
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::ReadPolygonsForAreaSolid(CCryFile& file, int numberOfPolygons, bool bObstruction)
{
	static const int numberOfStaticVertices(200);
	Vec3 pStaticVertices[numberOfStaticVertices];

	for (int i = 0; i < numberOfPolygons; ++i)
	{
		int numberOfVertices(0);
		file.ReadType(&numberOfVertices);
		Vec3* pVertices(NULL);
		if (numberOfVertices > numberOfStaticVertices)
			pVertices = new Vec3[numberOfVertices];
		else
			pVertices = pStaticVertices;
		for (int k = 0; k < numberOfVertices; ++k)
			file.ReadType(&pVertices[k]);
		m_pArea->AddConvexHullToSolid(pVertices, bObstruction, numberOfVertices);
		if (pVertices != pStaticVertices)
			delete[] pVertices;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAreaProxy::GetSignature(TSerialize signature)
{
	signature.BeginGroup("AreaProxy");
	signature.EndGroup();
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::Serialize(TSerialize ser)
{
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::SetPoints(Vec3 const* const pPoints, bool const* const pSoundObstructionSegments, size_t const numLocalPoints, float const height)
{
	m_pArea->SetHeight(height);
	m_pArea->SetAreaType(ENTITY_AREA_TYPE_SHAPE);

	if (numLocalPoints > 0)
	{
		if (pPoints != nullptr && m_pEntity != nullptr)
		{
			m_localPoints.resize(numLocalPoints);
			memcpy(&m_localPoints[0], pPoints, numLocalPoints * sizeof(Vec3));

			Matrix34 const& worldTM = m_pEntity->GetWorldTM();
			CRY_ASSERT(s_tmpWorldPoints.empty());

			for (size_t i = 0; i < numLocalPoints; ++i)
			{
				s_tmpWorldPoints.push_back(worldTM.TransformPoint(m_localPoints[i]));
			}
		}

		if (pSoundObstructionSegments != nullptr)
		{
			// Here we pack the data again! (1 byte*numAudioPoints to 1 bit*numAudioPoints)
			size_t const numAudioPoints = numLocalPoints + 2; // Adding "Roof" and "Floor"
			m_abObstructSound.resize(numAudioPoints);

			for (size_t i = 0; i < numAudioPoints; ++i)
			{
				m_abObstructSound[i] = pSoundObstructionSegments[i];
			}
		}

		if (!s_tmpWorldPoints.empty())
		{
			m_pArea->SetPoints(&s_tmpWorldPoints[0], pSoundObstructionSegments, numLocalPoints);
			s_tmpWorldPoints.clear();
		}
		else if (pSoundObstructionSegments != nullptr)
		{
			m_pArea->SetPoints(nullptr, pSoundObstructionSegments, numLocalPoints);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
const Vec3* CAreaProxy::GetPoints()
{
	if (m_localPoints.empty())
		return 0;
	return &m_localPoints[0];
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::SetBox(const Vec3& min, const Vec3& max, const bool* const pabSoundObstructionSides, size_t const nSideCount)
{
	m_pArea->SetBox(min, max, m_pEntity->GetWorldTM());
	m_localPoints.clear();

	if (pabSoundObstructionSides != nullptr && nSideCount > 0)
	{
		m_abObstructSound.clear();
		m_abObstructSound.reserve(6);

		// Here we pack the data again! (1 byte*nSideCount to 1 bit*nSideCount)
		for (size_t i = 0; i < nSideCount; ++i)
		{
			m_abObstructSound.push_back(pabSoundObstructionSides[i]);
		}

		CRY_ASSERT(m_abObstructSound.size() == 6);
		size_t nIndex = 0;

		tSoundObstruction::const_iterator Iter(m_abObstructSound.begin());
		tSoundObstruction::const_iterator const IterEnd(m_abObstructSound.end());

		for (; Iter != IterEnd; ++Iter)
		{
			bool const bObstructed = (bool)(*Iter);
			m_pArea->SetSoundObstructionOnAreaFace(nIndex, bObstructed);
			++nIndex;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::SetSphere(const Vec3& vCenter, float fRadius)
{
	m_vCenter = vCenter;
	m_fRadius = fRadius;
	m_localPoints.clear();
	m_pArea->SetSphere(m_pEntity->GetWorldTM().TransformPoint(m_vCenter), fRadius);
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::BeginSettingSolid(const Matrix34& worldTM)
{
	m_pArea->BeginSettingSolid(worldTM);
	m_pArea->SetAreaType(ENTITY_AREA_TYPE_SOLID);
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::AddConvexHullToSolid(const Vec3* verticesOfConvexHull, bool bObstruction, int numberOfVertices)
{
	m_pArea->AddConvexHullToSolid(verticesOfConvexHull, bObstruction, numberOfVertices);
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::EndSettingSolid()
{
	m_pArea->EndSettingSolid();
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::SetGravityVolume(const Vec3* pPoints, int nNumPoints, float fRadius, float fGravity, bool bDontDisableInvisible, float fFalloff, float fDamping)
{
	m_bIsEnableInternal = true;
	m_fRadius = fRadius;
	m_fGravity = fGravity;
	m_fFalloff = fFalloff;
	m_fDamping = fDamping;
	m_bDontDisableInvisible = bDontDisableInvisible;

	m_bezierPoints.resize(nNumPoints);
	if (nNumPoints > 0)
		memcpy(&m_bezierPoints[0], pPoints, nNumPoints * sizeof(Vec3));

	if (!bDontDisableInvisible)
		m_pEntity->SetTimer(0, 11000);

	m_pArea->SetAreaType(ENTITY_AREA_TYPE_GRAVITYVOLUME);
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::SetSoundObstructionOnAreaFace(size_t const index, bool const bObstructs)
{
	CRY_ASSERT(index < m_abObstructSound.size());
	m_abObstructSound[index] = bObstructs;
	m_pArea->SetSoundObstructionOnAreaFace(index, bObstructs);
}

//////////////////////////////////////////////////////////////////////////
size_t CAreaProxy::GetNumberOfEntitiesInArea() const
{
	return m_pArea->GetEntityAmount();
}

//////////////////////////////////////////////////////////////////////////
EntityId CAreaProxy::GetEntityInAreaByIdx(size_t const index) const
{
	return m_pArea->GetEntityByIdx(index);
}

#include "Obj_CollisionTest.h"
#include "Transform.h"
#include "GameInstance.h"
#include "Model.h"
#include "Bounding_AABB.h"
#include "Bounding_Sphere.h"
#include "Bounding_OBB.h"

CObj_CollisionTest::CObj_CollisionTest ( EngineContext* pContext )
	: CGameObject ( pContext )
{
}

CObj_CollisionTest::CObj_CollisionTest ( const CObj_CollisionTest& Prototype )
	: CGameObject ( Prototype.m_pContext )
{
	m_pColliderCom = Prototype.m_pColliderCom;
	if ( m_pColliderCom != nullptr )
		Safe_AddRef ( m_pColliderCom );
}

HRESULT CObj_CollisionTest::Initialize_Prototype ()
{
	return S_OK;
}

HRESULT CObj_CollisionTest::Initialize ( void* pArg )
{
	if ( nullptr == pArg )
		return E_FAIL;

	Obj_CollisionTest_DESC* pDesc = static_cast< Obj_CollisionTest_DESC* >( pArg );


	_float3 position = pDesc->vPosition;
	_float3 rotation = pDesc->vRotation;
	_float3 scale = pDesc->vScale;

	// CGameObject::Initialize가 Transform 생성 및 속도 설정
	if ( FAILED ( __super::Initialize ( pArg ) ) )
		return E_FAIL;

	// JSON에서 받은 TRS 적용
	m_pTransformCom->Scaling ( scale.x , scale.y , scale.z );
	m_pTransformCom->RotationQuaternion ( rotation.x , rotation.y , rotation.z );
	m_pTransformCom->Set_State ( CTransform::STATE_POSITION ,
		XMVectorSet ( position.x , position.y , position.z , 1.f ) );

	m_eColliderType = pDesc->eColliderType;

	if ( FAILED ( Ready_Components () ) )
		return E_FAIL;

	return S_OK;
}

void CObj_CollisionTest::Priority_Update ( _float fTimeDelta )
{
}

void CObj_CollisionTest::Update ( _float fTimeDelta )
{
	__super::Update ( fTimeDelta );


	// 충돌체 업데이트
	_matrix WorldMatrix = m_pTransformCom->Get_WorldMatrix ();
	// 1. Main Collider
	if ( m_pColliderCom != nullptr ) {
		m_pColliderCom->Update ( m_pTransformCom->Get_WorldMatrix () );
	}
}

void CObj_CollisionTest::Late_Update ( _float fTimeDelta )
{
	m_pGameInstance->Add_RenderObject ( CRenderer::RG_NONBLEND , this );
}

#ifdef _DEBUG
void CObj_CollisionTest::Render ( ID3D12GraphicsCommandList* _commandList )
{
	// Transform 컴포넌트의 월드 행렬을 RootConstantBuffer에 넘겨준다.
	m_pGameInstance->Add_RenderCollider(m_pColliderCom );
}
#endif

HRESULT CObj_CollisionTest::Ready_Components ()
{
	// Collider 컴포넌트 생성
	// Main Collider

	if ( m_eColliderType == CCollider::TYPE_AABB )
	{
		CBounding_AABB::BOUND_AABB_DESC ColliderDesc;
		ColliderDesc.vExtents = _float3 ( 50.f , 125.0f , 50.f );
		ColliderDesc.vCenter = _float3 ( 0.0f , 125.0f , 0.0f );
		if ( FAILED ( Add_Component ( 1 , TEXT ( "Prototype_Component_AABB" ) ,
			TEXT ( "Com_Collider" ) , reinterpret_cast< CComponent** >( &m_pColliderCom ) , &ColliderDesc ) ) )
		{
			MSG_BOX ( "Failed to Add Component : Collider in Player_3rd" );
			return E_FAIL;
		}
	}
	else if( m_eColliderType == CCollider::TYPE_SPHERE)
	{
		CBounding_Sphere::BOUND_SPHERE_DESC ColliderDesc;
		ColliderDesc.fRadius = 55.f;
		ColliderDesc.vCenter = _float3 ( 0.f , 40.f , 0.f );
		if ( FAILED ( Add_Component ( 1 , TEXT ( "Prototype_Component_Sphere" ) ,
			TEXT ( "Com_Collider" ) , reinterpret_cast< CComponent** >( &m_pColliderCom ) , &ColliderDesc ) ) )
		{
			MSG_BOX ( "Failed to Add Component : Collider in Player_3rd" );
			return E_FAIL;
		}
	}
	else if( m_eColliderType == CCollider::TYPE_OBB)
	{
		CBounding_OBB::BOUND_OBB_DESC ColliderDesc;
		ColliderDesc.vExtents = _float3 ( 50.f , 125.0f , 50.f );
		ColliderDesc.vCenter = _float3 ( 0.0f , 125.0f , 0.0f );
		ColliderDesc.vRotation = _float3 ( 0.f , 0.f , 0.f );
		if ( FAILED ( Add_Component ( 1 , TEXT ( "Prototype_Component_OBB" ) ,
			TEXT ( "Com_Collider" ) , reinterpret_cast< CComponent** >( &m_pColliderCom ) , &ColliderDesc ) ) )
		{
			MSG_BOX ( "Failed to Add Component : Collider in Player_3rd" );
			return E_FAIL;
		}
	}
	else
	{
		return E_FAIL;
	}
	return S_OK;
}

CObj_CollisionTest* CObj_CollisionTest::Create ( EngineContext* pContext )
{
	CObj_CollisionTest* pInstance = new CObj_CollisionTest ( pContext );
	if ( FAILED ( pInstance->Initialize_Prototype () ) )
	{
		Safe_Release ( pInstance );
		MSG_BOX ( "Failed to Create : CObj_CollisionTest" );
	}
	return pInstance;
}

CGameObject* CObj_CollisionTest::Clone ( void* pArg )
{
	CObj_CollisionTest* pInstance = new CObj_CollisionTest ( *this );
	if ( FAILED ( pInstance->Initialize ( pArg ) ) )
	{
		Safe_Release ( pInstance );
		MSG_BOX ( "Failed to Clone : CObj_CollisionTest" );
	}
	return pInstance;
}

void CObj_CollisionTest::Free ()
{
	__super::Free ();
}
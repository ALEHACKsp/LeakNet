//========= Copyright � 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "c_basehlplayer.h"
#include "view.h"
#include "IClientVehicle.h"

extern ConVar default_fov;
extern ConVar sensitivity;
extern ConVar zoom_sensitivity_ratio;

int m_iIDEntIndex = 0;

IMPLEMENT_CLIENTCLASS_DT(C_BaseHLPlayer, DT_HL2_Player, CHL2_Player)
	RecvPropInt( RECVINFO( m_iIDEntIndex )),
	RecvPropDataTable(RECVINFO_DT(m_HL2Local),0, &REFERENCE_RECV_TABLE(DT_HL2Local), DataTableRecvProxy_StaticDataTable),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: Drops player's primary weapon
//-----------------------------------------------------------------------------
void CC_DropPrimary( void )
{
	C_BasePlayer *pPlayer = (C_BasePlayer *) C_BasePlayer::GetLocalPlayer();
	
	if ( pPlayer == NULL )
		return;

	pPlayer->Weapon_DropPrimary();
}

static ConCommand dropprimary("dropprimary", CC_DropPrimary, "dropprimary: Drops the primary weapon of the player.");

static	float savedFOV;

//-----------------------------------------------------------------------------
// Purpose: Zooms the camera in
//-----------------------------------------------------------------------------
void CC_ZoomIn( void )
{
	C_BaseHLPlayer *pPlayer = dynamic_cast<C_BaseHLPlayer *>(C_BasePlayer::GetLocalPlayer());
	
	if ( pPlayer == NULL )
		return;

	float targetFOV = max( 5, pPlayer->CurrentFOV() - 40.0f );

	pPlayer->Zoom( (targetFOV - pPlayer->CurrentFOV() ), 0.5f );
}

static ConCommand zoomin( "+zoom", CC_ZoomIn, "zoom: zooms the player's view in and out." );

//-----------------------------------------------------------------------------
// Purpose: Zooms the camera out
//-----------------------------------------------------------------------------
void CC_ZoomOut( void )
{
	C_BaseHLPlayer *pPlayer = dynamic_cast<C_BaseHLPlayer *>(C_BasePlayer::GetLocalPlayer());
	
	if ( pPlayer == NULL )
		return;

	pPlayer->Zoom( 0.0f, 0.5f );
}

static ConCommand zoomout( "-zoom", CC_ZoomOut, "zoom: zooms the player's view in and out." );

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
C_BaseHLPlayer::C_BaseHLPlayer()
{
	m_flZoomStart		= 0.0f;
	m_flZoomEnd			= 0.0f;
	m_flZoomRate		= 0.0f;
	m_flZoomStartTime	= 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void C_BaseHLPlayer::OnDataChanged( DataUpdateType_t updateType )
{
	// Make sure we're thinking
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
	UpdateIDTarget();
	BaseClass::OnDataChanged( updateType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseHLPlayer::Weapon_DropPrimary( void )
{
	engine->ServerCmd( "DropPrimary" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseHLPlayer::UpdateFOV( void )
{
	//Find our FOV with offset zoom value
	float flFOVOffset = CurrentFOV() + GetZoom();

	//Set this to the default if it's set to zero
	if ( flFOVOffset == 0 )
	{
		flFOVOffset = default_fov.GetInt();
	}
	
	// Clamp FOV in MP
//	int min_fov = ( engine->GetMaxClients() == 1 ) ? 5 : default_fov.GetInt();
	int min_fov = 5;
	
	// Don't let it go too low
	flFOVOffset = max( min_fov, flFOVOffset );

	// Set for rendering
	engine->SetFieldOfView( flFOVOffset );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float C_BaseHLPlayer::GetZoom( void )
{
	float fFOV = m_flZoomEnd;

	//See if we need to lerp the values
	if ( ( m_flZoomStart != m_flZoomEnd ) && ( m_flZoomRate > 0.0f ) )
	{
		float deltaTime = (float)( gpGlobals->curtime - m_flZoomStartTime ) / m_flZoomRate;

		if ( deltaTime >= 1.0f )
		{
			//If we're past the zoom time, just take the new value and stop lerping
			fFOV = m_flZoomStart = m_flZoomEnd;
		}
		else
		{
			fFOV = SimpleSplineRemapVal( deltaTime, 0.0f, 1.0f, m_flZoomStart, m_flZoomEnd );
		}
	}

	return fFOV;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : FOVOffset - 
//			time - 
//-----------------------------------------------------------------------------
void C_BaseHLPlayer::Zoom( float FOVOffset, float time )
{
	m_flZoomStart		= GetZoom();
	m_flZoomEnd			= FOVOffset;
	m_flZoomRate		= time;
	m_flZoomStartTime	= gpGlobals->curtime;
}


//-----------------------------------------------------------------------------
// Purpose: Hack to zero out player's pitch, use value from poseparameter instead
// Input  : flags - 
// Output : int
//-----------------------------------------------------------------------------
int C_BaseHLPlayer::DrawModel( int flags )
{
	// Not pitch for player
	QAngle saveAngles = GetLocalAngles();

	QAngle useAngles = saveAngles;
	useAngles[ PITCH ] = 0.0f;

	SetLocalAngles( useAngles );

	int iret = BaseClass::DrawModel( flags );

	SetLocalAngles( saveAngles );

	return iret;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_BaseHLPlayer::GetIDTarget( void ) const
{
	return m_iIDEntIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Update this client's target entity
//-----------------------------------------------------------------------------
void C_BaseHLPlayer::UpdateIDTarget( void )
{
	if ( !IsLocalPlayer() )
		return;

	// If the server's forcing us to a specific ID target, use it instead
	/*
	if ( m_TFLocal.m_iIDEntIndex )
	{
		m_iIDEntIndex = m_TFLocal.m_iIDEntIndex;
		return;
	}
	*/

	// Clear old target and find a new one
	m_iIDEntIndex = 0;

	trace_t tr;
	Vector vecStart, vecEnd;
	VectorMA( MainViewOrigin(), 1500, MainViewForward(), vecEnd );
	VectorMA( MainViewOrigin(), 48,   MainViewForward(), vecStart );
	UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );
	if ( tr.DidHitNonWorldEntity() )
	{
		C_BaseEntity *pEntity = tr.m_pEnt;
		IClientVehicle *vehicle = GetVehicle();
		C_BaseEntity *pVehicleEntity = vehicle ? vehicle->GetVehicleEnt() : NULL;

		if ( pEntity && (pEntity != this) && (pEntity != pVehicleEntity) )
		{
			// Make sure it's not an object
		//	if ( !dynamic_cast<C_BaseObject*>( pEntity ) )
			{
				m_iIDEntIndex = pEntity->entindex();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
/*
void C_BaseHLPlayer::SetIDEnt( C_BaseEntity *pEntity )
{
	if ( pEntity )
		m_TFLocal.m_iIDEntIndex = pEntity->entindex();
	else
		m_TFLocal.m_iIDEntIndex = 0;
}
*/
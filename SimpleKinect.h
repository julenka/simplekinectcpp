// SimpleKinect.h

#pragma once
#include <memory>

// from stdafx.h
#include <stdio.h>
#include <tchar.h>

#include <iostream>
#include <Windows.h>
#include "NuiApi.h"

using namespace std;


enum NUI_HANDEDNESS { 
	NUI_HANDEDNESS_RIGHT = 0, 
	NUI_HANDEDNESS_LEFT, 
	NUI_HANDEDNESS_COUNT};

class SimpleKinect
{
public:

	static const int        cDepthWidth  = 640;
	static const int        cDepthHeight = 480;
	static const int		cColorWidth	= 640;
	static const int		cColorHeight = 480;
	static const int        cBytesPerPixel = 4;
	static const int		cOpticalFlowWindowSize = 8;
	static const int		cNumPlayers = 7;
	static const int		cDepthMaximum = 4000;
	static const int		cDepthMinimum = 800;

	// TODO: add method to stop kinect

	// TODO: figure out a safe way to expose these, perhaps require copy to buffer
	BYTE*                   m_pCurrentRGB;
	USHORT*					m_pCurrentDepth;
	Vector4*				m_pCurrentWorld;		
	LONG*					m_pDepthToColorCoordinates;
	LONG*					m_pOpticFlow;
	LONG*					m_pOpticFlowDz;
	NUI_SKELETON_FRAME		m_curSkelFrame;
	NUI_SKELETON_DATA*		m_pActiveSkeleton;
	int						m_activePlayerIndex;

	float*					m_minDepthPerPlayer;
	float*					m_maxDepthPerPlayer;

	SimpleKinect();
	~SimpleKinect();

	HRESULT                 Initialize();
	BOOL					ReadyForUpdate();
	void					UpdateDepth();
	void					UpdateWorld();
	void					UpdateColor();
	void					UpdateSkeleton();
	void					UpdateOpticFlow();
	void					UpdateOpticFlowDz();
	

private:
	// Current Kinect
	INuiSensor*             m_pNuiSensor;
   
	HANDLE                  m_pDepthStreamHandle;
	HANDLE                  m_pColorStreamHandle;

	BYTE*                   m_pPrevRGB;
	USHORT*					m_pPrevDepth;
	Vector4*				m_pPrevWorld;
	


	BOOL					m_isInitialized;
};

// Safe release for interfaces
template<class Interface>
inline void SafeRelease( Interface *& pInterfaceToRelease )
{
    if ( pInterfaceToRelease != NULL )
    {
        pInterfaceToRelease->Release();
        pInterfaceToRelease = NULL;
    }
}

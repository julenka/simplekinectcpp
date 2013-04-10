// This is the main DLL file.
#pragma once
#include "SimpleKinect.h"

SimpleKinect::SimpleKinect()
{
	// initialize arrays
	m_pCurrentRGB = new BYTE[cColorWidth * cColorHeight * cBytesPerPixel];
	m_pPrevRGB = new BYTE[cColorWidth * cColorHeight * cBytesPerPixel];

	m_pCurrentDepth = new USHORT[cDepthWidth * cDepthHeight];
	m_pPrevDepth = new USHORT[cDepthWidth * cDepthHeight];
	
	m_pCurrentWorld = new Vector4[cDepthWidth * cDepthHeight];
	m_pPrevWorld = new Vector4[cDepthWidth * cDepthHeight];
	
	m_pDepthToColorCoordinates = new LONG[cDepthWidth * cDepthHeight * 2];
	m_pOpticFlow = new LONG[cDepthWidth * cDepthHeight * 3];
	m_pOpticFlowDz = new LONG[cDepthWidth * cDepthHeight];

	m_isInitialized = false;

	m_minDepthPerPlayer = new float[cNumPlayers + 1];
	m_maxDepthPerPlayer = new float[cNumPlayers + 1];

	for(int i = 0; i < cNumPlayers + 1; i++)
	{
		m_minDepthPerPlayer[i] = 1000000;
		m_maxDepthPerPlayer[i] = 0;
	}
}

SimpleKinect::~SimpleKinect()
{
	if (m_pNuiSensor)
    {
        m_pNuiSensor->NuiShutdown();
    }
    
    SafeRelease(m_pNuiSensor);

	delete[] m_pCurrentRGB;
	delete[] m_pPrevRGB;
	delete[] m_pCurrentDepth;
	delete[] m_pPrevDepth;
	delete[] m_pDepthToColorCoordinates;
	delete[] m_pOpticFlow;
	delete[] m_pCurrentWorld;
	delete[] m_pPrevWorld;
	delete[] m_pOpticFlowDz;
	delete[] m_minDepthPerPlayer;
	delete[] m_maxDepthPerPlayer;
}

// TODO: allow for customizing which streams are initialized
HRESULT SimpleKinect::Initialize()
{
	INuiSensor * pNuiSensor;
    HRESULT hr;

    int iSensorCount = 0;
    hr = NuiGetSensorCount(&iSensorCount);
    if (FAILED(hr))
    {
        return hr;
    }

    // Find the correct sensor
	cout << "SimpleKinect: Finding sensor..." << endl;

    for (int i = 0; i < iSensorCount; ++i)
    {
        // Create the sensor so we can check status, if we can't create it, move on to the next
        hr = NuiCreateSensorByIndex(i, &pNuiSensor);
        if (FAILED(hr))
        {
            continue;
        }

        // Get the status of the sensor, and if connected, then we can initialize it
        hr = pNuiSensor->NuiStatus();
        if (S_OK == hr)
        {
            m_pNuiSensor = pNuiSensor;
            break;
        }

        // This sensor wasn't OK, so release it since we're not using it
        pNuiSensor->Release();
    }

	
	// Open depth stream
    if (NULL != m_pNuiSensor)
    {
		cout << "SimpleKinect: Opening depth stream.." << endl;
        // Initialize the Kinect and specify that we'll be using depth, player index and color
		hr = m_pNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_SKELETON ); 
        if (SUCCEEDED(hr))
        {

            // Open a depth image stream to receive depth frames
            hr = m_pNuiSensor->NuiImageStreamOpen(
				NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX,
                NUI_IMAGE_RESOLUTION_640x480,
                0,
                2,
                NULL,
                &m_pDepthStreamHandle);
        } else {
			cerr << "SimpleKinect: Failed to open depth and player stream!" << endl;
			return E_FAIL;
		}

		// open color stream
		cout << "SimpleKinect: Opening color stream.." << endl;
		hr = m_pNuiSensor->NuiImageStreamOpen(
			NUI_IMAGE_TYPE_COLOR,
			NUI_IMAGE_RESOLUTION_640x480,
			0,
			2,
			NULL,
			&m_pColorStreamHandle);
		if(!SUCCEEDED(hr))
		{
			cerr << "SimpleKinect: Failed to open color stream!" << endl;
			return E_FAIL;
		}

		cout << "SimpleKinect: Opening skeleton stream.." << endl;
		// open skeleton stream
		if(HasSkeletalEngine(m_pNuiSensor))
		{
			hr = m_pNuiSensor->NuiSkeletonTrackingEnable(NULL, 
				NUI_SKELETON_TRACKING_FLAG_ENABLE_IN_NEAR_RANGE | 
				NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT |
				NUI_SKELETON_TRACKING_FLAG_SUPPRESS_NO_FRAME_DATA);
			if(!SUCCEEDED(hr))
			{
				cerr << "SimpleKinect: Failed to open skeleton stream!" << endl;
				return E_FAIL;
			}
		}
		else 
		{
			cerr << "SimpleKinect: Sensor does not have skeletal engine!" << endl;
		}
		
    }

    if (NULL == m_pNuiSensor || FAILED(hr))
    {
        cerr << "SimpleKinect: No ready Kinect found!" << endl;
        return E_FAIL;
    } else 
	{
		BSTR uniqueId = m_pNuiSensor->NuiUniqueId();
		cout << "SimpleKinect Connected to sensor " << uniqueId << endl;
	}
	m_isInitialized = true;
    return hr;
}

BOOL	SimpleKinect::ReadyForUpdate()
{
	return m_isInitialized;
}

void SimpleKinect::UpdateDepth()
{
	HRESULT hr;
    NUI_IMAGE_FRAME imageFrame;
	hr = m_pNuiSensor->NuiImageStreamGetNextFrame(m_pDepthStreamHandle, 0, &imageFrame);
    if (FAILED(hr))
    {
        return;
    }

	INuiFrameTexture * pTexture = imageFrame.pFrameTexture;
    NUI_LOCKED_RECT LockedRect;

	// Lock the frame data so the Kinect knows not to modify it while we're reading it
    pTexture->LockRect(0, &LockedRect, NULL, 0);

	// make sure we're receiving real data
	if (LockedRect.Pitch == 0) 
	{
		return;
	}

	// copy current to old
	memcpy(m_pPrevDepth, m_pCurrentDepth, LockedRect.size);
	    
	// copy in data to current
	memcpy(m_pCurrentDepth, LockedRect.pBits, LockedRect.size);

    // We're done with the texture so unlock it
    pTexture->UnlockRect(0);

    // Release the frame
    m_pNuiSensor->NuiImageStreamReleaseFrame(m_pDepthStreamHandle, &imageFrame);

	
	// update the mapping from depth to color
	hr = m_pNuiSensor->NuiImageGetColorPixelCoordinateFrameFromDepthPixelFrameAtResolution(
		NUI_IMAGE_RESOLUTION_640x480,
		NUI_IMAGE_RESOLUTION_640x480,
		cDepthWidth * cDepthHeight,
		m_pCurrentDepth,
		cDepthWidth * cDepthHeight * 2,
		m_pDepthToColorCoordinates
		);

	if(FAILED(hr))
	{
		return;
	}
}

void SimpleKinect::UpdateWorld()
{
	// copy current world coords to prev world coords
	memcpy(m_pPrevWorld, m_pCurrentWorld, sizeof(Vector4) * cDepthWidth * cDepthHeight);

	// convert depth coords to world coords in depth fram
	int x=0,y=0;
	USHORT* pBufferRun = m_pCurrentDepth;
	Vector4* pWorldRun = m_pCurrentWorld;
	const USHORT* pBufferEnd = pBufferRun + (cDepthWidth * cDepthHeight);
	
	float fSkeletonX = 0, fSkeletonY = 0;
	USHORT depth, player;
	while(pBufferRun < pBufferEnd)
	{
		if(*pBufferRun < NUI_IMAGE_DEPTH_MAXIMUM && *pBufferRun > NUI_IMAGE_DEPTH_MINIMUM){
			depth = NuiDepthPixelToDepth(*pBufferRun);
			player = NuiDepthPixelToPlayerIndex(*pBufferRun);

			float depthM = depth;
			//
			// Center of depth sensor is at (0,0,0) in skeleton space, and
			// and (width/2,height/2) in depth image coordinates.  Note that positive Y
			// is up in skeleton space and down in image coordinates.
			//
			fSkeletonX = (x/2.0 - 160)  * NUI_CAMERA_DEPTH_IMAGE_TO_SKELETON_MULTIPLIER_320x240 * depthM;
			fSkeletonY = -(y/2.0 - 120) * NUI_CAMERA_DEPTH_IMAGE_TO_SKELETON_MULTIPLIER_320x240 * depthM;

			(*pWorldRun).x = fSkeletonX;
			(*pWorldRun).y = fSkeletonY;
			(*pWorldRun).z = depthM;


			// update min/max
			if(depthM < m_minDepthPerPlayer[player])
			{
				m_minDepthPerPlayer[player] = depthM;
			}

			if(depthM > m_maxDepthPerPlayer[player])
			{
				m_maxDepthPerPlayer[player] = depthM;
			}

		} else 
		{
			(*pWorldRun).x = 0;
			(*pWorldRun).y = 0;
			(*pWorldRun).z = 0;
		}


		x++;
		if(x >= cDepthWidth)
		{
			y++;
			x = 0;
		}
		pBufferRun++;
		pWorldRun++;
	}
}

void SimpleKinect::UpdateColor()
{
	HRESULT hr;
    NUI_IMAGE_FRAME imageFrame;
	hr = m_pNuiSensor->NuiImageStreamGetNextFrame(m_pColorStreamHandle, 0, &imageFrame);
    if (FAILED(hr))
    {
        return;
    }

	INuiFrameTexture * pTexture = imageFrame.pFrameTexture;
    NUI_LOCKED_RECT LockedRect;
	// Lock the frame data so the Kinect knows not to modify it while we're reading it
    pTexture->LockRect(0, &LockedRect, NULL, 0);

	// make sure we're receiving real data
	if (LockedRect.Pitch == 0) 
	{
		return;
	}

	// copy current to old
	memcpy(m_pPrevRGB, m_pCurrentRGB, LockedRect.size);
	    
	// copy in data to current
	memcpy(m_pCurrentRGB, LockedRect.pBits, LockedRect.size);

    // We're done with the texture so unlock it
    pTexture->UnlockRect(0);

    // Release the frame
    m_pNuiSensor->NuiImageStreamReleaseFrame(m_pColorStreamHandle, &imageFrame);

}

void SimpleKinect::UpdateSkeleton()
{
	bool foundSkeleton = false;
	if(SUCCEEDED(m_pNuiSensor->NuiSkeletonGetNextFrame(0, &m_curSkelFrame)))
	{
		for ( int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
        {
            NUI_SKELETON_TRACKING_STATE trackingState = m_curSkelFrame.SkeletonData[i].eTrackingState;

            if ( trackingState == NUI_SKELETON_TRACKED || trackingState == NUI_SKELETON_POSITION_ONLY )
            {
				if(!foundSkeleton) m_pActiveSkeleton  = &m_curSkelFrame.SkeletonData[i];
				m_activePlayerIndex = i;
                foundSkeleton = true;
            }
        }
	}
	
    // no skeletons!
    if( !foundSkeleton )
    {
        m_pActiveSkeleton = NULL;
		m_activePlayerIndex = -1;
		return ;
    }

	// smooth out the skeleton data
    HRESULT hr = m_pNuiSensor->NuiTransformSmooth(&m_curSkelFrame,NULL);

}

void SimpleKinect::UpdateOpticFlow()
{
	USHORT player;
	Vector4 prevWorld, curWorld;

	LONG* pOpticFlow = m_pOpticFlow;

    // end pixel is start + width*height - 1
	int x = cOpticalFlowWindowSize / 2,  y = cOpticalFlowWindowSize / 2, i = 0;
	int xx = 0, yy = 0, dx = 0, dy = 0, dz = 0, mindx = 0, mindy = 0, mindz =0 ;

	USHORT* pCurrentDepth = m_pCurrentDepth + (y * cDepthWidth) + x;
    const USHORT * pBufferEnd = pCurrentDepth + (cDepthWidth * cDepthHeight);
	// TODO: Use world coordiantes!
	while ( pCurrentDepth < pBufferEnd )
    {
		player = NuiDepthPixelToPlayerIndex(*pCurrentDepth);
		curWorld = m_pCurrentWorld[y * cDepthWidth + x];
		dx=0, dy=0, dz=0, mindx=0,mindy=0,mindz=0;

		// to do: change backto 0 when done debugging

			// find coordinate of pixel that has smallest distance to current pixel

		double minDistance = LONG_MAX;
		
		for(yy = y - cOpticalFlowWindowSize / 2; yy < y + cOpticalFlowWindowSize / 2; yy++)
		{
			for (xx = x - cOpticalFlowWindowSize / 2; xx < x + cOpticalFlowWindowSize / 2; xx++)
			{
				prevWorld = m_pPrevWorld[(yy) * cDepthWidth + xx];

				dy = curWorld.y - prevWorld.y;
				dx = curWorld.x - prevWorld.x;
				dz = curWorld.z - prevWorld.z;
				double distance = sqrtl(dx * dx + dy * dy + dz * dz);
				if(distance < minDistance)
				{
					// make sure that we are not accidentally recording giant jumps in z/x/y, whihc represent just noise
					minDistance = distance;
					mindx = dx;
					mindy = dy;
					mindz = dz;
				}
			}
		}
		

        pOpticFlow[0] = mindx;
		pOpticFlow[1] = mindy;
		pOpticFlow[2] = mindz;

		// update indices
		x+= cOpticalFlowWindowSize;
		if(x >= cDepthWidth){
			x = cOpticalFlowWindowSize / 2;
			y+= cOpticalFlowWindowSize;
			pCurrentDepth = m_pCurrentDepth + y * cDepthWidth + x;
		} else 
		{
			pCurrentDepth += cOpticalFlowWindowSize;
		}
		pOpticFlow += 3; 
    }
}



void SimpleKinect::UpdateOpticFlowDz()
{
	USHORT player;
	Vector4 prevWorld, curWorld;

	LONG* pOpticFlow = m_pOpticFlowDz;

    // end pixel is start + width*height - 1
	int x = 0,  y = 0, i = 0;

	USHORT* pCurrentDepth = m_pCurrentDepth + (y * cDepthWidth) + x;
    const USHORT * pBufferEnd = pCurrentDepth + (cDepthWidth * cDepthHeight);

	// TODO: Use world coordiantes!
	while ( pCurrentDepth < pBufferEnd )
    {
		player = NuiDepthPixelToPlayerIndex(*pCurrentDepth);


		curWorld = m_pCurrentWorld[y * cDepthWidth + x];
		prevWorld = m_pPrevWorld[y * cDepthWidth + x];
		
		*pOpticFlow = curWorld.z - prevWorld.z;

		// update indices
		x++;
		if(x >= cDepthWidth){
			x = 0;
			y++;
			pCurrentDepth = m_pCurrentDepth + y * cDepthWidth + x;
		} else 
		{
			pCurrentDepth += cOpticalFlowWindowSize;
		}
		pOpticFlow ++; 
    }
}
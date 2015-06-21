/********************************************************************
Vireio Perception: Open-Source Stereoscopic 3D Driver
Copyright (C) 2012 Andres Hernandez

File <OculusDirectToRiftView.cpp> and
Class <OculusDirectToRiftView> :
Copyright (C) 2012 Andres Hernandez
Modifications Copyright (C) 2013 Chris Drain

Vireio Perception Version History:
v1.0.0 2012 by Andres Hernandez
v1.0.X 2013 by John Hicks, Neil Schneider
v1.1.x 2013 by Primary Coding Author: Chris Drain
Team Support: John Hicks, Phil Larkson, Neil Schneider
v2.0.x 2013 by Denis Reischl, Neil Schneider, Joshua Brown

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
********************************************************************/

#include "OculusDirectToRiftView.h"
#include "StereoView.h"
#include "D3DProxyDevice.h"
#include "Resource.h"
#include "OculusTracker.h"

//------------------------------------------------------------
// ovrSwapTextureSet wrapper class that also maintains the render target views
// needed for D3D11 rendering.
struct OculusTexture
{
    ovrSwapTextureSet      * TextureSet;
    ID3D11RenderTargetView * TexRtv[3];

    OculusTexture(ovrHmd hmd, Sizei size)
    {
        D3D11_TEXTURE2D_DESC dsDesc;
        dsDesc.Width            = size.w;
        dsDesc.Height           = size.h;
        dsDesc.MipLevels        = 1;
        dsDesc.ArraySize        = 1;
        dsDesc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        dsDesc.SampleDesc.Count = 1;   // No multi-sampling allowed
        dsDesc.SampleDesc.Quality = 0;
        dsDesc.Usage            = D3D11_USAGE_DEFAULT;
        dsDesc.CPUAccessFlags   = 0;
        dsDesc.MiscFlags        = 0;
        dsDesc.BindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

        ovrHmd_CreateSwapTextureSetD3D11(hmd, DIRECTX.Device, &dsDesc, &TextureSet);
        for (int i = 0; i < TextureSet->TextureCount; ++i)
        {
            ovrD3D11Texture* tex = (ovrD3D11Texture*)&TextureSet->Textures[i];
            DIRECTX.Device->CreateRenderTargetView(tex->D3D11.pTexture, NULL, &TexRtv[i]);
        }
    }

    void AdvanceToNextTexture()
    {
        TextureSet->CurrentIndex = (TextureSet->CurrentIndex + 1) % TextureSet->TextureCount;
    }
    void Release(ovrHmd hmd)
    {
        ovrHmd_DestroySwapTextureSet(hmd, TextureSet);
    }
};

//------------------------------------------------------------------------- 
struct VoidScene  
{
    Model * screen;
	Texture *texture;

 	VoidScene(ID3D11Texture2D *pTexture) :
		m_pTexture(pTexture)
    {
		SHOW_CALL("VoidScene()");

		//We have a hold of this
		m_pTexture->AddRef();

		D3D11_TEXTURE2D_DESC desc;
		pTexture->GetDesc(&desc);
#if _DEBUG
		vireio::debugf("Texture Format: %i", desc.Format);
#endif

		float aspect = (float)(desc.Width) / (float)(desc.Height);
		texture = new Texture(false, Sizei(desc.Width, desc.Height));
		screen = new Model(texture, -1.0f * aspect, 0.0f, 2.0f, aspect);
    }

	~VoidScene()
	{
		SHOW_CALL("~VoidScene()");
		texture->Tex->Release();
		m_pTexture->Release();
		delete texture;
		delete screen;
	}

   void Render(Matrix4f projView, float R, float G, float B, float A, bool standardUniforms)
    {   
		SHOW_CALL("Render()");
		DIRECTX.Context->CopyResource(texture->Tex, m_pTexture);
		screen->Render(projView,R,G,B,A,standardUniforms);    
	}

private:
	ID3D11Texture2D *m_pTexture;

};

/**
* Constructor.
* @param config Game configuration.
* @param hmd Oculus Rift Head Mounted Display info.
***/ 
OculusDirectToRiftView::OculusDirectToRiftView(ProxyConfig *config, HMDisplayInfo *hmd, MotionTracker *motionTracker) : 
	StereoView(config),
	hmdInfo(hmd)
{
	SHOW_CALL("OculusDirectToRiftView::OculusDirectToRiftView()");

	if (motionTracker &&
		config->tracker_mode == MotionTracker::OCULUSTRACK)
	{
		m_pOculusTracker = static_cast<OculusTracker*>(motionTracker);
		rift = m_pOculusTracker->GetOVRHmd();

		bool initialized = DIRECTX.InitWindowAndDevice(NULL, Recti(0, 0, 0, 0), L"Vireio Perception - Ignore this dialog");
		VALIDATE(initialized, "Unable to initialize window and D3D11 device.");

		for (int eye = 0; eye < 2; eye++)
		{
			Sizei idealSize = ovrHmd_GetFovTextureSize(rift, (ovrEyeType)eye, rift->DefaultEyeFov[eye], 1.0f);
			pEyeRenderTexture[eye]      = new OculusTexture(rift, idealSize);
			pEyeDepthBuffer[eye]        = new DepthBuffer(DIRECTX.Device, idealSize);
			eyeRenderViewport[eye].Pos  = Vector2i(0, 0);
			eyeRenderViewport[eye].Size = idealSize;

			vireio::debugf("Eye: %i, Size(%i, %i)", eye, idealSize.h, idealSize.w);
		}	

		// Setup VR components, filling out description
		eyeRenderDesc[0] = ovrHmd_GetRenderDesc(rift, ovrEye_Left, rift->DefaultEyeFov[0]);
		eyeRenderDesc[1] = ovrHmd_GetRenderDesc(rift, ovrEye_Right, rift->DefaultEyeFov[1]);
	}
}

void OculusDirectToRiftView::ReleaseEverything()
{
	SHOW_CALL("OculusDirectToRiftView::ReleaseEverything()");

	if (m_pScene[0])
	{
		delete m_pScene[0];
		m_pScene[0] = NULL;
	}

	if (m_pScene[1])
	{
		delete m_pScene[1];
		m_pScene[1] = NULL;
	}


	//Call base class
	StereoView::ReleaseEverything();
}


void OculusDirectToRiftView::Draw(D3D9ProxySurface* stereoCapableSurface)
{
    // Get both eye poses simultaneously, with IPD offset already included. 
    ovrPosef         EyeRenderPose[2];
    ovrVector3f      HmdToEyeViewOffset[2] = { eyeRenderDesc[0].HmdToEyeViewOffset,
                                                eyeRenderDesc[1].HmdToEyeViewOffset };

	//If we aren't in disconnected mode, we want to render here mono-scopically
	ovr_CalcEyePoses(m_pOculusTracker->GetOvrTrackingState().HeadPose.ThePose, HmdToEyeViewOffset, EyeRenderPose);

    // Render Scene to Eye Buffers
    for (int eye = 0; eye < 2; eye++)
    {
        // Increment to use next texture, just before writing
        pEyeRenderTexture[eye]->AdvanceToNextTexture();

        // Clear and set up rendertarget
        int texIndex = pEyeRenderTexture[eye]->TextureSet->CurrentIndex;
        DIRECTX.SetAndClearRenderTarget(pEyeRenderTexture[eye]->TexRtv[texIndex], pEyeDepthBuffer[eye]);
        DIRECTX.SetViewport(Recti(eyeRenderViewport[eye]));

		if (!m_pScene[eye])
		{
			ID3D11Resource *tempResource11 = NULL;
			HANDLE textHandle = NULL;
			if (config->swap_eyes)
			{
				textHandle = eye == 0 ? stereoCapableSurface->getHandleRight() : stereoCapableSurface->getHandleLeft();
			}
			else
			{
				textHandle = eye == 0 ? stereoCapableSurface->getHandleLeft() : stereoCapableSurface->getHandleRight();
			}

			IID iid = __uuidof(ID3D11Resource);
			if (FAILED(DIRECTX.Device->OpenSharedResource(textHandle, iid, (void**)(&tempResource11))))
			{
				vireio::debugf("Failed to open shared resource");
				return;
			}

			//QI for the texture
			ID3D11Texture2D *pDX9Texture = NULL;
			tempResource11->QueryInterface(__uuidof(ID3D11Texture2D), (void**)(&pDX9Texture)); 
			tempResource11->Release();

			//Create the void scene
			m_pScene[eye] = new VoidScene(pDX9Texture);

			//We don't need to keep the ref here now
			pDX9Texture->Release();
		}


        // View and projection matrices for the main camera
		Camera mainCam(Vector3f(0.0f, 1.0f, ZoomOutScale + m_screenViewGlideFactor), Matrix4f::RotationY(0.0f));

        // View and projection matrices for the stereo camera
        Camera finalCam(mainCam.Pos + mainCam.Rot.Transform(EyeRenderPose[eye].Position),
            mainCam.Rot * Matrix4f(EyeRenderPose[eye].Orientation));

		//View matrix is a monoscopic straight ahead camera is not in disconnected screen view mode
		Matrix4f view = m_disconnectedScreenView ? finalCam.GetViewMatrix() : mainCam.GetViewMatrix();
        Matrix4f proj = ovrMatrix4f_Projection(eyeRenderDesc[eye].Fov, 0.2f, 1000.0f, ovrProjection_RightHanded);
		
		m_pScene[eye]->Render(proj*view, 1, 1, 1, 1, true);
    }

    // Initialize our single full screen Fov layer.
    ovrLayerEyeFov ld;
    ld.Header.Type  = ovrLayerType_EyeFov;
    ld.Header.Flags = 0;

    for (int eye = 0; eye < 2; eye++)
    {
        ld.ColorTexture[eye] = pEyeRenderTexture[eye]->TextureSet;
        ld.Viewport[eye]     = eyeRenderViewport[eye];
        ld.Fov[eye]          = rift->DefaultEyeFov[eye];
		ld.RenderPose[eye]   = EyeRenderPose[eye];
    }

    ovrLayerHeader* layers = &ld.Header;
    ovrResult result = ovrHmd_SubmitFrame(rift, 0, nullptr, &layers, 1);


	//Screen output
	IDirect3DSurface9* leftImage = stereoCapableSurface->getActualLeft();
	m_pActualDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
	m_pActualDevice->StretchRect(leftImage, NULL, backBuffer, NULL, D3DTEXF_NONE);
	backBuffer->Release();
}



/**
* Sets vertex shader constants.
***/ 
void OculusDirectToRiftView::SetViewEffectInitialValues() 
{
	SHOW_CALL("OculusDirectToRiftView::SetViewEffectInitialValues\n");
}

void OculusDirectToRiftView::PostViewEffectCleanup()
{
}

void OculusDirectToRiftView::SetVRMouseSquish(float squish)
{
}

/**
* Calculate all vertex shader constants.
***/ 
void OculusDirectToRiftView::CalculateShaderVariables()
{
	SHOW_CALL("OculusDirectToRiftView::CalculateShaderVariables");

	if (!m_disconnectedScreenView)
	{
		if (m_screenViewGlideFactor > 0.0f)
			m_screenViewGlideFactor -= 0.04f;
	}
	else
	{
		//Disconnected screen view not active
		if (m_screenViewGlideFactor < 1.0f)
			m_screenViewGlideFactor += 0.04f;
	}

}

/**
* Loads Oculus Rift shader effect files.
***/ 
void OculusDirectToRiftView::InitShaderEffects()
{
	SHOW_CALL("OculusDirectToRiftView::InitShaderEffects");
}


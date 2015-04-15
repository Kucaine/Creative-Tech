
// http://msdn.microsoft.com/en-us/library/windows/apps/ff729721.aspx
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include "resource.h"

#include "NuiApi.h"
// Need to re add kinect10.lib to linker input
#include <iostream>

// opencv
#include "opencv2/core/core.hpp"
#include "opencv2/flann/miniflann.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/photo/photo.hpp"
#include "opencv2/video/video.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/ml/ml.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/contrib/contrib.hpp"
#include "opencv2/core/core_c.h"
#include "opencv2/highgui/highgui_c.h"
#include "opencv2/imgproc/imgproc_c.h"

// Namespaces
using namespace cv;
using namespace std;
using namespace DirectX;

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
    XMFLOAT3 Pos;
};


struct ConstantBuffer
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
	XMFLOAT2 fCentre;
};


//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE               g_hInst = nullptr;
HWND                    g_hWnd = nullptr;
D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*           g_pd3dDevice = nullptr;
ID3D11Device1*          g_pd3dDevice1 = nullptr;
ID3D11DeviceContext*    g_pImmediateContext = nullptr;
ID3D11DeviceContext1*   g_pImmediateContext1 = nullptr;
IDXGISwapChain*         g_pSwapChain = nullptr;
IDXGISwapChain1*        g_pSwapChain1 = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader*     g_pVertexShader = nullptr;
ID3D11PixelShader*      g_pPixelShader = nullptr;
ID3D11InputLayout*      g_pVertexLayout = nullptr;
ID3D11Buffer*           g_pVertexBuffer = nullptr;
ID3D11Buffer*           g_pIndexBuffer = nullptr;
ID3D11Buffer*           g_pConstantBuffer = nullptr;
XMMATRIX                g_World;
XMMATRIX                g_View;
XMMATRIX                g_Projection;
XMFLOAT2				g_GaussCentre;
INuiSensor*				pNuiSensor = nullptr;
NUI_IMAGE_FRAME			imageFrame;
HANDLE                  m_pDepthStreamHandle;
HANDLE                  m_hNextDepthFrameEvent;
BYTE*                   m_depthRGBX;
HANDLE                  m_pColorStreamHandle;
HANDLE                  m_hNextColorFrameEvent;
INuiFrameTexture*		frameTexture;
const int				width = 800;
const int				height = 600;
long					depthToRgbMap[width*height*2];
float					depthDataArray[width*height*3];
float					colourDataArray[width*height*3];
int						arc;
char**					argv;
//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow );
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );
void Render();
HRESULT KinectConnected();
void DepthProcessing(float depthDataArray[]);
void ColourProcessing(float colourDataArray[]);
int OpenCV(int argc, char** argv);


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow )
{
    UNREFERENCED_PARAMETER( hPrevInstance );
    UNREFERENCED_PARAMETER( lpCmdLine );

    if( FAILED( InitWindow( hInstance, nCmdShow ) ) )
        return 0;

    if( FAILED( InitDevice() ) )
    {
        CleanupDevice();
        return 0;
    }
	
	
	KinectConnected();

	/*VideoCapture cap(CV_CAP_OPENNI);

	if (!cap.isOpened())
	{
		cout << "Cannot open the video cam" << endl;
		return 0;
	}

	double width = cap.get(CV_CAP_PROP_FRAME_WIDTH);
	double height = cap.get(CV_CAP_PROP_FRAME_HEIGHT);

	cout << "Frame size: " << width << "x" << height << endl;

	namedWindow("Kinect", CV_WINDOW_AUTOSIZE);*/

	/*while(1)
	{
		Mat frame;

		bool success = cap.read(frame);

		if (!success)
		{
			cout << " Cannot reada frame from video camera" << endl;
			break;
		}

		imshow("Kinect", frame);
	}
	*/

    // Main message loop
    MSG msg = {0};
    while( WM_QUIT != msg.message )
    {
        if( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
        else
        {
			
			DepthProcessing(depthDataArray);
			ColourProcessing(colourDataArray);
            Render();
        }
    }



    CleanupDevice();

    return ( int )msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow )
{
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof( WNDCLASSEX );
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon( hInstance, ( LPCTSTR )IDI_TUTORIAL1 );
    wcex.hCursor = LoadCursor( nullptr, IDC_ARROW );
    wcex.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"TutorialWindowClass";
    wcex.hIconSm = LoadIcon( wcex.hInstance, ( LPCTSTR )IDI_TUTORIAL1 );
    if( !RegisterClassEx( &wcex ) )
        return E_FAIL;

    // Create window
    g_hInst = hInstance;
    RECT rc = { 0, 0, 800, 600 };
    AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
    g_hWnd = CreateWindow( L"TutorialWindowClass", L"Direct3D 11 Tutorial 4: 3D Spaces",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
                           nullptr );
    if( !g_hWnd )
        return E_FAIL;

    ShowWindow( g_hWnd, nCmdShow );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DCompile
//
// With VS 11, we could load up prebuilt .cso files instead...
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    HRESULT hr = S_OK;

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;

    // Disable optimizations to further improve shader debugging
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pErrorBlob = nullptr;
    hr = D3DCompileFromFile( szFileName, nullptr, nullptr, szEntryPoint, szShaderModel, 
        dwShaderFlags, 0, ppBlobOut, &pErrorBlob );
    if( FAILED(hr) )
    {
        if( pErrorBlob )
        {
            OutputDebugStringA( reinterpret_cast<const char*>( pErrorBlob->GetBufferPointer() ) );
            pErrorBlob->Release();
        }
        return hr;
    }
    if( pErrorBlob ) pErrorBlob->Release();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect( g_hWnd, &rc );
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE( driverTypes );

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
	UINT numFeatureLevels = ARRAYSIZE( featureLevels );

    for( UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++ )
    {
        g_driverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDevice( nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
                                D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext );

        if ( hr == E_INVALIDARG )
        {
            // DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
            hr = D3D11CreateDevice( nullptr, g_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
                                    D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext );
        }

        if( SUCCEEDED( hr ) )
            break;
    }
    if( FAILED( hr ) )
        return hr;

    // Obtain DXGI factory from device (since we used nullptr for pAdapter above)
    IDXGIFactory1* dxgiFactory = nullptr;
    {
        IDXGIDevice* dxgiDevice = nullptr;
        hr = g_pd3dDevice->QueryInterface( __uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice) );
        if (SUCCEEDED(hr))
        {
            IDXGIAdapter* adapter = nullptr;
            hr = dxgiDevice->GetAdapter(&adapter);
            if (SUCCEEDED(hr))
            {
                hr = adapter->GetParent( __uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory) );
                adapter->Release();
            }
            dxgiDevice->Release();
        }
    }
    if (FAILED(hr))
        return hr;

    // Create swap chain
    IDXGIFactory2* dxgiFactory2 = nullptr;
    hr = dxgiFactory->QueryInterface( __uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2) );
    if ( dxgiFactory2 )
    {
        // DirectX 11.1 or later
        hr = g_pd3dDevice->QueryInterface( __uuidof(ID3D11Device1), reinterpret_cast<void**>(&g_pd3dDevice1) );
        if (SUCCEEDED(hr))
        {
            (void) g_pImmediateContext->QueryInterface( __uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&g_pImmediateContext1) );
        }

        DXGI_SWAP_CHAIN_DESC1 sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.Width = width;
        sd.Height = height;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 1;

        hr = dxgiFactory2->CreateSwapChainForHwnd( g_pd3dDevice, g_hWnd, &sd, nullptr, nullptr, &g_pSwapChain1 );
        if (SUCCEEDED(hr))
        {
            hr = g_pSwapChain1->QueryInterface( __uuidof(IDXGISwapChain), reinterpret_cast<void**>(&g_pSwapChain) );
        }

        dxgiFactory2->Release();
    }
    else
    {
        // DirectX 11.0 systems
        DXGI_SWAP_CHAIN_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = 1;
        sd.BufferDesc.Width = width;
        sd.BufferDesc.Height = height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = g_hWnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;

        hr = dxgiFactory->CreateSwapChain( g_pd3dDevice, &sd, &g_pSwapChain );
    }

    // Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
    dxgiFactory->MakeWindowAssociation( g_hWnd, DXGI_MWA_NO_ALT_ENTER );

    dxgiFactory->Release();

    if (FAILED(hr))
        return hr;

    // Create a render target view
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = g_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast<void**>( &pBackBuffer ) );
    if( FAILED( hr ) )
        return hr;

    hr = g_pd3dDevice->CreateRenderTargetView( pBackBuffer, nullptr, &g_pRenderTargetView );
    pBackBuffer->Release();
    if( FAILED( hr ) )
        return hr;

    g_pImmediateContext->OMSetRenderTargets( 1, &g_pRenderTargetView, nullptr );

    // Setup the viewport
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports( 1, &vp );

    // Compile the vertex shader
    ID3DBlob* pVSBlob = nullptr;
    hr = CompileShaderFromFile( L"fx.fx", "VS", "vs_4_0", &pVSBlob );
    if( FAILED( hr ) )
    {
        MessageBox( nullptr,
                    L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK );
        return hr;
    }

	// Create the vertex shader
	hr = g_pd3dDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader );
	if( FAILED( hr ) )
	{	
		pVSBlob->Release();
        return hr;
	}

    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE( layout );

    // Create the input layout
	hr = g_pd3dDevice->CreateInputLayout( layout, numElements, pVSBlob->GetBufferPointer(),
                                          pVSBlob->GetBufferSize(), &g_pVertexLayout );
	pVSBlob->Release();
	if( FAILED( hr ) )
        return hr;

    // Set the input layout
    g_pImmediateContext->IASetInputLayout( g_pVertexLayout );

	// Compile the pixel shader
	ID3DBlob* pPSBlob = nullptr;
    hr = CompileShaderFromFile( L"fx.fx", "PS", "ps_4_0", &pPSBlob );
    if( FAILED( hr ) )
    {
        MessageBox( nullptr,
                    L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK );
        return hr;
    }

	// Create the pixel shader
	hr = g_pd3dDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader );
	pPSBlob->Release();
    if( FAILED( hr ) )
        return hr;

    // Create vertex buffer
    SimpleVertex vertices[] =
    {
        { XMFLOAT3( -1.0f, -1.0f, 1.0f )},
        { XMFLOAT3( -1.0f, 1.0f, 1.0f )},
        { XMFLOAT3( 1.0f, 1.0f, 1.0f )},
        { XMFLOAT3( 1.0f, -1.0f, 1.0f )},
    };
    D3D11_BUFFER_DESC bd;
	ZeroMemory( &bd, sizeof(bd) );
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( SimpleVertex ) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory( &InitData, sizeof(InitData) );
    InitData.pSysMem = vertices;
    hr = g_pd3dDevice->CreateBuffer( &bd, &InitData, &g_pVertexBuffer );
    if( FAILED( hr ) )
        return hr;

    // Set vertex buffer
    UINT stride = sizeof( SimpleVertex );
    UINT offset = 0;
    g_pImmediateContext->IASetVertexBuffers( 0, 1, &g_pVertexBuffer, &stride, &offset );

    // Create index buffer
    WORD indices[] =
    {
       3,1,0,
	   1,3,2,
    };
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( WORD ) * 6;        // 36 vertices needed for 12 triangles in a triangle list
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
    InitData.pSysMem = indices;
    hr = g_pd3dDevice->CreateBuffer( &bd, &InitData, &g_pIndexBuffer );
    if( FAILED( hr ) )
        return hr;

    // Set index buffer
    g_pImmediateContext->IASetIndexBuffer( g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0 );

    // Set primitive topology
    g_pImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

	// Create the constant buffer
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(ConstantBuffer);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
    hr = g_pd3dDevice->CreateBuffer( &bd, nullptr, &g_pConstantBuffer );
    if( FAILED( hr ) )
        return hr;

    // Initialize the world matrix
	g_World = XMMatrixIdentity();

    // Initialize the view matrix
	XMVECTOR Eye = XMVectorSet( 0.0f, 0.0f, -5.0f, 0.0f );
	XMVECTOR At = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
	XMVECTOR Up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
	g_View = XMMatrixLookAtLH( Eye, At, Up );

    // Initialize the projection matrix
	g_Projection = XMMatrixPerspectiveFovLH( XM_PIDIV2, width / (FLOAT)height, 0.01f, 100.0f );

	// Initialize Gaussian centre
	g_GaussCentre = XMFLOAT2(width/2,height/2);

	
	
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
    if( g_pImmediateContext ) g_pImmediateContext->ClearState();

    if( g_pConstantBuffer ) g_pConstantBuffer->Release();
    if( g_pVertexBuffer ) g_pVertexBuffer->Release();
    if( g_pIndexBuffer ) g_pIndexBuffer->Release();
    if( g_pVertexLayout ) g_pVertexLayout->Release();
    if( g_pVertexShader ) g_pVertexShader->Release();
    if( g_pPixelShader ) g_pPixelShader->Release();
    if( g_pRenderTargetView ) g_pRenderTargetView->Release();
    if( g_pSwapChain1 ) g_pSwapChain1->Release();
    if( g_pSwapChain ) g_pSwapChain->Release();
    if( g_pImmediateContext1 ) g_pImmediateContext1->Release();
    if( g_pImmediateContext ) g_pImmediateContext->Release();
    if( g_pd3dDevice1 ) g_pd3dDevice1->Release();
    if( g_pd3dDevice ) g_pd3dDevice->Release();
}


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch( message )
    {
    case WM_PAINT:
        hdc = BeginPaint( hWnd, &ps );
        EndPaint( hWnd, &ps );
        break;

    case WM_DESTROY:
        PostQuitMessage( 0 );
        break;

        // Note that this tutorial does not handle resizing (WM_SIZE) requests,
        // so we created the window without the resize border.

    default:
        return DefWindowProc( hWnd, message, wParam, lParam );
    }

    return 0;
}


//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
       // Update our time
    static float t = 0.0f;
    if( g_driverType == D3D_DRIVER_TYPE_REFERENCE )
    {
        t += ( float )XM_PI * 0.0125f;
    }
    else
    {
        static ULONGLONG timeStart = 0;
        ULONGLONG timeCur = GetTickCount64();
        if( timeStart == 0 )
            timeStart = timeCur;
        t = ( timeCur - timeStart ) / 1000.0f;
    }


	

    //
    // Clear the back buffer
    //
    g_pImmediateContext->ClearRenderTargetView( g_pRenderTargetView, Colors::MidnightBlue );

    //
    // Update variables
    //
    ConstantBuffer cb;
	cb.mWorld = XMMatrixTranspose( g_World );
	cb.mView = XMMatrixTranspose( g_View );
	cb.mProjection = XMMatrixTranspose( g_Projection );
	cb.fCentre = g_GaussCentre;
	g_pImmediateContext->UpdateSubresource( g_pConstantBuffer, 0, nullptr, &cb, 0, 0 );

    //
    // Renders a triangle
    //
	g_pImmediateContext->VSSetShader( g_pVertexShader, nullptr, 0 );
	g_pImmediateContext->PSSetConstantBuffers( 0, 1, &g_pConstantBuffer );
	g_pImmediateContext->PSSetShader( g_pPixelShader, nullptr, 0 );
	g_pImmediateContext->DrawIndexed( 6, 0, 0 );        // 36 vertices needed for 12 triangles in a triangle list

    //
    // Present our back buffer to our front buffer
    //
    g_pSwapChain->Present( 0, 0 );
}


HRESULT KinectConnected()
{
	INuiSensor* sensor;
	HRESULT hr;

	int sensorCount = 0;
	hr = NuiGetSensorCount(&sensorCount);
	if (FAILED(hr))
	{
		return hr;
	}

	// Look at each kinect sensor

	for (int i = 0; i < sensorCount; i++)
	{
		// Create the sensor so we can check the status , if we cant create it , move on to next
		hr = NuiCreateSensorByIndex(i,&sensor);
		if (FAILED(hr))
		{
			continue;
		}

		// Get the status of the sensor , and if connected, then we can intialize it
		hr = sensor->NuiStatus();
		if (S_OK == hr)
		{
			pNuiSensor = sensor;
			break;
		}

		// This sensor wasnt ok , so release it since were not using it
		sensor->Release();
	}

	if (NULL != pNuiSensor)
	{
		//Initialize the kinect and specify that well be using depth
		hr = pNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_DEPTH);
		if( SUCCEEDED(hr))
		{
			// Create an event that will be signaled when depth data is available
			m_hNextDepthFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		

			// Open the depth image stream to recieve depth frames
			hr = pNuiSensor->NuiImageStreamOpen(
				 NUI_IMAGE_TYPE_DEPTH,
                NUI_IMAGE_RESOLUTION_640x480,
                0,
                2,
                m_hNextDepthFrameEvent,
                &m_pDepthStreamHandle);
		}

		//Initialize the kinect and specify that well be using colour
		hr = pNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_COLOR);
		if (SUCCEEDED(hr))
		{
			// Create an event that will be signalled when colour data is available 
			m_hNextColorFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

			// Open the colour image stream to recieve colour frames
			hr = pNuiSensor->NuiImageStreamOpen(
				 NUI_IMAGE_TYPE_COLOR,
                NUI_IMAGE_RESOLUTION_640x480,
                0,
                2,
				m_hNextColorFrameEvent,
				&m_pColorStreamHandle);
		}
	}

	if (NULL == pNuiSensor || FAILED(hr))
	{
		// Display message here
		return E_FAIL;
	}
	return hr;
}

void DepthProcessing(float depthDataArray[])
{
	HRESULT hr;

	// Attempt to get the depth frame 
	hr = pNuiSensor->NuiImageStreamGetNextFrame(m_pDepthStreamHandle, 0, &imageFrame);
	if (FAILED(hr))
	{
		return;
	}

	INuiFrameTexture* frameTexture;

	// Get the depth image pixel texture 
	hr = pNuiSensor->NuiImageFrameGetDepthImagePixelFrameTexture(
		m_pDepthStreamHandle, &imageFrame, nullptr, &frameTexture);

	if(FAILED(hr))
	{
		// Do something if display failed 
	}

	NUI_LOCKED_RECT lockedRect;

	// Lock the frame data so the Kinect knows not to modify it while we're reading it
	frameTexture->LockRect(0,&lockedRect,NULL, 0);

	// Make sure we've recieved valid data
	if (lockedRect.Pitch !=0)
	{
		// Get the min and max relable depth for the current frame
		int minDepth = ( NUI_IMAGE_DEPTH_MINIMUM) >> NUI_IMAGE_PLAYER_INDEX_SHIFT;
		int maxDepth = ( NUI_IMAGE_DEPTH_MAXIMUM) >> NUI_IMAGE_PLAYER_INDEX_SHIFT;

		
		const USHORT* curr = (const USHORT*) lockedRect.pBits;
		float *fDest = depthDataArray;
		long* depthToRGB = (long*) depthToRgbMap;
		for (int j = 0; j < height; j++) {
			for(int i = 0; i < width; i++) {
				// Get the depth of pixle in millimetres
				USHORT depth = NuiDepthPixelToDepth(*curr);
				//Store the coordinates of the point corresponding to this pixel
				Vector4 pos = NuiTransformDepthImageToSkeleton(i,j,*curr);
				*fDest++ = pos.x/pos.w;
				*fDest++ = pos.y/pos.w;
				*fDest++ = pos.z/pos.w;
				// Store the index into the colour array corresponding to this pixel
				NuiImageGetColorPixelCoordinatesFromDepthPixelAtResolution(
					NUI_IMAGE_RESOLUTION_80x60, // colour frame resolution
					NUI_IMAGE_RESOLUTION_80x60, // depth frame resolution
					NULL,						
					i, j, *curr,				// Column, row and depth in the depth image
					depthToRGB, depthToRGB +1);  // Ouput: column and row (x,y) in the colour image
	
				depthToRGB += 2;
				*curr++;
			}
		}
	}
    // We're done with the texture so unlock it
    frameTexture->UnlockRect(0);

	frameTexture->Release();

    // Release the frame
    pNuiSensor->NuiImageStreamReleaseFrame(m_pDepthStreamHandle, &imageFrame);
	
}

void ColourProcessing(float colourDataArray[])
{
	HRESULT hr;
	NUI_IMAGE_FRAME imageFrame;

	
	// Attempt to get the colour frame
	hr = pNuiSensor->NuiImageStreamGetNextFrame(m_pColorStreamHandle, 0, &imageFrame);
	if (FAILED(hr))
	{
		return;
	}

	INuiFrameTexture* pTexture = imageFrame.pFrameTexture;
	NUI_LOCKED_RECT lockedRect;

	// Lock the frame data so the kinect knows not to modify it while we're reading it
	pTexture->LockRect(0, &lockedRect, NULL, 0);

	// Make sure we've recieved valid data
	if (lockedRect.Pitch !=0)
	{
		const BYTE* start = (const BYTE*) lockedRect.pBits;
		// Do some thing with color data
		float *fDest = colourDataArray;
		long* depthToRGB = (long*) depthToRgbMap;
		for (int j = 0; j < height; ++j){
			for (int i = 0 ; i < width; ++i){
				// Determine the colour pixel for depth pixel i,j
				long x = *depthToRGB++;
				long y = *depthToRGB++;
				// If out of bounds, then do not colour this pixel
				if ( x < 0 || y < 0 || x > width || y > height) {
					for (int n = 0; n > 3; ++n) *fDest++ = 0.f;
				}
				else{
					// Determine rgb colour for depth pixel (i,j) from colour pixel (x,y)
					const BYTE* colour = start + (x+800*y)*4;
					for ( int n = 0; n < 3; ++n) *fDest++ = colour[2-n]/255.f;
				}
			}
		}

	}

	// We're done with the texture so unlock it
	pTexture->UnlockRect(0);

	// Release the frame
	pNuiSensor->NuiImageStreamReleaseFrame(m_pColorStreamHandle, &imageFrame);
}

int OpenCV(int argc, char** argv)
{
	// This was a tutorial from the opencv 
	// Used to detect colour and track an object
	// Idea was to get the kienct to use this to detect a colour
	// and then find the depth of that point on the image
	http://opencv-srf.blogspot.co.uk/2010/09/object-detection-using-color-seperation.html
	VideoCapture cap(0); // capture the video from webcam

	if (!cap.isOpened() ) // If not success exit program
	{
		cout << "Cannot Open the webcam" << endl;
		return -1;
	}

	namedWindow("Control", CV_WINDOW_AUTOSIZE); // Create a window called "Control"

	int iLowH = 170;
	int iHighH = 179;

	int iLowS = 150;
	int iHighS = 255;

	int iLowV = 60;
	int iHighV = 255;

	// Create the trackbars in "control window
	createTrackbar("LowH", "Control", &iLowH, 179); // HUE (0 - 179)
	createTrackbar("HighH", "Control", &iHighH, 179);

	createTrackbar("LowS", "Control", &iLowS, 255); // Saturation (0-255)
	createTrackbar("HighS", "Control",&iHighS, 255);

	createTrackbar("LowV", "Control", &iLowV, 255); // Value (0 -255)
	createTrackbar("HighV", "Control", &iHighV, 255);

	int iLastX = -1;
	int iLastY = -1;


	// Capture a temporary image from the camera
	Mat imgTmp;
	cap.read(imgTmp);

	// Create a black image with the size as the camera output
	Mat imgLines = Mat::zeros( imgTmp.size(), CV_8UC3);;

	while (true)
	{
		Mat imgOriginal;

		bool bSuccess = cap.read(imgOriginal); // read a new frame from video

		if (!bSuccess) // if not success, break loop
		{
			cout << "Cannot read a frame fomr video stream" << endl;
			break;
		}

		Mat imgHSV;

		cvtColor(imgOriginal, imgHSV, COLOR_BGR2HSV); // Convert the captured frame from BGR to HSV

		Mat imgThresholded;

		inRange(imgHSV, Scalar(iLowH, iLowS, iLowV), Scalar(iHighH, iHighS, iHighV), imgThresholded); // Threshold the image

		// Morphological opening ( removes small objects from the foreground)
		erode(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5,5)));
		dilate(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5,5)));

		// Morphological closing ( removes small holes from the foreground)
		dilate(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5,5)));
		erode(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5,5)));

		// Calculate the moments of the thresholded image
		Moments oMoments = moments(imgThresholded);

		double dM01 = oMoments.m01;
		double dM10 = oMoments.m10;
		double dArea = oMoments.m00;

		// if the area <= 10000, I consider that there are no ojects in the image and its because of the noise , the area is not zero
		if (dArea > 1000)
		{
			// Calculate the position of the ball 
			int posX = dM10/ dArea;
			int posY = dM01/ dArea;

			if (iLastX >= 0 && iLastY >= 0 && posX >= 0 && posY >= 0)
			{
				// Draw a red line form the previos point to the current point
				line(imgLines, Point(posX, posY), Point(iLastX, iLastY), Scalar(0,0,255),2);
			}

			iLastX = posX;
			iLastY = posY;
		}

		imshow("Thresholded Image", imgThresholded); // show the thresholded image

		imgOriginal = imgOriginal + imgLines;
		imshow("Original", imgOriginal); // show the original image

		if (waitKey(30) == 27) //wait for esc key press for 30ms . if esc key is press , break loop
		{
			cout << "esc key is pressed by user" << endl;
			break;
		}
	}
	return 0;
}





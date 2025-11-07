#include "targetver.h"
#include <windows.h>
#include <initguid.h>

#include <d3d11.h>
#pragma comment (lib, "D3D11")
#include <d2d1.h>
#pragma comment (lib, "D2d1")
#include <d2d1_1.h>
#include <d2d1_3.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h> // IDXGISwapChain2

#include <d2d1effects_2.h>

#include <dwrite.h>
#pragma comment(lib,"Dwrite")

#include <math.h> // ceil

#include <wincodec.h>
#pragma comment(lib,"windowscodecs")

#include <Urlmon.h> // URLDownloadToCacheFile
#pragma comment (lib, "Urlmon")

template <class T> void SafeRelease(T** ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

class CScrollingText
{
private:
	ID2D1Factory1* m_pD2DFactory1 = nullptr;
	IDWriteFactory* m_pDWriteFactory = nullptr;
	IWICImagingFactory* m_pWICImagingFactory = nullptr;

	WCHAR m_sText[1024] = L"";
	D2D1::ColorF m_TextColor = D2D1::ColorF::Black;
	D2D1::ColorF m_BackColor = D2D1::ColorF::White;
	ID2D1SolidColorBrush* m_pMainBrush = nullptr;
	D2D1::ColorF m_pGradientColor1 = D2D1::ColorF::White;
	D2D1::ColorF m_pGradientColor2 = D2D1::ColorF::White;
	ID2D1LinearGradientBrush* m_pLinearGradientBrush = nullptr;
	ID2D1BitmapBrush1* m_pBackBitmapBrush = nullptr;

	void* m_pD3D11DevicePtr = NULL;
	ID3D11Device* m_pD3D11Device= NULL;
	ID3D11DeviceContext* m_pD3D11DeviceContext = NULL;

	IDXGIDevice1* m_pDXGIDevice = nullptr;
	ID2D1Bitmap1* m_pD2DTargetBitmap = nullptr;
	IDXGISwapChain1* m_pDXGISwapChain1 = nullptr;

	ID2D1Device* m_pD2DDevice = nullptr;
	ID2D1DeviceContext* m_pD2DContext = nullptr;

	IDWriteTextFormat* m_pTextFormat = nullptr;
	IDWriteTextLayout* m_pTextLayout = nullptr;

	//ID2D1Image* m_pShadowImage = nullptr;
	ID2D1Effect* m_pShadowEffect = nullptr;

	int m_bOrientation = 0;
	float m_nX = 0.0F;
	float m_nY = 0.0F;
	WCHAR m_sFont[32] = L"Arial";
	int m_nFontsize = 0;
	bool m_bBold = false;
	bool m_bItalic = false;
	bool m_bShadow = false;
	bool m_bFade = false;
	float m_nSpeed = 1.0F;
	int m_nWidth = 500;
	int m_nHeight = 500;

	HANDLE m_hFrameLatencyWaitable = NULL;
	bool m_bRunning = false;

	LPCTSTR lpszClassName = L"ScrollingText";

	static LRESULT CALLBACK WndProcStatic(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	HWND m_hStatic;

	HRESULT CreateD3D11Device();
	HRESULT CreateDeviceResources();
	HRESULT CreateFormatAndLayout();
	HRESULT CreateSwapChain(HWND hWnd);
	HRESULT ConfigureSwapChain();
	HRESULT CreateD2DBitmapFromFile(LPCWSTR szFileName, ID2D1Bitmap** pD2DBitmap);
	HRESULT CreateD2DBitmapFromURL(LPCWSTR wsURL, ID2D1Bitmap** pD2DBitmap);

	static HRESULT OnPaintProc(HWND hWnd);

public:
	CScrollingText(HINSTANCE hInstance, HWND hWndParent, int nX, int nY, int nWidth, int nHeight);
	void Initialize(ID2D1Factory1* pD2DFactory1, IDWriteFactory* pDWriteFactory, IWICImagingFactory* pWICImagingFactory, LPCWSTR sText,
		LPCWSTR sFont, int nFontSize, int nWidth, int nHeight, bool bOrientation = 0, bool bBold = false, bool bItalic = false, bool bShadow = false, bool bFade = false);
	HRESULT SetGradientBackground(D2D1::ColorF pGradientColor1, D2D1::ColorF pGradientColor2);
	HRESULT SetBitmapBackgroundFromURL(LPCWSTR wsURL);
	
	D2D1::ColorF GetTextColor()
	{
		return m_TextColor;
	}

	void SetTextColor(D2D1::ColorF value)
	{
		m_TextColor = value;
		SafeRelease(&m_pMainBrush);
		if (m_pD2DContext != nullptr)
		{
			m_pD2DContext->CreateSolidColorBrush(D2D1::ColorF(m_TextColor), &m_pMainBrush);
		}
	}

	D2D1::ColorF GetBackgroundColor()
	{
		return m_BackColor;
	}

	void SetBackgroundColor(D2D1::ColorF value)
	{
		m_BackColor = value;
	}

	float GetSpeed()
	{
		return m_nSpeed;
	}
	virtual void SetSpeed(float value)
	{
		m_nSpeed = value;
	}

public:
	~CScrollingText()
	{
		KillTimer(m_hStatic, 1);
		SafeRelease(&m_pTextFormat);
		SafeRelease(&m_pTextLayout);

		SafeRelease(&m_pMainBrush);
		SafeRelease(&m_pLinearGradientBrush);
		SafeRelease(&m_pBackBitmapBrush);

		//SafeRelease(&m_pShadowImage);
		SafeRelease(&m_pShadowEffect);
		
		if (m_pD2DContext)
			m_pD2DContext->SetTarget(NULL);
		SafeRelease(&m_pD2DTargetBitmap);		
		SafeRelease(&m_pD2DContext);				

		SafeRelease(&m_pDXGISwapChain1);
		SafeRelease(&m_pDXGIDevice);

		SafeRelease(&m_pD3D11Device);	
		/*SafeRelease(&m_pDWriteFactory);
		SafeRelease(&m_pD2DFactory1);*/
		//SafeRelease(&m_pWICImagingFactory);
	}
};
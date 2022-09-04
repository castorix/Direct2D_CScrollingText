#include "CScrollingText.h"

static WNDPROC m_OldWndProc;

CScrollingText::CScrollingText(HINSTANCE hInstance, HWND hWndParent, int nX, int nY, int nWidth, int nHeight)
{
	WNDCLASS wc;
	GetClassInfo(hInstance, L"STATIC", &wc);
	wc.hInstance = hInstance;
	wc.lpszClassName = lpszClassName;
	m_OldWndProc = wc.lpfnWndProc;
	wc.lpfnWndProc = WndProcStatic;

	if (!RegisterClass(&wc))
	{
		if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
			MessageBox(NULL, L"Registering Error", L"Error", MB_OK | MB_ICONSTOP);
	}
	m_hStatic = CreateWindowEx(0, lpszClassName, L"", WS_CHILD | WS_VISIBLE | WS_BORDER, nX, nY, nWidth, nHeight, hWndParent, (HMENU)100, hInstance, this);
}

void CScrollingText::Initialize(ID2D1Factory1* pD2DFactory1, IDWriteFactory* pDWriteFactory, IWICImagingFactory* pWICImagingFactory, LPCWSTR sText,
	LPCWSTR sFont, int nFontSize, int nWidth, int nHeight, bool bOrientation, bool bBold, bool bItalic, bool bShadow, bool bFade)
{
	HRESULT hr = S_OK;
	m_pD2DFactory1 = pD2DFactory1;
	m_pDWriteFactory = pDWriteFactory;
	m_pWICImagingFactory = pWICImagingFactory;
	lstrcpy(m_sText, sText);
	lstrcpy(m_sFont, sFont);
	m_nFontsize = nFontSize;
	m_nWidth = nWidth;
	m_nHeight = nHeight;
	m_bOrientation = bOrientation;
	m_bBold = bBold;
	m_bItalic = bItalic;
	m_bShadow = bShadow;
	m_bFade = bFade;
	hr = CreateD3D11Device();
	hr = CreateDeviceResources();
	hr = CreateSwapChain(m_hStatic);
	if (SUCCEEDED(hr))
		hr = ConfigureSwapChain();
	SetTimer(m_hStatic, 1, 15, NULL);
	RECT rc;
	GetClientRect(m_hStatic, &rc);
	m_nX = rc.right - rc.left;
	m_nY = rc.bottom - rc.top;
}

LRESULT CALLBACK CScrollingText::WndProcStatic(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
	{
		//SetTimer(hWnd, 1, 15, NULL);
		return 0;
	}
	case WM_NCCREATE:
	{
		CREATESTRUCT* pCS = (CREATESTRUCT*)lParam;
		CScrollingText* pST = (CScrollingText*)pCS->lpCreateParams;
		//SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)pCS->lpCreateParams);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)pST);
		return 1;
	}
	case WM_PAINT:
	{
		return OnPaintProc(hWnd);
	}
	break;
	case WM_TIMER:
	{
		RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE);
	}
	break;
	case WM_DESTROY:
	{
		CScrollingText* pST = (CScrollingText*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if (pST)
			delete(pST);
		return 0;
	}
	break;
	default:
		return(CallWindowProc(m_OldWndProc, hWnd, uMsg, wParam, lParam));
	}
	return 0;
}

HRESULT CScrollingText::OnPaintProc(HWND hWnd)
{
	HRESULT hr = S_OK;

	CScrollingText* pST = (CScrollingText*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hWnd, &ps);
	if (pST)
	{
		if (pST->m_pD2DContext)
		{
			pST->m_pD2DContext->BeginDraw();

			D2D1_SIZE_F size = pST->m_pD2DContext->GetSize();
			pST->m_pD2DContext->Clear(D2D1::ColorF(pST->m_BackColor));
			pST->m_pD2DContext->SetTransform(D2D1::Matrix3x2F::Identity());

			D2D1_RECT_F rect{ 0.0f, 0.0f, size.width, size.height };
			if (pST->m_pLinearGradientBrush != nullptr)
			{
				pST->m_pD2DContext->FillRectangle(rect, pST->m_pLinearGradientBrush);
			}
			else if (pST->m_pBackBitmapBrush != nullptr)
			{
				pST->m_pD2DContext->FillRectangle(rect, pST->m_pBackBitmapBrush);
			}
			// Adapted from https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/multimedia/Direct2D/TextAnimationSample/TextAnimationSample.cpp
			DWRITE_TEXT_METRICS textMetrics = { 0 };
			if (pST->m_pTextLayout)
			{
				pST->m_pTextLayout->GetMetrics(&textMetrics);
				DWRITE_OVERHANG_METRICS overhangMetrics;
				pST->m_pTextLayout->GetOverhangMetrics(&overhangMetrics);
				UINT nDPI = GetDpiForWindow(hWnd);
				D2D1_SIZE_F padding = D2D1::SizeF(96.0f / nDPI, 96.0f / nDPI);
				D2D1_POINT_2F overhangOffset = D2D1::Point2F(ceil(overhangMetrics.left + padding.width), ceil(overhangMetrics.top + padding.height));
				D2D1_SIZE_F maskSize = D2D1::SizeF(
					overhangMetrics.right + padding.width + overhangOffset.x + pST->m_pTextLayout->GetMaxWidth(),
					overhangMetrics.bottom + padding.height + overhangOffset.y + pST->m_pTextLayout->GetMaxHeight()
				);
				D2D1_SIZE_U maskPixelSize = D2D1::SizeU(
					static_cast<UINT>(ceil(maskSize.width * nDPI / 96.0f)),
					static_cast<UINT>(ceil(maskSize.height * nDPI / 96.0f))
				);

				D2D1_MATRIX_3X2_F pTransform;
				D2D1_POINT_2F pt;
				if (pST->m_bOrientation == 0)
				{
					float nY = (size.height - textMetrics.height + overhangOffset.y) / 2.0F;
					if (maskPixelSize.height > textMetrics.height)
					{
						nY += (overhangOffset.y) / 2.0F;
					}
					pTransform = D2D1::Matrix3x2F::Translation(pST->m_nX, 1);
					pt = D2D1::Point2F(overhangOffset.x, nY);
				}
				else
				{
					float nX = (size.width - textMetrics.width) / 2 + overhangOffset.x;
					pTransform = D2D1::Matrix3x2F::Translation(1, pST->m_nY);
					pt = D2D1::Point2F(nX, overhangOffset.y);
				}
				pST->m_pD2DContext->SetTransform(pTransform);

				if (pST->m_bShadow)
				{
					ID2D1BitmapRenderTarget* pCompatibleRenderTarget = nullptr;
					D2D1_SIZE_U sizeU = D2D1::SizeU(size.width, size.height);
					D2D1_PIXEL_FORMAT pf{ DXGI_FORMAT::DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE::D2D1_ALPHA_MODE_PREMULTIPLIED };
					hr = pST->m_pD2DContext->CreateCompatibleRenderTarget(size, sizeU, pf, D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS::D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE, &pCompatibleRenderTarget);
					if (SUCCEEDED(hr))
					{
						pCompatibleRenderTarget->BeginDraw();
						pCompatibleRenderTarget->Clear(nullptr);
						pCompatibleRenderTarget->DrawTextLayout(pt, pST->m_pTextLayout, pST->m_pMainBrush, D2D1_DRAW_TEXT_OPTIONS::D2D1_DRAW_TEXT_OPTIONS_NO_SNAP | D2D1_DRAW_TEXT_OPTIONS::D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
						hr = pCompatibleRenderTarget->EndDraw();
						ID2D1Bitmap* pCompatibleBitmap = nullptr;
						hr = pCompatibleRenderTarget->GetBitmap(&pCompatibleBitmap);

						ID2D1Effect* shadowEffect = nullptr;
						hr = pST->m_pD2DContext->CreateEffect(CLSID_D2D1Shadow, &shadowEffect);
						shadowEffect->SetInput(0, pCompatibleBitmap);

						D2D1_RECT_F rectBackground{ 0.0f, 0.0f, size.width, size.height };
						D2D1_SIZE_F bmpSizeBackground = pCompatibleBitmap->GetSize();
						D2D1_RECT_F sourceRectangleBackground{ 0.0f, 0.0f, bmpSizeBackground.width, bmpSizeBackground.height };

						D2D1_POINT_2F ptShadow = D2D1::Point2F(3, 3);
						D2D1_RECT_F sourceRectangle{ 0, 0, size.width, size.height };
						pST->m_pD2DContext->DrawImage(shadowEffect, ptShadow, sourceRectangle, D2D1_INTERPOLATION_MODE::D2D1_INTERPOLATION_MODE_LINEAR, D2D1_COMPOSITE_MODE::D2D1_COMPOSITE_MODE_SOURCE_OVER);
						SafeRelease(&shadowEffect);
						SafeRelease(&pCompatibleBitmap);
						SafeRelease(&pCompatibleRenderTarget);
					}
				}

				pST->m_pD2DContext->DrawTextLayout(pt, pST->m_pTextLayout, pST->m_pMainBrush, D2D1_DRAW_TEXT_OPTIONS::D2D1_DRAW_TEXT_OPTIONS_NO_SNAP | D2D1_DRAW_TEXT_OPTIONS::D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);

				if (pST->m_bFade)
				{
					pST->m_pD2DContext->SetTransform(D2D1::Matrix3x2F::Identity());
					ID2D1LinearGradientBrush* pLinearGradientBrush = nullptr;

					D2D1_GRADIENT_STOP gs[2];
					gs[0].color = D2D1::ColorF(pST->m_pGradientColor1);
					gs[0].position = 0.0F;
					auto c2 = new D2D1::ColorF(pST->m_pGradientColor1);
					gs[1].color = D2D1::ColorF(c2->r, c2->g, c2->b, 0.0F);
					gs[1].position = 1.0F;

					ID2D1GradientStopCollection* pGSC = nullptr;
					hr = pST->m_pD2DContext->CreateGradientStopCollection(gs, 2, D2D1_GAMMA::D2D1_GAMMA_2_2, D2D1_EXTEND_MODE::D2D1_EXTEND_MODE_CLAMP, &pGSC);
					if (SUCCEEDED(hr))
					{
						D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props = D2D1::LinearGradientBrushProperties(
							D2D1::Point2F(0, 0),
							D2D1::Point2F(size.width, size.height)
						);
						if (pST->m_bOrientation == 0)
						{
							props.startPoint = D2D1::Point2F(0, 0);
							props.endPoint = D2D1::Point2F(size.width / 5, 0);
							hr = pST->m_pD2DContext->CreateLinearGradientBrush(props, pGSC, &pLinearGradientBrush);
							if (SUCCEEDED(hr))
							{
								D2D1_RECT_F rect{ 0.0f, 0.0f, size.width, size.height };
								pST->m_pD2DContext->FillRectangle(rect, pLinearGradientBrush);
								SafeRelease(&pLinearGradientBrush);
							}
						}
						else
						{
							props.startPoint = D2D1::Point2F(0, 0);
							props.endPoint = D2D1::Point2F(0, size.height / 5);
							hr = pST->m_pD2DContext->CreateLinearGradientBrush(props, pGSC, &pLinearGradientBrush);
							if (SUCCEEDED(hr))
							{
								D2D1_RECT_F rect{ 0, 0, size.width, size.height };
								pST->m_pD2DContext->FillRectangle(rect, pLinearGradientBrush);
								SafeRelease(&pLinearGradientBrush);
							}
						}
						SafeRelease(&pGSC);
					}

					auto c1 = new D2D1::ColorF(pST->m_pGradientColor2);
					gs[0].color = D2D1::ColorF(c1->r, c1->g, c1->b, 0.0F);
					gs[0].position = 0.0F;
					gs[1].color = D2D1::ColorF(pST->m_pGradientColor2);
					gs[1].position = 1.0F;
					hr = pST->m_pD2DContext->CreateGradientStopCollection(gs, 2, D2D1_GAMMA::D2D1_GAMMA_2_2, D2D1_EXTEND_MODE::D2D1_EXTEND_MODE_CLAMP, &pGSC);
					if (SUCCEEDED(hr))
					{
						D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props = D2D1::LinearGradientBrushProperties(
							D2D1::Point2F(0, 0),
							D2D1::Point2F(size.width, size.height)
						);

						if (pST->m_bOrientation == 0)
						{
							props.startPoint = D2D1::Point2F(size.width - size.width / 5, 0);
							props.endPoint = D2D1::Point2F(size.width, 0);
							hr = pST->m_pD2DContext->CreateLinearGradientBrush(props, pGSC, &pLinearGradientBrush);
							if (SUCCEEDED(hr))
							{
								D2D1_RECT_F rect{ size.width - size.width / 5, 0, size.width, size.height };
								pST->m_pD2DContext->FillRectangle(rect, pLinearGradientBrush);
								SafeRelease(&pLinearGradientBrush);
							}
						}
						else
						{
							props.startPoint = D2D1::Point2F(0, size.height - size.height / 5);
							props.endPoint = D2D1::Point2F(0, size.height);
							hr = pST->m_pD2DContext->CreateLinearGradientBrush(props, pGSC, &pLinearGradientBrush);
							if (SUCCEEDED(hr))
							{
								D2D1_RECT_F rect{ 0, size.height - size.height / 5, size.width, size.height };
								pST->m_pD2DContext->FillRectangle(rect, pLinearGradientBrush);
								SafeRelease(&pLinearGradientBrush);
							}
						}
						SafeRelease(&pGSC);
					}
				}

				if (pST->m_bOrientation == 0)
				{
					pST->m_nX -= pST->m_nSpeed;
					if (pST->m_nX + maskPixelSize.width <= 0)
					{
						pST->m_nX = size.width;
					}
				}
				else
				{
					pST->m_nY -= pST->m_nSpeed;
					if (pST->m_nY + maskPixelSize.height <= 0)
					{
						pST->m_nY = size.height;
					}
				}
			}

			hr = pST->m_pD2DContext->EndDraw();
			if (hr == D2DERR_RECREATE_TARGET)
			{
				pST->m_pD2DContext->SetTarget(NULL);
				SafeRelease(&pST->m_pD2DContext);
				hr = pST->CreateD3D11Device();
				hr = pST->CreateDeviceResources();
				hr = pST->CreateSwapChain(hWnd);
				hr = pST->ConfigureSwapChain();
			}
			hr = pST->m_pDXGISwapChain1->Present(1, 0);
		}
	}
	EndPaint(hWnd, &ps);
	return hr;
}

HRESULT CScrollingText::CreateD3D11Device()
{
	HRESULT hr = S_OK;
	auto creationFlags = static_cast<unsigned int>(D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_BGRA_SUPPORT);
	creationFlags |= static_cast<unsigned int>(D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_DEBUG);

	D3D_FEATURE_LEVEL featureLevels[] = {
	D3D_FEATURE_LEVEL_11_1,
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
	D3D_FEATURE_LEVEL_9_3,
	D3D_FEATURE_LEVEL_9_2,
	D3D_FEATURE_LEVEL_9_1
	};
	D3D_FEATURE_LEVEL* featureLevel = nullptr;
	hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE::D3D_DRIVER_TYPE_HARDWARE, 0, creationFlags,
		featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, &m_pD3D11Device, featureLevel, &m_pD3D11DeviceContext);
	if (SUCCEEDED(hr))
	{
		hr = m_pD3D11Device->QueryInterface(IID_PPV_ARGS(&m_pDXGIDevice));
		hr = m_pD2DFactory1->CreateDevice(m_pDXGIDevice, &m_pD2DDevice);
		if (SUCCEEDED(hr))
		{
			m_pD2DContext = nullptr;
			hr = m_pD2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS::D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_pD2DContext);
			m_pD2DContext->SetAntialiasMode(D2D1_ANTIALIAS_MODE::D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
			m_pD2DContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE::D2D1_TEXT_ANTIALIAS_MODE_DEFAULT);
			SafeRelease(&m_pD2DDevice);
		}
		SafeRelease(&m_pD3D11DeviceContext);
	}
	return hr;
}

HRESULT CScrollingText::CreateDeviceResources()
{
	HRESULT hr = S_OK;
	if (m_pMainBrush == nullptr)
	{
		hr = m_pD2DContext->CreateSolidColorBrush(D2D1::ColorF(m_TextColor), &m_pMainBrush);
	}
	hr = CreateFormatAndLayout();
	return hr;
}

HRESULT CScrollingText::CreateFormatAndLayout()
{
	HRESULT hr = S_OK;
	if (m_pTextFormat == nullptr)
	{
		DWRITE_FONT_WEIGHT weight;
		if (m_bBold)
		{
			weight = DWRITE_FONT_WEIGHT::DWRITE_FONT_WEIGHT_BOLD;
		}
		else
		{
			weight = DWRITE_FONT_WEIGHT::DWRITE_FONT_WEIGHT_NORMAL;
		}
		DWRITE_FONT_STYLE style;
		if (m_bItalic)
		{
			style = DWRITE_FONT_STYLE::DWRITE_FONT_STYLE_ITALIC;
		}
		else
		{
			style = DWRITE_FONT_STYLE::DWRITE_FONT_STYLE_NORMAL;
		}
		hr = m_pDWriteFactory->CreateTextFormat(m_sFont, nullptr, weight, style, DWRITE_FONT_STRETCH::DWRITE_FONT_STRETCH_NORMAL, m_nFontsize, L"", &m_pTextFormat);
		if (SUCCEEDED(hr))
		{
			hr = m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT::DWRITE_TEXT_ALIGNMENT_CENTER);
			hr = m_pDWriteFactory->CreateTextLayout(m_sText, ARRAYSIZE(m_sText), m_pTextFormat, m_nWidth, m_nHeight, &m_pTextLayout);
			if (SUCCEEDED(hr))
			{
				IDWriteTypography* pTypography = nullptr;
				hr = m_pDWriteFactory->CreateTypography(&pTypography);
				if (SUCCEEDED(hr))
				{
					DWRITE_FONT_FEATURE ff =
					{
							DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_7,
							1
					};
					hr = pTypography->AddFontFeature(ff);
					if (SUCCEEDED(hr))
					{
						DWRITE_TEXT_RANGE tr = { 0, ARRAYSIZE(m_sText) };
						m_pTextLayout->SetTypography(pTypography, tr);
					}
					SafeRelease(&pTypography);
				}
			}
		}
	}
	return hr;
}

HRESULT CScrollingText::CreateSwapChain(HWND hWnd)
{
	HRESULT hr = S_OK;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
	//swapChainDesc.Width = 1
	//swapChainDesc.Height = 1
	RECT rc;
	GetClientRect(m_hStatic, &rc);
	swapChainDesc.Width = rc.right - rc.left;
	swapChainDesc.Height = rc.bottom - rc.top;
	swapChainDesc.Format = DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1; // don't use multi-sampling
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2; // use double buffering to enable flip
	swapChainDesc.Scaling = (hWnd != NULL) ? DXGI_SCALING::DXGI_SCALING_NONE : DXGI_SCALING::DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	swapChainDesc.Flags = 0;

	IDXGIAdapter* pDXGIAdapter = nullptr;
	hr = m_pDXGIDevice->GetAdapter(&pDXGIAdapter);
	if (SUCCEEDED(hr))
	{
		IDXGIFactory2* pDXGIFactory2 = nullptr;
		hr = pDXGIAdapter->GetParent(IID_PPV_ARGS(&pDXGIFactory2));
		if (SUCCEEDED(hr))
		{
			if (hWnd != NULL)
			{
				// hr = 0x887a0001
				hr = pDXGIFactory2->CreateSwapChainForHwnd(m_pD3D11Device, hWnd, &swapChainDesc, nullptr, nullptr, &m_pDXGISwapChain1);
			}
			else
			{
				hr = pDXGIFactory2->CreateSwapChainForComposition(m_pD3D11Device, &swapChainDesc, nullptr, &m_pDXGISwapChain1);
			}
			if (SUCCEEDED(hr))
				hr = m_pDXGIDevice->SetMaximumFrameLatency(1);
			SafeRelease(&pDXGIFactory2);
		}
		SafeRelease(&pDXGIAdapter);
	}
	return hr;
}

HRESULT CScrollingText::ConfigureSwapChain()
{
	HRESULT hr = S_OK;

	D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
		D2D1_BITMAP_OPTIONS::D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS::D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
		D2D1::PixelFormat(DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE::D2D1_ALPHA_MODE_IGNORE),
		0,
		0,
		NULL
	);

	unsigned int nDPI = GetDpiForWindow(m_hStatic);
	bitmapProperties.dpiX = nDPI;
	bitmapProperties.dpiY = nDPI;

	IDXGISurface* pDXGISurface;
	hr = m_pDXGISwapChain1->GetBuffer(0, IID_PPV_ARGS(&pDXGISurface));
	if (SUCCEEDED(hr))
	{
		hr = m_pD2DContext->CreateBitmapFromDxgiSurface(pDXGISurface, bitmapProperties, &m_pD2DTargetBitmap);
		if (SUCCEEDED(hr))
		{
			m_pD2DContext->SetTarget(m_pD2DTargetBitmap);
		}
		SafeRelease(&pDXGISurface);
	}
	return hr;
}

HRESULT CScrollingText::SetGradientBackground(D2D1::ColorF pGradientColor1, D2D1::ColorF pGradientColor2)
{
	HRESULT hr = S_OK;
	CScrollingText* pST = (CScrollingText*)GetWindowLongPtr(this->m_hStatic, GWLP_USERDATA);
	if (pST)
	{
		D2D1_GRADIENT_STOP gs[2];
		m_pGradientColor1 = pGradientColor1;
		m_pGradientColor2 = pGradientColor2;
		gs[0].color = D2D1::ColorF(m_pGradientColor1);
		gs[0].position = 0.0F;
		gs[1].color = D2D1::ColorF(m_pGradientColor2);
		gs[1].position = 1.0F;

		ID2D1GradientStopCollection* pGSC = nullptr;
		hr = m_pD2DContext->CreateGradientStopCollection(gs, 2, D2D1_GAMMA::D2D1_GAMMA_2_2, D2D1_EXTEND_MODE::D2D1_EXTEND_MODE_CLAMP, &pGSC);
		if (SUCCEEDED(hr))
		{
			D2D1_SIZE_F size = pST->m_pD2DContext->GetSize();
			D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props = D2D1::LinearGradientBrushProperties(
				D2D1::Point2F(0, 0),
				D2D1::Point2F(size.width, size.height)
			);

			if (m_bOrientation == 0)
			{
				props.startPoint = D2D1::Point2F(0, 0);
				props.endPoint = D2D1::Point2F(size.width, size.height);
			}
			else
			{
				props.startPoint = D2D1::Point2F(0, 0);
				props.endPoint = D2D1::Point2F(0, size.height);
			}

			hr = m_pD2DContext->CreateLinearGradientBrush(props, pGSC, &m_pLinearGradientBrush);
			SafeRelease(&pGSC);
		}
	}
	return hr;
}

HRESULT CScrollingText::SetBitmapBackgroundFromURL(LPCWSTR wsURL)
{
	HRESULT hr = S_OK;
	CScrollingText* pST = (CScrollingText*)GetWindowLongPtr(this->m_hStatic, GWLP_USERDATA);
	if (pST)
	{
		ID2D1Bitmap* pD2DBitmap = nullptr;
		hr = CreateD2DBitmapFromURL(wsURL, &pD2DBitmap);
		D2D1_BITMAP_BRUSH_PROPERTIES1 bbp = D2D1::BitmapBrushProperties1(
			D2D1_EXTEND_MODE::D2D1_EXTEND_MODE_WRAP,
			D2D1_EXTEND_MODE::D2D1_EXTEND_MODE_WRAP,
			D2D1_INTERPOLATION_MODE::D2D1_INTERPOLATION_MODE_LINEAR);
		hr = pST->m_pD2DContext->CreateBitmapBrush(pD2DBitmap, bbp, &m_pBackBitmapBrush);
		SafeRelease(&pD2DBitmap);
	}
	return hr;
}
// https://docs.microsoft.com/en-us/windows/win32/direct2d/how-to-load-a-direct2d-bitmap-from-a-file
HRESULT CScrollingText::CreateD2DBitmapFromFile(LPCWSTR szFileName, ID2D1Bitmap** pD2DBitmap)
{
	HRESULT hr = S_OK;
	CScrollingText* pST = (CScrollingText*)GetWindowLongPtr(this->m_hStatic, GWLP_USERDATA);
	if (pST)
	{
		IWICBitmapDecoder* pDecoder = NULL;
		hr = pST->m_pWICImagingFactory->CreateDecoderFromFilename(szFileName, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &pDecoder);
		if (SUCCEEDED(hr))
		{
			IWICBitmapFrameDecode* pFrame = NULL;
			if (SUCCEEDED(hr))
				hr = pDecoder->GetFrame(0, &pFrame);

			if (SUCCEEDED(hr))
			{
				IWICFormatConverter* pConvertedSourceBitmap = NULL;
				hr = pST->m_pWICImagingFactory->CreateFormatConverter(&pConvertedSourceBitmap);
				if (SUCCEEDED(hr))
				{
					hr = pConvertedSourceBitmap->Initialize(
						pFrame,
						GUID_WICPixelFormat32bppPBGRA,
						WICBitmapDitherTypeNone,
						NULL,
						0.f,
						WICBitmapPaletteTypeCustom
					);
					if (SUCCEEDED(hr))
						hr = pST->m_pD2DContext->CreateBitmapFromWicBitmap(pConvertedSourceBitmap, NULL, &*pD2DBitmap);
					SafeRelease(&pConvertedSourceBitmap);
				}
			}
			SafeRelease(&pDecoder);
			SafeRelease(&pFrame);
		}
	}
	return hr;
}

HRESULT CScrollingText::CreateD2DBitmapFromURL(LPCWSTR wsURL, ID2D1Bitmap** pD2DBitmap)
{
	HRESULT hr = S_OK;
	WCHAR wszFilename[MAX_PATH];
	hr = URLDownloadToCacheFile(NULL, wsURL, wszFilename, ARRAYSIZE(wszFilename), 0x0, NULL);
	if (SUCCEEDED(hr))
	{
		hr = CreateD2DBitmapFromFile(wszFilename, &*pD2DBitmap);
	}
	return hr;
}





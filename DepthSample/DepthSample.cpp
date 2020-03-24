#ifndef STRICT
#define STRICT	// �����ȃR�[�h���^��v������
#endif
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <k4a/k4a.h>

#pragma comment( lib, "k4a.lib" )

#define ENABLE_CSV_OUTPUT		1			// 1=CSV �o�͂�L���ɂ���

// �C���[�W�̉𑜓x
#define RESOLUTION_WIDTH		(640)
#define RESOLUTION_HEIGHT		(576)

// Win32 �A�v���p�̃p�����[�^
static const TCHAR szClassName[] = TEXT("�[�x�T���v��");
HWND g_hWnd = NULL;							// �A�v���P�[�V�����̃E�B���h�E
HBITMAP g_hBMP = NULL, g_hBMPold = NULL;	// �\������r�b�g�}�b�v�̃n���h��
HDC g_hDCBMP = NULL;						// �\������r�b�g�}�b�v�̃R���e�L�X�g
BITMAPINFO g_biBMP = { 0, };				// �r�b�g�}�b�v�̏�� (�𑜓x��t�H�[�}�b�g)
LPDWORD g_pdwPixel = NULL;					// �r�b�g�}�b�v�̒��g�̐擪 (�s�N�Z�����)
UINT16* g_pDepthMap = NULL;					// �[�x�}�b�v�o�b�t�@�̃|�C���^

// Azure Kinect �p�̃p�����[�^
k4a_device_t g_hAzureKinect = nullptr;		// Azure Kinect �̃f�o�C�X�n���h��

// Kinect ������������
k4a_result_t CreateKinect()
{
	k4a_result_t hr;

	// Azure Kinect ������������
	hr = k4a_device_open( K4A_DEVICE_DEFAULT, &g_hAzureKinect );
	if ( hr == K4A_RESULT_SUCCEEDED )
	{
		// Azure Kinect �̃J�����ݒ�
		k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
		config.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
		config.color_resolution = K4A_COLOR_RESOLUTION_OFF;
		config.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
		config.camera_fps = K4A_FRAMES_PER_SECOND_30;
		config.synchronized_images_only = false;
		config.depth_delay_off_color_usec = 0;
		config.wired_sync_mode = K4A_WIRED_SYNC_MODE_STANDALONE;
		config.subordinate_delay_off_master_usec = 0;
		config.disable_streaming_indicator = false;

		// Azure Kinect �̎g�p���J�n����
		hr = k4a_device_start_cameras( g_hAzureKinect, &config );
		if ( hr == K4A_RESULT_SUCCEEDED )
		{
			return hr;
		}
		else
		{
			MessageBox( NULL, TEXT("Azure Kinect ���J�n�ł��܂���ł���"), TEXT("�G���["), MB_OK );
		}
		// Azure Kinect �̎g�p����߂�
		k4a_device_close( g_hAzureKinect );
	}
	else
	{
		MessageBox( NULL, TEXT("Azure Kinect �̏������Ɏ��s - �J�����̏�Ԃ��m�F���Ă�������"), TEXT("�G���["), MB_OK );
	}
	return hr;
}

// Kinect ���I������
void DestroyKinect()
{
	if ( g_hAzureKinect )
	{
		// Azure Kinect ���~����
		k4a_device_stop_cameras( g_hAzureKinect );

		// Azure Kinect �̎g�p����߂�
		k4a_device_close( g_hAzureKinect );
		g_hAzureKinect = nullptr;
	}
}

// Kinect �̃��C�����[�v����
uint32_t KinectProc()
{
	k4a_wait_result_t hr;
	uint32_t uImageSize = 0;

	// �L���v�`���[����
	k4a_capture_t hCapture = nullptr;
	hr = k4a_device_get_capture( g_hAzureKinect, &hCapture, K4A_WAIT_INFINITE );
	if ( hr == K4A_WAIT_RESULT_SUCCEEDED )
	{
		k4a_image_t hImage;

		// �[�x�C���[�W���擾����
		hImage = k4a_capture_get_depth_image( hCapture );

		// �L���v�`���[���J������
		k4a_capture_release( hCapture );

		if ( hImage )
		{
			// �C���[�W�s�N�Z���̐擪�|�C���^���擾����
			uint8_t* p = k4a_image_get_buffer( hImage );
			if ( p )
			{
				// �C���[�W�T�C�Y���擾����
				uImageSize = (uint32_t) k4a_image_get_size( hImage );
				CopyMemory( g_pDepthMap, p, uImageSize );
			}
			// �C���[�W���J������
			k4a_image_release( hImage );
		}
	}
	return uImageSize;
}

#if ENABLE_CSV_OUTPUT
// CSV �t�@�C���Ƀf�[�^���o�͂���
void WriteCSV()
{
	// CSV ���쐬����
	HANDLE hFile = CreateFileA( "depth.csv", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		for( int y = 0; y < RESOLUTION_HEIGHT; y++ )
		{
			// �[�x���� CSV �ɏo��
			char szText[8192] = "";
			for( int x = 0; x < RESOLUTION_WIDTH; x++ )
			{
				const WORD w = g_pDepthMap[y * RESOLUTION_WIDTH + x];
				sprintf_s( szText, 8192, "%s%d,", szText, w );
			}

			// ���s���ăt�@�C���o��
			DWORD dwWritten;
			DWORD dwLen = (DWORD) strlen( szText );
			strcpy_s( &szText[dwLen - 1], 8192, "\r\n" );
			WriteFile( hFile, szText, dwLen, &dwWritten, NULL );
		}
		CloseHandle( hFile );
	}
}
#endif

LRESULT CALLBACK WndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch( uMsg )
	{
	case WM_PAINT:
		{
			// ��ʕ\������
			PAINTSTRUCT ps;
			HDC hDC = BeginPaint( hWnd, &ps );

			// ��ʃT�C�Y���擾����
			RECT rect;
			GetClientRect( hWnd, &rect );

			// 16-bit �[�x�}�b�v���� 32-bit �J���[�r�b�g�}�b�v�ɕϊ�����
			for( uint32_t u = 0; u < RESOLUTION_WIDTH * RESOLUTION_HEIGHT; u++ )
			{
				const WORD w = g_pDepthMap[u];
				g_pdwPixel[u] = 0xFF000000 | (w << 16) | (w << 8) | w;
			}

			// �J���[�������[�x�̕\��
			StretchBlt( hDC, 0, 0, rect.right, rect.bottom, g_hDCBMP, 0, 0, RESOLUTION_WIDTH, RESOLUTION_HEIGHT, SRCCOPY );
			EndPaint( hWnd, &ps );
		}
		return 0;
#if ENABLE_CSV_OUTPUT
	case WM_KEYDOWN:
		// �X�y�[�X�L�[�������ꂽ�� CSV �o�͂���
		if ( wParam == VK_SPACE )
			WriteCSV();
		break;
#endif
	case WM_CLOSE:
		DestroyWindow( hWnd );
	case WM_DESTROY:
		PostQuitMessage( 0 );
		break;
	default:
		return DefWindowProc( hWnd, uMsg, wParam, lParam );
	}
	return 0;
}

// �A�v���P�[�V�����̏����� (�E�B���h�E��`��p�̃r�b�g�}�b�v���쐬)
HRESULT InitApp( HINSTANCE hInst, int nCmdShow )
{
	WNDCLASSEX wc = { 0, };
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
	wc.hCursor = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (HBRUSH) GetStockObject( NULL_BRUSH );
	wc.lpszClassName = szClassName;
	wc.hIconSm = LoadIcon( NULL, IDI_APPLICATION );
	if ( ! RegisterClassEx( &wc ) )
	{
		MessageBox( NULL, TEXT("�A�v���P�[�V�����N���X�̏������Ɏ��s"), TEXT("�G���["), MB_OK );
		return E_FAIL;
	}

	// �A�v���P�[�V�����E�B���h�E���쐬����
	g_hWnd = CreateWindow( szClassName, szClassName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInst, NULL );
	if ( ! g_hWnd )
	{
		MessageBox( NULL, TEXT("�E�B���h�E�̏������Ɏ��s"), TEXT("�G���["), MB_OK );
		return E_FAIL;
	}

	// ��ʕ\���p�̃r�b�g�}�b�v���쐬����
	ZeroMemory( &g_biBMP, sizeof(g_biBMP) );
	g_biBMP.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	g_biBMP.bmiHeader.biBitCount = 32;
	g_biBMP.bmiHeader.biPlanes = 1;
	g_biBMP.bmiHeader.biWidth = RESOLUTION_WIDTH;
	g_biBMP.bmiHeader.biHeight = -(int) RESOLUTION_HEIGHT;
	g_hBMP = CreateDIBSection( NULL, &g_biBMP, DIB_RGB_COLORS, (LPVOID*) (&g_pdwPixel), NULL, 0 );
	HDC hDC = GetDC( g_hWnd );
	g_hDCBMP = CreateCompatibleDC( hDC );
	ReleaseDC( g_hWnd, hDC );
	g_hBMPold = (HBITMAP) SelectObject( g_hDCBMP, g_hBMP );

	// �[�x�}�b�v�p�o�b�t�@���쐬
	g_pDepthMap = new UINT16[RESOLUTION_WIDTH * RESOLUTION_HEIGHT];

	ShowWindow( g_hWnd, nCmdShow );
	UpdateWindow( g_hWnd );

	return S_OK;
}

// �A�v���P�[�V�����̌�n��
HRESULT UninitApp()
{
	// �[�x�}�b�v���J������
	if ( g_pDepthMap )
	{
		delete [] g_pDepthMap;
		g_pDepthMap = NULL;
	}

	// ��ʕ\���p�̃r�b�g�}�b�v���J������
	if ( g_hDCBMP || g_hBMP )
	{
		SelectObject( g_hDCBMP, g_hBMPold );
		DeleteObject( g_hBMP );
		DeleteDC( g_hDCBMP );
		g_hBMP = NULL;
		g_hDCBMP = NULL;
	}
	return S_OK;
}

// �G���g���[�|�C���g
int WINAPI WinMain( HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow )
{
	// �A�v���P�[�V�����̏������֐����Ă�
	if ( FAILED( InitApp( hInst, nCmdShow ) ) )
		return 1;

	// Kinect �̏������֐����Ă�
	if ( FAILED( CreateKinect() ) )
		return 1;

	// �A�v���P�[�V�������[�v
	MSG msg;
	while( GetMessage( &msg, NULL, 0, 0 ) )
	{
		// �E�B���h�E���b�Z�[�W������
		TranslateMessage( &msg );
		DispatchMessage( &msg );

		// Kinect �����֐����Ă�
		if ( KinectProc() )
		{
			// Kinect ���ɍX�V������Ε`�惁�b�Z�[�W�𔭍s����
			InvalidateRect( g_hWnd, NULL, TRUE );
		}
	}

	// Kinect �̏I���֐����Ă�
	DestroyKinect();

	// �A�v���P�[�V�������I���֐����Ă�
	UninitApp();

	return (int) msg.wParam;
}

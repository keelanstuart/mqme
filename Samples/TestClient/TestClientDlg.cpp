
// TestClientDlg.cpp : implementation file
//

#include "stdafx.h"
#include "TestClient.h"
#include "TestClientDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CTestClientDlg dialog

constexpr UINT WM_MQME_DISCONNECTED = WM_APP + 1;
constexpr UINT WM_MQME_CONNECTED    = WM_APP + 2;


CTestClientDlg::CTestClientDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_TESTCLIENT_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    InitializeCriticalSection(&m_csLog);
}

CTestClientDlg::~CTestClientDlg()
{
    DeleteCriticalSection(&m_csLog);
}

void CTestClientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CTestClientDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_CONNECT, &CTestClientDlg::OnBnClickedConnect)
	ON_BN_CLICKED(IDC_SEND, &CTestClientDlg::OnBnClickedSend)
	ON_MESSAGE(WM_MQME_CONNECTED, &CTestClientDlg::OnConnected)
	ON_MESSAGE(WM_MQME_DISCONNECTED, &CTestClientDlg::OnDisconnected)
END_MESSAGE_MAP()


BOOL CTestClientDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	if (m_edAddr.SubclassDlgItem(IDC_SERVERADDR, this))
		m_edAddr.SetWindowText(_T("localhost"));
	m_btnConnect.SubclassDlgItem(IDC_CONNECT, this);
	m_edLog.SubclassDlgItem(IDC_LOG, this);
	if (m_edInput.SubclassDlgItem(IDC_INPUT, this))
		m_edInput.SetWindowText(_T("This is a test message."));
	m_btnSend.SubclassDlgItem(IDC_SEND, this);

	m_Me = mqme::GenerateChannel();
	m_pClient = mqme::IClient::NewClient();
	if (!m_pClient)
		return FALSE;

	mqme::IClient::PACKET_HANDLER HandlePacket = [this](mqme::IClient *client, mqme::IPacket *packet)
	{
		switch (packet->GetID())
		{
			case 'HIYA':
			{
				mqme::IPacket *pp = mqme::IPacket::NewPacket();
				if (pp)
				{
					mqme::channel_t g = {0};
					g.m_GuidBytes[0] = 123;

					pp->SetContext(g);
					pp->SetData('JOIN', 0, nullptr);
					client->SendPacket(pp);
				}
				break;
			}

			case 'TEXT':
				AppendLog((TCHAR *)(packet->GetData()), RGB(80, 80, 128), false, false);
				break;
		}

		return true;
	};

	m_pClient->RegisterPacketHandler('HIYA', HandlePacket);
	m_pClient->RegisterPacketHandler('TEXT', HandlePacket);

	mqme::IClient::EVENT_HANDLER HandleEvent = [this](mqme::IClient *client, mqme::IClient::EventType ev)
	{
		switch (ev)
		{
			case mqme::IClient::EventType::CONNECTED:
				PostMessage(WM_MQME_CONNECTED);
				break;

			case mqme::IClient::EventType::DISCONNECTED:
				PostMessage(WM_MQME_DISCONNECTED);
				break;
		}
		return true;
	};

	m_pClient->RegisterEventHandler(mqme::IClient::EventType::CONNECTED, HandleEvent);
	m_pClient->RegisterEventHandler(mqme::IClient::EventType::DISCONNECTED, HandleEvent);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CTestClientDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CTestClientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CTestClientDlg::AppendLog(const TCHAR *text, COLORREF color, bool bold, bool italic)
{
    EnterCriticalSection(&m_csLog);

    CHARFORMAT cf = { 0 };
	int txtlen = m_edLog.GetTextLength();

	CString t;
	if (txtlen)
		t = _T("\r\n");

	t += text;

	cf.cbSize = sizeof(cf);
	//cf.dwMask = CFM_BOLD | CFM_ITALIC | CFM_COLOR;
	//cf.dwEffects = (bold ? CFE_BOLD : 0) | (italic ? CFE_ITALIC : 0) & ~CFE_AUTOCOLOR;
	cf.crTextColor = color;

	m_edLog.SetSel(txtlen, -1);
	m_edLog.ReplaceSel(t);

	m_edLog.SetSel(m_edLog.GetTextLength() - txtlen, m_edLog.GetTextLength());
	m_edLog.SetSelectionCharFormat(cf);
	m_edLog.LineScroll(m_edLog.GetLineCount(), 0);

    LeaveCriticalSection(&m_csLog);
}


void CTestClientDlg::OnBnClickedConnect()
{
	if (m_pClient)
	{
		// disable the connect button first
		// it'll be re-enabled when we either connect or disconnect via event callback
		m_btnConnect.EnableWindow(false);

		if (m_pClient->IsConnected())
		{
			m_pClient->Disconnect();
		}
		else
		{
			CString addr_;
			m_edAddr.GetWindowText(addr_);
			CStringA addr = CW2A(addr_);

			if (m_pClient->Connect((LPCSTR)addr, 12345, &m_Me))
			{
				mqme::IPacket *pp = mqme::IPacket::NewPacket();
				if (pp)
				{
					pp->SetData('HELO', 0, nullptr);
					m_pClient->SendPacket(pp);
				}
			}
		}
	}
}


void CTestClientDlg::OnOK()
{
}


void CTestClientDlg::OnCancel()
{
	CDialogEx::OnCancel();
}


void CTestClientDlg::OnBnClickedSend()
{
	CString t;
	m_edInput.GetWindowText(t);
	if (t.IsEmpty())
		return;

	AppendLog(t, RGB(0, 0, 0), false, false);

	mqme::IPacket *pp = mqme::IPacket::NewPacket();
	if (pp)
	{
		mqme::channel_t g = {0};
		g.m_GuidBytes[0] = 123;

		pp->SetContext(g);
		pp->SetData('TEXT', (t.GetLength() + 1) * sizeof(TCHAR), (BYTE *)((LPCTSTR)t));
		m_pClient->SendPacket(pp);
	}
}


LRESULT CTestClientDlg::OnConnected(WPARAM wparam, LPARAM lparam)
{
	AppendLog(_T("Connected"), RGB(80, 128, 80), false, true);

	m_btnSend.EnableWindow();
	m_edAddr.EnableWindow(FALSE);
	m_btnConnect.SetWindowText(_T("Disconnect"));
	m_btnConnect.EnableWindow();

	return 0;
}


LRESULT CTestClientDlg::OnDisconnected(WPARAM wparam, LPARAM lparam)
{
	AppendLog(_T("Disonnected"), RGB(128, 80, 80), false, true);

	m_btnSend.EnableWindow(FALSE);
	m_edAddr.EnableWindow();
	m_btnConnect.SetWindowText(_T("Connect"));
	m_btnConnect.EnableWindow();

	return 0;
}

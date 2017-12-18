
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



CTestClientDlg::CTestClientDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_TESTCLIENT_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
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
END_MESSAGE_MAP()


// CTestClientDlg message handlers

bool CTestClientDlg::HandlePacket(mqme::ICoreClient *client, mqme::ICorePacket *packet, LPVOID userdata)
{
	CTestClientDlg *_this = (CTestClientDlg *)userdata;

	switch (packet->GetID())
	{
		case 'HIYA':
		{
			MessageBeep(MB_OK);
			_this->AppendLog(_T("Server responded; joining test channel..."), RGB(0, 0, 0), false, false);

			mqme::ICorePacket *pp = mqme::ICorePacket::NewPacket();
			if (pp)
			{
				GUID g;
				memset(&g, 0, sizeof(GUID));
				g.Data1 = 123;

				pp->SetContext(g);
				pp->SetData('JOIN', 0, nullptr);
				client->SendPacket(pp);
			}
			break;
		}

		case 'TEXT':
			_this->AppendLog((TCHAR *)(packet->GetData()), RGB(0, 0, 0), false, false);
			break;
	}

	return true;
}

bool CTestClientDlg::HandleEvent(mqme::ICoreClient *client, mqme::ICoreClient::EEventType ev, LPVOID userdata)
{
	CTestClientDlg *_this = (CTestClientDlg *)userdata;

	switch (ev)
	{
		case mqme::ICoreClient::ET_DISCONNECTED:
			_this->AppendLog(_T("Disconnected"), RGB(0, 0, 0), false, false);
			break;
	}
	return true;
}

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

	m_pClient = mqme::ICoreClient::NewClient();
	if (!m_pClient)
		return FALSE;

	m_pClient->RegisterPacketHandler('HIYA', HandlePacket, (LPVOID)this);
	m_pClient->RegisterPacketHandler('TEXT', HandlePacket, (LPVOID)this);

	m_pClient->RegisterEventHandler(mqme::ICoreClient::ET_DISCONNECTED, HandleEvent, (LPVOID)this);

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
	CHARFORMAT cf = { 0 };
	int txtlen = m_edLog.GetTextLength();

	CString t;
	if (txtlen)
		t = _T("\n");

	t += text;

	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_BOLD | CFM_ITALIC | CFM_COLOR;
	cf.dwEffects = (bold ? CFE_BOLD : 0) | (italic ? CFE_ITALIC : 0) | ~CFE_AUTOCOLOR;
	cf.crTextColor = color;

	m_edLog.SetSel(txtlen, -1);
	m_edLog.ReplaceSel(t);

	m_edLog.SetSel(m_edLog.GetTextLength() - txtlen, m_edLog.GetTextLength());
	m_edLog.SetSelectionCharFormat(cf);
	m_edLog.LineScroll(m_edLog.GetLineCount(), 0);
}


void CTestClientDlg::OnBnClickedConnect()
{
	if (m_pClient)
	{
		if (m_pClient->IsConnected())
		{
			m_btnConnect.EnableWindow(false);
			m_pClient->Disconnect();
			if (!m_pClient->IsConnected())
			{
				m_btnConnect.SetWindowText(_T("Connect"));
				m_btnConnect.EnableWindow(true);
				m_edAddr.EnableWindow(true);
			}
		}
		else
		{
			CString addr;
			m_edAddr.GetWindowText(addr);
			if (m_pClient->Connect((LPCTSTR)addr, 12345))
			{
				m_edAddr.EnableWindow(false);
				m_btnConnect.SetWindowText(_T("Disconnect"));
				AppendLog(_T("Connected"), RGB(0, 0, 0), false, false);

				mqme::ICorePacket *pp = mqme::ICorePacket::NewPacket();
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
//	CDialogEx::OnOK();
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

//	if (!m_pClient->IsConnected())
//		return;

	mqme::ICorePacket *pp = mqme::ICorePacket::NewPacket();
	if (pp)
	{
		GUID g;
		memset(&g, 0, sizeof(GUID));
		g.Data1 = 123;

		pp->SetContext(g);
		pp->SetData('TEXT', (t.GetLength() + 1) * sizeof(TCHAR), (BYTE *)((LPCTSTR)t));
		m_pClient->SendPacket(pp);
	}
}

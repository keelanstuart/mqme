
// TestClientDlg.h : header file
//

#pragma once

#include <mqme.h>

// CTestClientDlg dialog
class CTestClientDlg : public CDialogEx
{
// Construction
public:
	CTestClientDlg(CWnd* pParent = NULL);	// standard constructor
    virtual ~CTestClientDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TESTCLIENT_DIALOG };
#endif

	void AppendLog(const TCHAR *text, COLORREF color, bool bold, bool italic);

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	mqme::channel_t m_Me;
	mqme::IClient *m_pClient;
    CRITICAL_SECTION m_csLog;

	CEdit m_edAddr;
	CButton m_btnConnect;
	CRichEditCtrl m_edLog;
	CEdit m_edInput;
	CButton m_btnSend;

	static bool HandlePacket(mqme::IClient *client, mqme::IPacket *packet, LPVOID userdata);
	static bool HandleEvent(mqme::IClient *client, mqme::IClient::EventType ev, LPVOID userdata);

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedConnect();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnBnClickedSend();
	afx_msg LRESULT OnConnected(WPARAM wparam, LPARAM lparam);
	afx_msg LRESULT OnDisconnected(WPARAM wparam, LPARAM lparam);
};

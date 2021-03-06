/*
 * InfraRecorder - CD/DVD burning software
 * Copyright (C) 2006-2012 Christian Kindahl
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.hh"
#include <ckmmc/util.hh>
#include <base/string_util.hh>
#include "infrarecorder.hh"
#include "ctrl_messages.hh"
#include "cdrtools_parse_strings.hh"
#include "string_table.hh"
#include "settings.hh"
#include "lang_util.hh"
#include "info_dlg.hh"
#include "trans_util.hh"
#include "device_util.hh"
#include "core2_util.hh"
#include "version.hh"
#include "visual_styles.hh"
#include "burn_image_general_page.hh"

CBurnImageGeneralPage::CBurnImageGeneralPage(bool bImageHasTOC,bool bEnableOnFly,
                                             bool bEnableVerify)
{
    m_bImageHasTOC = bImageHasTOC;
    m_bEnableOnFly = bEnableOnFly;
    m_bEnableVerify = bEnableVerify;
    m_uiParentTitleLen = 0;
    m_hIcon = NULL;
    m_hRefreshIcon = NULL;
    m_hRefreshImageList = NULL;

    // Try to load translated string.
    if (g_LanguageSettings.m_pLngProcessor != NULL)
    {	
        // Make sure that there is a strings translation section.
        if (g_LanguageSettings.m_pLngProcessor->EnterSection(_T("strings")))
        {
            TCHAR *szStrValue;
            if (g_LanguageSettings.m_pLngProcessor->GetValuePtr(TITLE_GENERAL,szStrValue))
                SetTitle(szStrValue);
        }
    }

    m_psp.dwFlags |= PSP_HASHELP;
}

CBurnImageGeneralPage::~CBurnImageGeneralPage()
{
    if (m_hRefreshImageList != NULL)
        ImageList_Destroy(m_hRefreshImageList);

    if (m_hRefreshIcon != NULL)
        DestroyIcon(m_hRefreshIcon);

    if (m_hIcon != NULL)
        DestroyIcon(m_hIcon);
}

bool CBurnImageGeneralPage::Translate()
{
    if (g_LanguageSettings.m_pLngProcessor == NULL)
        return false;

    CLngProcessor *pLng = g_LanguageSettings.m_pLngProcessor;
    
    // Make sure that there is a burn translation section.
    if (!pLng->EnterSection(_T("burn")))
        return false;

    // Translate.
    TCHAR *szStrValue;
    int iMaxStaticRight = 0;

    if (pLng->GetValuePtr(IDC_WRITESPEEDSTATIC,szStrValue))
    {
        SetDlgItemText(IDC_WRITESPEEDSTATIC,szStrValue);

        // Update the static width if necessary.
        int iStaticRight = UpdateStaticWidth(m_hWnd,IDC_WRITESPEEDSTATIC,szStrValue);
        if (iStaticRight > iMaxStaticRight)
            iMaxStaticRight = iStaticRight;
    }
    if (pLng->GetValuePtr(IDC_WRITEMETHODSTATIC,szStrValue))
    {
        SetDlgItemText(IDC_WRITEMETHODSTATIC,szStrValue);

        // Update the static width if necessary.
        int iStaticRight = UpdateStaticWidth(m_hWnd,IDC_WRITEMETHODSTATIC,szStrValue);
        if (iStaticRight > iMaxStaticRight)
            iMaxStaticRight = iStaticRight;
    }
    if (pLng->GetValuePtr(IDC_NUMCOPIESSTATIC,szStrValue))
    {
        SetDlgItemText(IDC_NUMCOPIESSTATIC,szStrValue);

        // Update the static width if necessary.
        int iStaticRight = UpdateStaticWidth(m_hWnd,IDC_NUMCOPIESSTATIC,szStrValue);
        if (iStaticRight > iMaxStaticRight)
            iMaxStaticRight = iStaticRight;
    }
    if (pLng->GetValuePtr(IDC_EJECTCHECK,szStrValue))
        SetDlgItemText(IDC_EJECTCHECK,szStrValue);
    if (pLng->GetValuePtr(IDC_SIMULATECHECK,szStrValue))
        SetDlgItemText(IDC_SIMULATECHECK,szStrValue);
    if (pLng->GetValuePtr(IDC_BUPCHECK,szStrValue))
        SetDlgItemText(IDC_BUPCHECK,szStrValue);
    if (pLng->GetValuePtr(IDC_PADCHECK,szStrValue))
        SetDlgItemText(IDC_PADCHECK,szStrValue);
    if (pLng->GetValuePtr(IDC_FIXATECHECK,szStrValue))
        SetDlgItemText(IDC_FIXATECHECK,szStrValue);
    if (pLng->GetValuePtr(IDC_VERIFYCHECK,szStrValue))
        SetDlgItemText(IDC_VERIFYCHECK,szStrValue);

    // Borrow the on the fly translation from the copy dialog.
    if (!pLng->EnterSection(_T("copy")))
        return false;

    if (pLng->GetValuePtr(IDC_ONFLYCHECK,szStrValue))
        SetDlgItemText(IDC_ONFLYCHECK,szStrValue);

    // Make sure that the edit controls are not in the way of the statics.
    if (iMaxStaticRight > 75)
    {
        UpdateEditPos(m_hWnd,IDC_WRITESPEEDCOMBO,iMaxStaticRight,true);
        UpdateEditPos(m_hWnd,IDC_WRITEMETHODCOMBO,iMaxStaticRight,true);
    }

    return true;
}

bool CBurnImageGeneralPage::InitRecorderMedia()
{
    if (m_uiParentTitleLen == 0)
        m_uiParentTitleLen = GetParentWindow(this).GetWindowTextLength();

    TCHAR szBuffer[256];
    GetParentWindow(this).GetWindowText(szBuffer,m_uiParentTitleLen + 1);

    // Obtain the device.
    ckmmc::Device &Device =
        *reinterpret_cast<ckmmc::Device *>(m_RecorderCombo.GetItemData(
                                          m_RecorderCombo.GetCurSel()));

    Device.refresh();

    // Get current profile.
    ckmmc::Device::Profile Profile = Device.profile();

    // Note: On CD-R/RW media the Test Write bit is valid only for Write Type
    // 1 or 2 (Track at Once or Session at Once). On DVD-R media, the Test
    // Write bit is valid only for Write Type 0 or 2 (Incremental or
    // Disc-at-once). This is completely disregarded at the moment.
    bool bSupportedProfile = false;
    switch (Profile)
    {
        case ckmmc::Device::ckPROFILE_DVDRAM:
        case ckmmc::Device::ckPROFILE_DVDPLUSRW:
        case ckmmc::Device::ckPROFILE_DVDPLUSRW_DL:
        case ckmmc::Device::ckPROFILE_DVDMINUSRW_RESTOV:
        case ckmmc::Device::ckPROFILE_DVDMINUSRW_SEQ:
        case ckmmc::Device::ckPROFILE_DVDPLUSR:
        case ckmmc::Device::ckPROFILE_DVDPLUSR_DL:
            // Not sure about these:
        case ckmmc::Device::ckPROFILE_BDROM:
        case ckmmc::Device::ckPROFILE_BDR_SRM:
        case ckmmc::Device::ckPROFILE_BDR_RRM:
        case ckmmc::Device::ckPROFILE_BDRE:
        case ckmmc::Device::ckPROFILE_HDDVDROM:
        case ckmmc::Device::ckPROFILE_HDDVDR:
        case ckmmc::Device::ckPROFILE_HDDVDRAM:
            bSupportedProfile = true;

            // Disable simulation (not supported for DVD+RW media).
            ::EnableWindow(GetDlgItem(IDC_SIMULATECHECK),FALSE);
            break;

        case ckmmc::Device::ckPROFILE_CDR:
        case ckmmc::Device::ckPROFILE_CDRW:
        case ckmmc::Device::ckPROFILE_DVDMINUSR_SEQ:
        case ckmmc::Device::ckPROFILE_DVDMINUSR_DL_SEQ:
        case ckmmc::Device::ckPROFILE_DVDMINUSR_DL_JUMP:
            bSupportedProfile = true;

            // Enable simulation if supported by recorder.
            ::EnableWindow(GetDlgItem(IDC_SIMULATECHECK),
                           Device.support(ckmmc::Device::ckDEVICE_TEST_WRITE));
            break;

        case ckmmc::Device::ckPROFILE_NONE:
            lstrcat(szBuffer,_T(" "));
            lstrcat(szBuffer,lngGetString(MEDIA_INSERTBLANK));
            break;

        default:
            lstrcat(szBuffer,_T(" "));
            lstrcat(szBuffer,lngGetString(MEDIA_UNSUPPORTED));
            break;
    }

    GetParentWindow(this).SetWindowText(szBuffer);

    if (bSupportedProfile)
    {
        // Maximum write speed.
        m_WriteSpeedCombo.ResetContent();
        m_WriteSpeedCombo.AddString(lngGetString(MISC_MAXIMUM));
        m_WriteSpeedCombo.SetItemData(0,static_cast<DWORD_PTR>(-1));
        m_WriteSpeedCombo.SetCurSel(0);

        // Other supported write speeds.
        const std::vector<ckcore::tuint32> &WriteSpeeds = Device.write_speeds();

        std::vector<ckcore::tuint32>::const_iterator it;
        for (it = WriteSpeeds.begin(); it != WriteSpeeds.end(); it++)
        {
            m_WriteSpeedCombo.AddString(ckmmc::util::kb_to_disp_speed(*it,Profile).c_str());
            m_WriteSpeedCombo.SetItemData(m_WriteSpeedCombo.GetCount() - 1,
                                          static_cast<DWORD_PTR>(ckmmc::util::kb_to_human_speed(*it,
                                                                 /*ckmmc::Device::ckPROFILE_CDR*/Profile)));
        }

        // Write modes.
        m_WriteMethodCombo.ResetContent();

        if (Device.support(ckmmc::Device::ckWM_SAO))
            m_WriteMethodCombo.AddString(lngGetString(WRITEMODE_SAO));
        if (Device.support(ckmmc::Device::ckWM_TAO))
        {
            m_WriteMethodCombo.AddString(lngGetString(WRITEMODE_TAO));
            m_WriteMethodCombo.AddString(lngGetString(WRITEMODE_TAONOPREGAP));
        }
        if (Device.support(ckmmc::Device::ckWM_RAW96R))
            m_WriteMethodCombo.AddString(lngGetString(WRITEMODE_RAW96R));
        if (Device.support(ckmmc::Device::ckWM_RAW16))
            m_WriteMethodCombo.AddString(lngGetString(WRITEMODE_RAW16));
        if (Device.support(ckmmc::Device::ckWM_RAW96P))
            m_WriteMethodCombo.AddString(lngGetString(WRITEMODE_RAW96P));

        m_WriteMethodCombo.SetCurSel(0);
    }
    else
    {
        return false;
    }

    return true;
}

void CBurnImageGeneralPage::SuggestWriteMethod()
{
    ckmmc::Device &Device =
        *reinterpret_cast<ckmmc::Device *>(m_RecorderCombo.GetItemData(
                                          m_RecorderCombo.GetCurSel()));

    // Suggest the best raw write method if the image has an associated TOC file.
    if (m_bImageHasTOC)
    {
        // Check for supported write modes. Raw96r has the highest priority, if
        // that's not support the raw16 mode is recommended, if none of the mentioned
        // write modes are supported a warning message is displayed.
        int uiStrID = -1;

        if (Device.support(ckmmc::Device::ckWM_RAW96R))
            uiStrID = WRITEMODE_RAW96R;
        else if (Device.support(ckmmc::Device::ckWM_RAW16))
            uiStrID = WRITEMODE_RAW16;

        const TCHAR *szStr = lngGetString(uiStrID);
        if (uiStrID != -1)
        {
            TCHAR szItemText[128];

            for (int i = 0; i < m_WriteMethodCombo.GetCount(); i++)
            {
                if (m_WriteMethodCombo.GetLBTextLen(i) >= (sizeof(szItemText) / sizeof(TCHAR)))
                    continue;

                m_WriteMethodCombo.GetLBText(i,szItemText);
                if (!lstrcmp(szItemText,szStr))
                {
                    m_WriteMethodCombo.SetCurSel(i);
                    break;
                }
            }
        }
        else
        {
            // No good raw writing mode found.
            MessageBox(lngGetString(WARNING_CLONEWRITEMETHOD),lngGetString(GENERAL_WARNING),
                MB_OK | MB_ICONWARNING);
        }
    }
    else if (g_ProjectSettings.m_bMultiSession)
    {
        // Suggest the TAO write mode for multi-session discs if available.
        if (Device.support(ckmmc::Device::ckWM_TAO))
        {
            const TCHAR *szStr = lngGetString(WRITEMODE_TAO);
            TCHAR szItemText[128];

            for (int i = 0; i < m_WriteMethodCombo.GetCount(); i++)
            {
                if (m_WriteMethodCombo.GetLBTextLen(i) >= (sizeof(szItemText) / sizeof(TCHAR)))
                    continue;

                m_WriteMethodCombo.GetLBText(i,szItemText);
                if (!lstrcmp(szItemText,szStr))
                {
                    m_WriteMethodCombo.SetCurSel(i);
                    break;
                }
            }
        }
    }
}

void CBurnImageGeneralPage::InitRefreshButton()
{
    m_hRefreshIcon = (HICON)LoadImage(_Module.GetResourceInstance(),MAKEINTRESOURCE(IDI_REFRESHICON),IMAGE_ICON,16,16,LR_DEFAULTCOLOR);

    // In Windows XP there is a bug causing the button to loose it's themed style if
    // assigned an icon. The solution to this is to assign an image list instead.
    if (g_WinVer.m_ulMajorCCVersion >= 6)
    {
        // Get color depth (minimum requirement is 32-bits for alpha blended images).
        int iBitsPixel = GetDeviceCaps(::GetDC(HWND_DESKTOP),BITSPIXEL);

        m_hRefreshImageList = ImageList_Create(16,16,ILC_COLOR32 | (iBitsPixel < 32 ? ILC_MASK : 0),0,1);
        ImageList_AddIcon(m_hRefreshImageList,m_hRefreshIcon);

        BUTTON_IMAGELIST bImageList;
        bImageList.himl = m_hRefreshImageList;
        bImageList.margin.left = 0;
        bImageList.margin.top = 0;
        bImageList.margin.right = 0;
        bImageList.margin.bottom = 0;
        bImageList.uAlign = BUTTON_IMAGELIST_ALIGN_CENTER;
        SendMessage(GetDlgItem(IDC_REFRESHBUTTON),BCM_SETIMAGELIST,0,(LPARAM)&bImageList);
    }
    else
    {
        SendMessage(GetDlgItem(IDC_REFRESHBUTTON),BM_SETIMAGE,IMAGE_ICON,(LPARAM)m_hRefreshIcon);
    }

    // If the application is themed, then adjust the size of the button.
    if (g_VisualStyles.IsThemeActive())
    {
        RECT rcButton;
        ::GetWindowRect(GetDlgItem(IDC_REFRESHBUTTON),&rcButton);
        ScreenToClient(&rcButton);
        ::SetWindowPos(GetDlgItem(IDC_REFRESHBUTTON),NULL,rcButton.left - 1,rcButton.top - 1,
            rcButton.right - rcButton.left + 2,rcButton.bottom - rcButton.top + 2,0);
    }
}

void CBurnImageGeneralPage::CheckRecorderMedia()
{
    // Check that we have a recorder in the system.
    if (m_RecorderCombo.GetItemData(m_RecorderCombo.GetCurSel()) == 0)
        return;

    bool bMediaInit = InitRecorderMedia();
    if (!bMediaInit)
    {
        // Write speed.
        m_WriteSpeedCombo.ResetContent();
        m_WriteSpeedCombo.AddString(lngGetString(MISC_MAXIMUM));
        m_WriteSpeedCombo.SetCurSel(0);

        // Write method.
        m_WriteMethodCombo.ResetContent();
        m_WriteMethodCombo.AddString(lngGetString(MISC_NOTAVAILABLE));
        m_WriteMethodCombo.SetCurSel(0);
    }

    m_WriteSpeedCombo.EnableWindow(bMediaInit);
    m_WriteMethodCombo.EnableWindow(bMediaInit);
    ::EnableWindow(::GetDlgItem(GetParent(),IDOK),bMediaInit);
    ::EnableWindow(GetDlgItem(IDC_WRITESPEEDSTATIC),bMediaInit);
    ::EnableWindow(GetDlgItem(IDC_WRITEMETHODSTATIC),bMediaInit);

    // Suggest a write method.
    SuggestWriteMethod();
}

bool CBurnImageGeneralPage::OnApply()
{
    // Verify the number of copies.
    TCHAR szNumCopies[64];
    m_NumCopiesCombo.GetWindowText(szNumCopies,sizeof(szNumCopies) / sizeof(TCHAR) -1);

    long lNumCopies = 1;
    if (lsscanf(szNumCopies,_T("%d"),&lNumCopies) != 1 || lNumCopies < 1)
    {
        lngMessageBox(m_hWnd,ERROR_NUMCOPIES,GENERAL_ERROR,MB_OK | MB_ICONERROR);
        return false;
    }

    // Remember the configuration.
    g_BurnImageSettings.m_bOnFly = IsDlgButtonChecked(IDC_ONFLYCHECK) == TRUE;
    g_BurnImageSettings.m_bVerify = IsDlgButtonChecked(IDC_VERIFYCHECK) == TRUE;
    g_BurnImageSettings.m_bEject = IsDlgButtonChecked(IDC_EJECTCHECK) == TRUE;
    g_BurnImageSettings.m_bSimulate = IsDlgButtonChecked(IDC_SIMULATECHECK) == TRUE;
    g_BurnImageSettings.m_bBUP = IsDlgButtonChecked(IDC_BUPCHECK) == TRUE;
    g_BurnImageSettings.m_bPadTracks = IsDlgButtonChecked(IDC_PADCHECK) == TRUE;
    g_BurnImageSettings.m_bFixate = IsDlgButtonChecked(IDC_FIXATECHECK) == TRUE;

    // For internal use only.
    g_BurnImageSettings.m_pRecorder =
        reinterpret_cast<ckmmc::Device *>(m_RecorderCombo.GetItemData(
                                          m_RecorderCombo.GetCurSel()));
    g_BurnImageSettings.m_iWriteSpeed = static_cast<int>(m_WriteSpeedCombo.GetItemData(m_WriteSpeedCombo.GetCurSel()));

    TCHAR szBuffer[32];
    GetDlgItemText(IDC_WRITEMETHODCOMBO,szBuffer,32);

    if (!lstrcmp(szBuffer,lngGetString(WRITEMODE_SAO)))
        g_BurnImageSettings.m_iWriteMethod = WRITEMETHOD_SAO;
    else if (!lstrcmp(szBuffer,lngGetString(WRITEMODE_TAO)))
        g_BurnImageSettings.m_iWriteMethod = WRITEMETHOD_TAO;
    else if (!lstrcmp(szBuffer,lngGetString(WRITEMODE_TAONOPREGAP)))
        g_BurnImageSettings.m_iWriteMethod = WRITEMETHOD_TAONOPREGAP;
    else if (!lstrcmp(szBuffer,lngGetString(WRITEMODE_RAW96R)))
        g_BurnImageSettings.m_iWriteMethod = WRITEMETHOD_RAW96R;
    else if (!lstrcmp(szBuffer,lngGetString(WRITEMODE_RAW16)))
        g_BurnImageSettings.m_iWriteMethod = WRITEMETHOD_RAW16;
    else if (!lstrcmp(szBuffer,lngGetString(WRITEMODE_RAW96P)))
        g_BurnImageSettings.m_iWriteMethod = WRITEMETHOD_RAW96P;

    g_BurnImageSettings.m_lNumCopies = lNumCopies;

    return true;
}

void CBurnImageGeneralPage::OnHelp()
{
    TCHAR szFileName[MAX_PATH];
    GetModuleFileName(NULL,szFileName,MAX_PATH - 1);

    ExtractFilePath(szFileName);
    lstrcat(szFileName,lngGetManual());
    lstrcat(szFileName,_T("::/how_to_use/burn_options.html"));

    HtmlHelp(m_hWnd,szFileName,HH_DISPLAY_TOC,NULL);
}

LRESULT CBurnImageGeneralPage::OnInitDialog(UINT uMsg,WPARAM wParam,LPARAM lParam,BOOL &bHandled)
{
    m_RecorderCombo = GetDlgItem(IDC_RECORDERCOMBO);
    m_WriteSpeedCombo = GetDlgItem(IDC_WRITESPEEDCOMBO);
    m_WriteMethodCombo = GetDlgItem(IDC_WRITEMETHODCOMBO);
    m_NumCopiesCombo = GetDlgItem(IDC_NUMCOPIESCOMBO);

    // Set the refresh button icon.
    InitRefreshButton();

    // Create the icon.
    HINSTANCE hInstance = LoadLibrary(_T("shell32.dll"));
        m_hIcon = (HICON)LoadImage(hInstance,MAKEINTRESOURCE(12),IMAGE_ICON,GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_DEFAULTCOLOR);
    FreeLibrary(hInstance);

    SendMessage(GetDlgItem(IDC_ICONSTATIC),STM_SETICON,(WPARAM)m_hIcon,0L);

    // Recorder combo box.
    std::vector<ckmmc::Device *>::const_iterator it;
    for (it = g_DeviceManager.devices().begin(); it !=
        g_DeviceManager.devices().end(); it++)
    {
        ckmmc::Device *pDevice = *it;

        if (!pDevice->recorder())
            continue;

        m_RecorderCombo.AddString(NDeviceUtil::GetDeviceName(*pDevice).c_str());
        m_RecorderCombo.SetItemData(m_RecorderCombo.GetCount() - 1,
                                    reinterpret_cast<DWORD_PTR>(pDevice));
    }

    if (m_RecorderCombo.GetCount() == 0)
    {
        m_RecorderCombo.AddString(lngGetString(FAILURE_NORECORDERS));
        m_RecorderCombo.EnableWindow(false);
        m_RecorderCombo.SetCurSel(0);

        m_WriteSpeedCombo.AddString(lngGetString(MISC_NOTAVAILABLE));
        m_WriteSpeedCombo.EnableWindow(false);
        m_WriteSpeedCombo.SetCurSel(0);

        m_WriteMethodCombo.AddString(lngGetString(MISC_NOTAVAILABLE));
        m_WriteMethodCombo.EnableWindow(false);
        m_WriteMethodCombo.SetCurSel(0);

        // Disable the OK button.
        ::EnableWindow(::GetDlgItem(GetParent(),IDOK),false);
    }
    else
    {
        m_RecorderCombo.SetCurSel(0);

        if (m_bImageHasTOC)
        {
            // Display information message.
            if (g_GlobalSettings.m_bRawImageInfo)
            {
                CInfoDlg InfoDlg(&g_GlobalSettings.m_bRawImageInfo,lngGetString(INFO_RAWIMAGE),INFODLG_NOCANCEL);
                InfoDlg.DoModal();
            }
        }

        BOOL bDummy;
        OnRecorderChange(NULL,NULL,NULL,bDummy);
    }

    // Setup the default settings.
    CheckDlgButton(IDC_ONFLYCHECK,g_BurnImageSettings.m_bOnFly);
    CheckDlgButton(IDC_VERIFYCHECK,g_BurnImageSettings.m_bVerify);
    CheckDlgButton(IDC_EJECTCHECK,g_BurnImageSettings.m_bEject);
    CheckDlgButton(IDC_SIMULATECHECK,g_BurnImageSettings.m_bSimulate);
    CheckDlgButton(IDC_BUPCHECK,g_BurnImageSettings.m_bBUP);
    CheckDlgButton(IDC_PADCHECK,g_BurnImageSettings.m_bPadTracks);
    CheckDlgButton(IDC_FIXATECHECK,g_BurnImageSettings.m_bFixate);

    // Translate the window.
    Translate();

    // Disable the on the fly check box if requested.
    if (!m_bEnableOnFly)
        ::EnableWindow(GetDlgItem(IDC_ONFLYCHECK),FALSE);

    if (!m_bEnableVerify)
        ::EnableWindow(GetDlgItem(IDC_VERIFYCHECK),FALSE);

    // Fill number of copies combo box.
    TCHAR szBuffer[64];
    for (int i = 1; i <= 10; i++)
    {
        lsprintf(szBuffer,_T("%d"),i);
        m_NumCopiesCombo.AddString(szBuffer);
    }
    m_NumCopiesCombo.SetCurSel(0);

    return TRUE;
}

LRESULT CBurnImageGeneralPage::OnTimer(UINT uMsg,WPARAM wParam,LPARAM lParam,BOOL &bHandled)
{
    ckmmc::Device *pDevice =
        reinterpret_cast<ckmmc::Device *>(m_RecorderCombo.GetItemData(
                                          m_RecorderCombo.GetCurSel()));

    // Check for media change.
    if (g_Core2.CheckMediaChange(*pDevice))
        CheckRecorderMedia();

    return TRUE;
}

LRESULT CBurnImageGeneralPage::OnRecorderChange(WORD wNotifyCode,WORD wID,HWND hWndCtl,BOOL &bHandled)
{
    ckmmc::Device *pDevice =
        reinterpret_cast<ckmmc::Device *>(m_RecorderCombo.GetItemData(
                                          m_RecorderCombo.GetCurSel()));

    ::SendMessage(GetParent(),WM_SETDEVICE,0,reinterpret_cast<LPARAM>(pDevice));

    // Kill any already running timers.
    ::KillTimer(m_hWnd,TIMER_ID);
    ::SetTimer(m_hWnd,TIMER_ID,TIMER_INTERVAL,NULL);

    // Initialize the drive media.
    CheckRecorderMedia();

    // General.
    ::EnableWindow(GetDlgItem(IDC_BUPCHECK),pDevice->support(ckmmc::Device::ckDEVICE_BUP));

    bHandled = false;
    return 0;
}

LRESULT CBurnImageGeneralPage::OnRefresh(WORD wNotifyCode,WORD wID,HWND hWndCtl,BOOL &bHandled)
{
    // Initialize the drive media.
    CheckRecorderMedia();

    return 0;
}

LRESULT CBurnImageGeneralPage::OnFixateCheck(WORD wNotifyCode,WORD wID,HWND hWndCtl,BOOL &bHandled)
{
    if (!IsDlgButtonChecked(IDC_FIXATECHECK))
    {
        // Display warning message.
        if (g_GlobalSettings.m_bFixateWarning)
        {
            CInfoDlg InfoDlg(&g_GlobalSettings.m_bFixateWarning,lngGetString(WARNING_NOFIXATION),INFODLG_ICONWARNING);
            if (InfoDlg.DoModal() == IDCANCEL)
                CheckDlgButton(IDC_FIXATECHECK,true);
        }
    }

    return 0;
}
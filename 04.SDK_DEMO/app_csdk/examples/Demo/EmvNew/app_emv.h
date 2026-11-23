
#ifndef __TRANS_CBK_H__
#define __TRANS_CBK_H__

typedef enum _pos_tag_t
{
    POS_TAG_START = 0,

    POS_TAG_HEAD = 0x7F00,
    APP_TAG_HEAD,
    POS_TAG_RES_EN_ONLINE_DATA,
    POS_TAG_RES_EN_ONLINE_RESULT,
    POS_TAG_RES_EN_BATCH_DATA,
    POS_TAG_RES_KN_ONLINE_DATA,
    POS_TAG_RES_KN_APP_DATA,

    APP_TAG_START = 0x9F8301,
	POS_TAG_TRANSACTION_CONTACTLESS_MODE,

    KERNEL_TAG_AID_LIST_TERMINAL                  = 0x9F8110,
    KERNEL_TAG_AID_LIST_CANDIDATE                 = 0x9F8111,
    KERNEL_TAG_AID_LIST_CANDIDATE_COUNTER         = 0x9F8112,
    KERNEL_TAG_AID_SELECT_RESULT_INDEX            = 0x9F8113,
    KERNEL_TAG_AID_LIST_DISPLAYNAME               = 0x9F8114,
    KERNEL_TAG_AID_SELECT_TRYAGAIN                = 0x9F8115,
	TAG_PBOC_ARITHMETIC_SUPPORT_SM                =  0xDF69,            // SM (national cryptographic) algorithm support
	TAG_EMV_TRANS_REVERSAL                        =  0x9F8104,

	POS_TAG_END,
}Pos_Tag_t;


/*Output AID candidate list structure*/
typedef struct _AidCandidate_t
{
	unsigned char _aid[16];		/*	AID	application identifier*/
	unsigned char _aid_len;		/*	length of AID*/
	unsigned char _lable[20];           /* Application label
								50(ICC), ans, 1-16 bytes, we prepare 20 for some PBOC2 errors*/
	unsigned char _lable_len;		/*	length of lable*/
	unsigned char _preferred_name[20];  /* Application preferred name 9F12(ICC), ans, 1-16 bytes */
	unsigned char _preferred_name_len;	/*	length of preferred name*/
	unsigned char _priority;            /* Application priority indicator 87(ICC), b, 1 bytes */
	unsigned char  _enable;              /* indicate whether the candidate is enabled 
									0	remove
									1	available*/
	unsigned char  _table_index;    /*  issuer code table index 9F11(ICC), n1, 1 byte */
	unsigned char _resv[2];		/*	reserve bytes	*/
	int   _file_offset;         		/* the offset of this AID	in the parameters file */
}AidCandidate_t;

typedef  struct  _EmvOnlineData_t
{
	char iccResponse[3];            /* Field 39 */
	unsigned char ackdata[1024];       /* Field 55 */
	int ackdatalen;      				/* Field 55 length */
}EmvOnlineData_t;

typedef enum
{
	EMV_DISP_READING_CARD= 0,
	EMV_DISP_SEE_PHONE ,
	EMV_DISP_NFC_RETAP,
	EMV_DISP_REMOVE_CARD,
	EMV_DISP_OFFLINE_PIN_BLOCKED,

}EmvKernelDisp;

typedef enum
{
    EMV_OFFLINEPIN_INPUT = 1,   /*Offline password*/
    EMV_LAST_OFFLINEPIN_INPUT = 2,   /*Last offline password*/
    EMV_ONLINEPIN_INPUT = 3,   /*Online password*/

}EMV_INPUT_PIN_TYPE;

typedef struct _EmvCallBack_t
{	
	//Multi-application selection: return >=0 for the selected AID index, -1 on failure
	int (*EMV_AidSelect)(AidCandidate_t *pList, int listNum);
	//Set kernel id, return 0 on success, non-zero on failure
	int (*EMV_SetKernelId)(unsigned char *aid,int aidLen,int *kernelId);
	//Card number confirmation, return 0 on confirm, -1 on failure
	int (*EMV_ConfirmCardInfo)(char *pan,int len);
	//Request PIN, cType: 1 offline PIN, 2 last offline PIN, 3 online PIN; return -1 input failed, -2 bypass (no input), -3 transaction aborted or timeout, >0 PIN length
	int (*EMV_InputPasswd)(EMV_INPUT_PIN_TYPE cType,char *pan, char *pszPin);
	//Identity confirmation, type 0-5: ID card/military ID/passport/entry permit/temporary ID card/other; pcon: ID number; return 1 cardholder identity confirmed, 0 identity confirmation failed
	int (*EMV_CertConfirm)(unsigned char type, unsigned char *pcon, unsigned char len);
	//Online, result: -1 online failed, >=0 online succeeded (with response message)
	int (*EMV_OnlineProcess)(EmvOnlineData_t* pOnlineData);
	//Information the kernel requires to display
	int (*EMV_ProcessDisp)(EmvKernelDisp cType);  ///0 success, 1 kernel exit
	void (*EMV_AfterSelectApp)();
	void (*EMV_AfterReadRecord)();
}EmvCallBack_t;

typedef	enum
{
	OFFLINEAPPROVED = 0,
	OFFLINEDECLINED,
	ONLINEAPPROVED,
	ONLINEDECLINED,
	UNABLEONLINE_OFFLINEAPPROVED,
	UNABLEONINE_OFFLINEDECLINED
}EMVTRANSRESULT;

typedef	enum
{
	PROCESS_STATUS_INIT = 0,
	KERNEL_PROCESS_STATUS_NFC_RETAP_SUCCESS,
	KERNEL_PROCESS_STATUS_NFC_RETAP_CANCLE,
	KERNEL_PROCESS_STATUS_NFC_RETAP_TIMEOUT,

}EMV_KERNEL_PROCESS_STATUS;

typedef struct{u8 *head; u32 len;} T_U8_VIEW;
#define UV_OK(uv)	((uv).head && (uv).len)

//Kernel transaction type
typedef enum
{
	EMV_L2_SALE 		= 0x00,  
	EMV_CASH			= 0x01,
	EMV_CASHBACK		= 0x09,
	EMV_INQUIRY			= 0x31,
	EMV_TRANFER			= 0x40,
	EMV_ADMIN			= 0x66,
	EMV_CASHDEPOSIT		= 0x21,
	EMV_PAYMENT			= 0x50,
	EMV_REFUND			= 0x20,
}EMVTransType;

//EMV transaction return values
typedef enum
{
	APP_RC_START = -1,
	APP_RC_COMPLETED = 0,
	APP_RC_TERMINAL,
	APP_RC_CANCEL,
	APP_RC_EMV_DENAIL,
	APP_RC_EMV_GAC2_DENAIL,
	APP_RC_NFC_NOT_ALLOW,//Transaction not allowed
	APP_RC_EMV_APP_BLOCK,//Transaction not allowed
	APP_RC_EMV_APP_SEE_PHONE,
	APP_RC_EMV_TRANS_TRY_ANOTHER_INTERFACE,
	APP_RC_EMV_TRANS_GPO_NOT_SUPPORT,
	APP_RC_FALL_BACK,
	APP_RC_EMV_CARD_BLOCK,
	APP_RC_CARD_NOT_SUPPORT,

	APP_RC_NFC_RETAP_TIMEOUT,
	APP_RC_NFC_RETAP_CANCEL,
	APP_RC_NFC_TERMINAL,
	APP_RC_NFC_DOUBLETAP_DENAIL,
	APP_RC_NFC_MULTI_CARD,
	APP_RC_NFC_TRY_AGAIN,

	APP_RC_TRANS_REVERSEL,

	APP_RC_NUMS,
}EMV_L2_Return;

int app_user_cbk(void *user_data, unsigned char *buf, unsigned int len);
int Emv_SetCoreData(unsigned int tagname, unsigned char *pvalue, int valuelen);
unsigned char* Emv_GetCoreData(unsigned int tagname, int *pvallen);
int Emv_FetchData(unsigned int* tagname, int count, unsigned char* obuf, int olen);
int Emv_KernelInit(EmvCallBack_t t_callbackfun);
EMV_L2_Return Emv_Process(ST_TRANSDATA *emvTransParams);
int Emv_CheckContactlessDoubleTap(EmvOnlineData_t* pOnlineData);
int Emv_SetContactlessOnlineResult(EmvOnlineData_t* pOnlineData);

#endif
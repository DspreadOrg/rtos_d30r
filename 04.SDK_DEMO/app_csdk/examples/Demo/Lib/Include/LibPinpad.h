//-----------------------------------------------------------------------------
// @file    : LibPinPad.h
// @brief   : PinPad-related functions
// @details : 
// @author  : 
// @version :      
// @date    :      
// @history :                  
//-----------------------------------------------------------------------------
#ifndef __LIB_PINPAD_H__
#define __LIB_PINPAD_H__

//-----------------------------------------------------------------------------
// Function declaration
//-----------------------------------------------------------------------------

typedef struct
{
    unsigned char timeout;              // Input timeout, in seconds
    unsigned char min_input_count;      // Minimum input length
    unsigned char max_input_count;      // Maximum input length
    unsigned char is_bypass;            // Is bypass allowed
} pin_param;

typedef enum
{
    KEY_MNG_MKSK = 0,      // MKSK 
    KEY_MNG_DUKPT_DES = 1, // DUKPT DES
    KEY_MNG_DUKPT_AES = 2, // DUKPT AES

    KEY_MNG__ERR
}E_KEY_MNG_SYS;

typedef enum
{
    PROTECT_KEY_ALG_3DES = 0,   // protect key with 3DES Algorithm
    PROTECT_KEY_ALG_AES128 = 1, // protect key with AES128 Algorithm
    PROTECT_KEY_ALG_AES192 = 2, // protect key with AES192 Algorithm
    PROTECT_KEY_ALG_AES256 = 3, // protect key with AES256 Algorithm

    PROTECT_KEY_ALG__ERR
}E_PROTECTION_KEY_ALG;

typedef enum
{
    MKSK_SK_TDK = 0, // Symmetric Key for Data Encryption
    MKSK_SK_MAK = 1, // MAC Key
    MKSK_SK_PIK = 2, // PIN Encryption Key
    MKSK_SK_RESERVE1 = 3, // reserve 01
    MKSK_SK_RESERVE2 = 4,  // reserve 01

    MKSK_SK_ERROR
}MKSK_SK_KEY_TYPE;

typedef enum
{
	DUKPT_DES_PIK          = 0,
	DUKPT_DES_MAK_REQUIRE  = 1,
	DUKPT_DES_MAK_RESPONSE = 2,
	DUKPT_DES_TDK_REQUIRE  = 3,
	DUKPT_DES_TDK_RESPONSE = 4,

    DUKPT_DES_KEY_ERROR
}DUKPT_DES_KEY_TYPE;

typedef enum
{
	DUKPT_AES_KEY_ENC_KEY             = 0,
	DUKPT_AES_KEY_PIN_ENC			  = 1,
	DUKPT_AES_KEY_MSG_AUTH_GENERATION = 2,
	DUKPT_AES_KEY_MSG_AUTH_VERIIFY	  = 3,
	DUKPT_AES_KEY_MSG_AUTH_BOTH		  = 4,
	DUKPT_AES_KEY_MSG_ENC_ENC		  = 5,
	DUKPT_AES_KEY_MSG_ENC_DEC		  = 6,
	DUKPT_AES_KEY_MSG_ENC_BOTH		  = 7,

    DUKPT_AES_KEY_ERROR
}DUKPT_AES_KEY_TYPE;

typedef enum
{
    E_SA_CREDKDH_CER_ID = 0, // KDH Credential Token 
    E_SA_CREDKRD_CER_ID = 1, // KRD Credential Token
}E_SA_CER_ID;

typedef enum
{
    E_CER_NOTBEFORETIME_ID = 0, //Certificate Effective Date
    E_CER_NOTAFTERTIME_ID  = 1, //Certificate Expiration Date
    E_CER_SERIALNUM_ID     = 2, //Certificate Serial Number
    E_CER_PUBKEY_ID        = 3, //Certificate Public Key
} E_CER_DATA;

typedef struct KT_RESP
{
	int taskId;            // the task ID
    unsigned char keyType; // key type
	unsigned char kcv[8];  // Key Check Value
	unsigned char kcvLen;  // length of Key Check Value
    unsigned char reserve[2];
}ST_KT_RESP;

typedef struct KT_RESULT
{
	int taskId;              // the task ID
    unsigned char keyType;   // key type
    unsigned char keyIndex;  // key index
	unsigned char ucKey[32]; // Key Data
	unsigned char keyLen;    // length of Key Data
    unsigned char ucKSN[16]; // KSN
    unsigned char ksnLen;    // length of KSN
}ST_KT_RESULT;

typedef void (*pinpad_callback_t)(unsigned char input_count, const unsigned char ucLastTimes);

typedef int (*ParseKTKdhCallback)(unsigned char *pData, unsigned short usLen,ST_KT_RESULT *pKtResult);


//-----------------------------------------------------------------------------
// Name    : Lib_PciGetPin
// Func    : Get PinBlock
// Paras   : ucKeyId[in],          input key ID
//           pucCardNo[int],       input card number
//           ucMode[in],           input mode
//           pucPinBlock[in/out],  output PinBlock
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------    
int Lib_PciGetPin(unsigned char ucKeyId,  unsigned char *pucCardNo, unsigned char ucMode, unsigned char *pucPinBlock);

//-----------------------------------------------------------------------------
// Name    : Lib_PciSetPinpadUI
// Func    : Set password input UI callback
// Paras   : cb[in],  UI callback
// Ret     :
// Desc    :
//-----------------------------------------------------------------------------
void Lib_PciSetPinpadUI(pinpad_callback_t cb);

//-----------------------------------------------------------------------------
// Name    : Lib_PciOfflinePinpad
// Func    : Offline password input
// Paras   : param[in],         Password keyboard parameters
//           ucLastTimes[in],   The number of attempts left to enter the password
//           pucPinBlock[out],  output PinBlock
// Ret     : ERR_OK, ERR_USER_CANCEL, ERR_CVM_PIN_BYPASS
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciOfflinePinpad(pin_param param, const unsigned char ucLastTimes, unsigned char *pucPinBlock);

//-----------------------------------------------------------------------------
// Name    : Lib_PciOnlinePinpad
// Func    : Online password input
// Paras   : param[in],        Password keyboard parameters
//           ucLastTimes[in],  The number of attempts left to enter the password
// Ret     : ERR_OK, ERR_USER_CANCEL, ERR_CVM_PIN_BYPASS
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciOnlinePinpad(pin_param param, const unsigned char ucLastTimes);

//-----------------------------------------------------------------------------
// Name    : Lib_PciGetMac
// Func    : Calculate MAC
// Paras   : ucKeyId[in],    input key ID 
//           usInLen[in],    input data length
//           pucInData[in],  input data buffer
//           pucMac[in],     output MAC
//           ucMode[in],     input calculation mode, The range is from 0 to 3
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------                   
int Lib_PciGetMac(unsigned char ucKeyId, unsigned short usInLen, unsigned char *pucInData, unsigned char *pucMac, 
                   unsigned char ucMode);

//-----------------------------------------------------------------------------
// Name    : Lib_PciSetKeyManageSys
// Func    : Set Key Management System and Protection Key Algorithm
// Paras   : ucKeyMngSys[in],      input Key Management System; refer to E_KEY_MNG_SYS
//                                 0 is MKSK,1 is DUKPT-DES,2 is DUKPT-AES 
//           ucProtectKeyAlg[in],  input Protection Key Algorithm,refer to E_PROTECTION_KEY_ALG
//                             0 is DES,1 is AES128,2 is AES192,3 is AES256.
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciSetKeyManageSys(E_KEY_MNG_SYS ucKeyMngSys,E_PROTECTION_KEY_ALG ucProtectKeyAlg);

//-----------------------------------------------------------------------------
// Name    : Lib_PciGetKeyManageSys
// Func    : Get Key Management System and Protection Key Algorithm
// Paras   : pucKeyMngSys[in/out],      input data buffer and output Key Management System;
//                                      0 is MKSK,1 is DUKPT-DES,2 is DUKPT-AES
//           pucProtectKeyAlg[in/out],  input data buffer and output Protection Key Algorithm,
//                                      0 is TDES,1 is AES128,2 is AES192,3 is AES256.
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciGetKeyManageSys(E_KEY_MNG_SYS *pucKeyMngSys,E_PROTECTION_KEY_ALG *pucProtectKeyAlg);

//-----------------------------------------------------------------------------
// Name    : Lib_PciWriteKEK
// Func    : Inject KEK.KEK is used to protect other keys in MKSK and DUKPT.
// Paras   : ucKeyId[in],   input key ID
//           pucKey[in],    input plaintext key
//           ucKeyLen[in],  input key length
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciWriteKEK(unsigned char ucKeyId,unsigned char *pucKey, unsigned char ucKeyLen);

//-----------------------------------------------------------------------------
// Name    : Lib_PciWriteMKSK_MK
// Func    : Inject the Master key encrypted with KEK in MKSK
// Paras   : ucKEKId[in],        input KEK ID
//           ucKeyId[in],        input key ID
//           pucKey[in],         input key
//           ucKeyLen[in],       input key length
//           pucCheckValue[in],  input check value,
//                               if neednot check value,0 can be transmitted
//           ucCVLen[in],        input check value length, 3 or 4 bytes.
//                               if neednot check value,0 can be transmitted
//           pucVector[in],      input initial vector in CBC mode,
//                               if ECB mode,0 can be transmitted
//           ucVectorLen[in],    input initial vector length,
//                               8 or 16 in CBC mode, 0 in ECB mode
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciWriteMKSK_MK(unsigned char ucKEKId,unsigned char ucKeyId, unsigned char *pucKey, unsigned char ucKeyLen,
                      unsigned char *pucCheckValue, unsigned char ucCVLen,unsigned char *pucVector,unsigned char ucVectorLen);

//-----------------------------------------------------------------------------
// Name    : Lib_PciWriteMKSK_SK
// Func    : Inject Session Key encrypted with Master Key in MKSK
// Paras   : eKeyType[in],       input Session Key Type(refer to MKSK_SK_KEY_TYPE),
//           ucMKId[in],         input Master Key ID
//           ucKeyId[in],        input key ID
//           pucKey[in],         input key
//           ucKeyLen[in],       input key length
//           pucCheckValue[in],  input check value length,
//                               if neednot check value,0 can be transmitted
//           ucCVLen[in],        input check value length,3 or 4 bytes.
//                               if neednot check value,0 can be transmitted
//           pucVector[in],      input initial vector in CBC mode,
//                               if ECB mode,0 can be transmitted
//           ucVectorLen[in],    input initial vector length,
//                               8 or 16 in CBC mode, 0 in ECB mode
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciWriteMKSK_SK(MKSK_SK_KEY_TYPE eKeyType,unsigned char ucMKId,unsigned char ucKeyId, unsigned char *pucKey, unsigned char ucKeyLen,
                      unsigned char *pucCheckValue, unsigned char ucCVLen,unsigned char *pucVector,unsigned char ucVectorLen);

//-----------------------------------------------------------------------------
// Name    : Lib_PciSKEncOrDec
// Func    : encrypt or decrypt data with session Key in MKSK
// Paras   : eKeyType[in],     input Session Key Type(refer to MKSK_SK_KEY_TYPE),
//           ucKeyId[in],      input Session key ID
//           ucAlg[in],        input Algorithm,0-DES,1-AES
//           encOrDec[in],     input operation type.0 is encryption,1 is decryption.
//           pucData[in/out],  input data and output result
//           usLen[in],        input data length
//           pucVector[in],    input initial vector in CBC mode,
//                             if ECB mode,0 can be transmitted
//           ucVectorLen[in],  input initial vector length,
//                             8 or 16 in CBC mode, 0 in ECB mode
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciSKEncOrDec(MKSK_SK_KEY_TYPE eKeyType,unsigned char ucKeyId,unsigned char ucAlg,unsigned char encOrDec,unsigned char *pucData, unsigned short usLen,
                      unsigned char *pucVector,unsigned char ucVectorLen);

//-----------------------------------------------------------------------------
// Name    : Lib_PciWriteDUKPT_BDK
// Func    : Inject BDK encrypted with KEK and KSN in DUKPT
// Paras   : ucKEKId[in],        input KEK ID
//           ucKeyId[in],        input key ID
//           pucBDK[in],         input ciphertext BDK
//           ucBDKLen[in],       input BDK length
//           pucKSN[in],         input plaintext KSN 
//           ucKSNLen[in],       input KSN length
//           pucCheckValue[in],  input check value,
//                               if neednot check value,0 can be transmitted
//           ucCVLen[in],        input check value length, 3 or 4 bytes.
//                               if neednot check value,0 can be transmitted
//           pucVector[in],      input initial vector in CBC mode,
//                               if ECB mode,0 can be transmitted
//           ucVectorLen[in],    input initial vector length,
//                               8 or 16 in CBC mode, 0 in ECB mode
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciWriteDUKPT_BDK(unsigned char ucKEKId,unsigned char ucKeyId, unsigned char *pucBDK, unsigned char ucBDKLen,
                          unsigned char *pucKSN, unsigned char ucKSNLen,unsigned char *pucCheckValue, unsigned char ucCVLen,
                          unsigned char *pucVector,unsigned char ucVectorLen);

//-----------------------------------------------------------------------------
// Name    : Lib_PciWriteDUKPT_IPEK
// Func    : Inject IPEK encrypted with KEK and KSN in DUKPT
// Paras   : ucKEKId[in],        input KEK ID
//           ucKeyId[in],        input key ID
//           pucIPEK[in],        input ciphertext IPEK
//           ucIPEKLen[in],      input IPEK length
//           pucKSN[in],         input plaintext KSN
//           ucKSNLen[in],       input KSN length
//           pucCheckValue[in],  input check value,
//                               if neednot check value,0 can be transmitted
//           ucCVLen[in],        input check value length, 3 or 4 bytes.
//                               if neednot check value,0 can be transmitted
//           pucVector[in],      input initial vector in CBC mode,
//                               if ECB mode,0 can be transmitted
//           ucVectorLen[in],    input initial vector length,
//                               8 or 16 in CBC mode, 0 in ECB mode
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciWriteDUKPT_IPEK(unsigned char ucKEKId,unsigned char ucKeyId, unsigned char *pucIPEK, unsigned char ucIPEKLen,
                          unsigned char *pucKSN, unsigned char ucKSNLen,unsigned char *pucCheckValue, unsigned char ucCVLen,
                          unsigned char *pucVector,unsigned char ucVectorLen);

//-----------------------------------------------------------------------------
// Name    : Lib_PciDUKPT_DES_EncOrDec
// Func    : encrypt or decrypt data with Key in DUKPT DES
// Paras   : eKeyType[in],     input Key Type(refer to DUKPT_DES_KEY_TYPE),
//           ucKeyId[in],      input key ID
//           ucAlg[in],        input Algorithm,0-DES,1-AES
//           encOrDec[in],     input operation type.0 is encryption,1 is decryption.
//           pucData[in/out],  input data and output result
//           usLen[in],        input data length
//           pucVector[in],    input initial vector in CBC mode,
//                             if ECB mode,0 can be transmitted
//           ucVectorLen[in],  input initial vector length,
//                             8 or 16 in CBC mode, 0 in ECB mode
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciDUKPT_DES_EncOrDec(DUKPT_DES_KEY_TYPE eKeyType,unsigned char ucKeyId,unsigned char ucAlg,unsigned char encOrDec,unsigned char *pucData, unsigned short usLen,
                      unsigned char *pucVector,unsigned char ucVectorLen);

//-----------------------------------------------------------------------------
// Name    : Lib_PciDUKPT_AES_EncOrDec
// Func    : encrypt or decrypt data with Key in DUKPT AES
// Paras   : eKeyType[in],     input Key Type(refer to DUKPT_AES_KEY_TYPE),
//           ucKeyId[in],      input key ID
//           ucAlg[in],        input Algorithm,0-DES,1-AES
//           encOrDec[in],     input operation type.0 is encryption,1 is decryption.
//           pucData[in/out],  input data and output result
//           usLen[in],        input data length
//           pucVector[in],    input initial vector in CBC mode,
//                             if ECB mode,0 can be transmitted
//           ucVectorLen[in],  input initial vector length,
//                               8 or 16 in CBC mode, 0 in ECB mode
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciDUKPT_AES_EncOrDec(DUKPT_AES_KEY_TYPE eKeyType,unsigned char ucKeyId,unsigned char ucAlg,unsigned char encOrDec,unsigned char *pucData, unsigned short usLen,
                      unsigned char *pucVector,unsigned char ucVectorLen);

//-----------------------------------------------------------------------------
// Name    : Lib_PciDUKPT_GetKsn
// Func    : Get Current KSN for the key ID set of DUKPT Keys
// Paras   : ucKeyId[in],        input key ID
//           pucKsn[in/out],     input data buffer and output KSN
//           pucKsnLen[in/out],  input data length and output KSN length
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciDUKPT_GetKsn(unsigned char ucKeyId,unsigned char *pucKsn,unsigned char *pucKsnLen);

//-----------------------------------------------------------------------------
// Name    : Lib_PciDUKPT_IncreaseKsn
// Func    : Increase Current KSN for the key ID set of DUKPT Keys
// Paras   : ucKeyId[in],  input key ID
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciDUKPT_IncreaseKsn(unsigned char ucKeyId);

//-----------------------------------------------------------------------------
// Name    : Lib_PciWriteX9_143
// Func    : Inject X9.143 Key Block
// Paras   : ucKEKId[in],        input KEK ID as the KBPK ID
//           ucKeyId[in],        input key ID
//           pucKeyBlock[in],    input X9.143 Key Block
//           usKeyBlockLen[in],  input X9.143 Key Block length
// Ret     : return Key Usage Value in X9.143 Key Block.
//           For example,'B0'(0x4230) and so on.
//           If failed,a value less than 0 is returned
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciWriteX9_143(unsigned char ucKEKId,unsigned char ucKeyId, unsigned char *pucKeyBlock, unsigned short usKeyBlockLen);

//-----------------------------------------------------------------------------
// Name    : Lib_PciCheckCA
// Func    : Check whether KDH and KRD Certificate Authority are valid.
// Paras   : 
// Ret     : 0 - Valid; 
//           -1 - KDH Certificate Authority is invalid
//           -2 - KRD Certificate Authority is invalid
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciCheckCA(void);

//-----------------------------------------------------------------------------
// Name    : Lib_PciSetCA
// Func    : Set KDH and KRD PEM Certificate Authority
// Paras   : pemCAkdh[in],  input KDH PEM Certificate Authority
//           lenCAkdh[in],  input length of KDH PEM Certificate Authority
//         : pemCAkrd[in],  input KRD PEM Certificate Authority
//           lenCAkrd[in],  input length of KRD PEM Certificate Authority
// Ret     : 0 - success, -1 - invalid parametes,
//           -2 - failed to set KDH CA 
//           -3 - failed to set KRD CA 
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciSetCA(unsigned char *pemCAkdh,unsigned short lenCAkdh,unsigned char *pemCAkrd,unsigned short lenCAkrd);

//-----------------------------------------------------------------------------
// Name    : Lib_PciCheckCT
// Func    : Check whether KDH and KRD Credential Token are valid.
// Paras   : eIndex[in],  input E_SA_CER_ID
// Ret     : 0 - valid, others - invalid
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciCheckCT(E_SA_CER_ID eIndex);

//-----------------------------------------------------------------------------
// Name    : Lib_PciGetKrdCsr
// Func    : Get KRD Certificate Signing Request
// Paras   : pucOutBuf[out],  input data buffer and output KRD Certificate 
//                            Signing Request
//           outLen[out],     output length of KRD Certificate Signing Request
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciGetKrdCsr(unsigned char *pucOutBuf, unsigned short *outLen);

//-----------------------------------------------------------------------------
// Name    : Lib_PciSetCTKrd
// Func    : Set KRD Credential Token from KDH
// Paras   : pemBuf[in],  input KRD Credential Token 
//           pemLen[in],  input length of KRD Credential Token 
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciSetCTKrd(unsigned char *pemBuf, unsigned int pemLen);

//-----------------------------------------------------------------------------
// Name    : Lib_PciGetCTKrd
// Func    : Get KRD Credential Token
// Paras   : pucOutBuf[out],  input data buffer and output KRD Certificate Token
//           outLen[out],     output length of KRD Certificate Token
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciGetCTKrd(unsigned char *pucBuf, unsigned short *OutLen);

//-----------------------------------------------------------------------------
// Name    : Lib_PciSetCTKdh
// Func    : Set KDH Credential Token from KDH
// Paras   : pemBuf[in],  input KDH Credential Token 
//           pemLen[in],  input length of KDH Credential Token 
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciSetCTKdh(unsigned char *pemBuf, unsigned int pemLen);

//-----------------------------------------------------------------------------
// Name    : Lib_PciGetItemFromCT
// Func    : Get item from Credential Token
// Paras   : eCerIndex[in],   input E_SA_CER_ID
//           eDataIndex[in],  input E_CER_DATA
//           outData[out],    input data buffer and output item
//           outLen[out],     output length of item
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciGetItemFromCT(E_SA_CER_ID eCerIndex, E_CER_DATA eDataIndex, unsigned char *outData, unsigned short *outLen);

//-----------------------------------------------------------------------------
// Name    : Lib_PciGetKrdRandom
// Func    : Get base64 random from KRD
// Paras   : pucBuf[out],  input data buffer and output base64 random
//           outLen[out],  output length of base64 random
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciGetKrdRandom(unsigned char *pucBuf, unsigned short *outLen);

//-----------------------------------------------------------------------------
// Name    : Lib_PciUpdateNewCTKdh
// Func    : Update new KDH Credential Token from KDH
// Paras   : pemBuf[in],  input KDH Credential Token 
//           pemLen[in],  input length of KDH Credential Token 
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciUpdateNewCTKdh(unsigned char *pemBuf, unsigned int pemLen);
    
//-----------------------------------------------------------------------------
// Name    : Lib_PciWriteKTKdh
// Func    : Write new KDH Key Token to the key ID postion
// Paras   : pemBuf[in],    input KDH Key Token 
//           pemLen[in],    input length of KDH Key Token 
//           pKTResp[out],  output result,refer to ST_KT_RESP
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciWriteKTKdh(unsigned char *pemBuf, unsigned int pemLen,ST_KT_RESP *pKTResp,ParseKTKdhCallback cb);

//-----------------------------------------------------------------------------
// Name    : Lib_PciDeleteCTKdh
// Func    : Delete KDH Credential Token after unbinding 
// Paras   : pemBuf[in],  input KDH Key Token 
//           pemLen[in],  input length of KDH Key Token 
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciDeleteCTKdh(unsigned char *pemBuf, unsigned int pemLen);

//-----------------------------------------------------------------------------
// Name    : Lib_PciGetBindStatus
// Func    : Get KDH Bind Status
// Paras   : pucStatus[in/out],  input data buffer and output KDH bind status
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciGetBindStatus(unsigned char *pucStatus);

//-----------------------------------------------------------------------------
// Name    : Lib_PciSetBindStatus
// Func    : Set KDH Bind Status
// Paras   : status[in],  input KDH bind status
// Ret     : 0 - success, others - fail
// Desc    :
//-----------------------------------------------------------------------------
int Lib_PciSetBindStatus(unsigned char status);

#endif
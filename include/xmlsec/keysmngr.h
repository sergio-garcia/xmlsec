/** 
 * XMLSec library
 *
 * Simple Keys Manager
 * 
 * See Copyright for the status of this software.
 * 
 * Author: Aleksey Sanin <aleksey@aleksey.com>
 */
#ifndef __XMLSEC_KEYSMGMR_H__
#define __XMLSEC_KEYSMGMR_H__    

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */ 

#include <xmlsec/xmlsec.h>
#include <xmlsec/keys.h>
#include <xmlsec/keyinfo.h>
#include <xmlsec/x509.h>

XMLSEC_EXPORT xmlSecKeysMngrPtr	xmlSecSimpleKeysMngrCreate 	(void);
XMLSEC_EXPORT void		xmlSecSimpleKeysMngrDestroy 	(xmlSecKeysMngrPtr mngr);

/**
 * Keys functions
 */
XMLSEC_EXPORT xmlSecKeyPtr xmlSecSimpleKeysMngrFindKey	(xmlSecKeysMngrCtxPtr keysMngrCtx);
XMLSEC_EXPORT int	xmlSecSimpleKeysMngrAddKey	(xmlSecKeysMngrPtr mngr, 
							 xmlSecKeyPtr key);
XMLSEC_EXPORT int	xmlSecSimpleKeysMngrLoad 	(xmlSecKeysMngrPtr mngr,
							 const char *uri,
							 int strict); 
XMLSEC_EXPORT int	xmlSecSimpleKeysMngrSave	(const xmlSecKeysMngrPtr mngr, 
							 const char *filename,
							 xmlSecKeyValueType type);
XMLSEC_EXPORT xmlSecKeyPtr xmlSecSimpleKeysMngrLoadPemKey(xmlSecKeysMngrPtr mngr,
							 const char *keyfile,
							 const char *keyPwd,
							 int privateKey);
/**
 * X509 certificates management
 */
#ifndef XMLSEC_NO_X509						 
XMLSEC_EXPORT xmlSecKeyDataPtr	xmlSecSimpleKeysMngrX509Find (xmlSecKeysMngrCtxPtr keysMngrCtx,
							 xmlChar *subjectName,
							 xmlChar *issuerName,
							 xmlChar *issuerSerial,
							 xmlChar *ski);
XMLSEC_EXPORT int	xmlSecSimpleKeysMngrX509Verify	(xmlSecKeysMngrCtxPtr keysMngrCtx,
    							 xmlSecKeyDataPtr cert);  
XMLSEC_EXPORT int	xmlSecSimpleKeysMngrLoadPemCert	(xmlSecKeysMngrPtr mngr,
							 const char *filename,
							 int trusted);
XMLSEC_EXPORT int	xmlSecSimpleKeysMngrAddCertsDir	(xmlSecKeysMngrPtr mngr,
							 const char *path);
XMLSEC_EXPORT int	xmlSecSimpleKeysMngrLoadPkcs12	(xmlSecKeysMngrPtr mngr,
							 const char* name,
							 const char *filename,
							 const char *pwd);
XMLSEC_EXPORT void	xmlSecSimpleKeysMngrSetCertsFlags(xmlSecKeysMngrPtr mngr,
							unsigned long flags);    
#endif /* XMLSEC_NO_X509 */


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __XMLSEC_KEYSMGMR_H__ */


/** 
 * XMLSec library
 *
 * "XML Encryption" implementation
 *  http://www.w3.org/TR/xmlenc-core
 * 
 * See Copyright for the status of this software.
 * 
 * Author: Aleksey Sanin <aleksey@aleksey.com>
 */
#include "globals.h"

#ifndef XMLSEC_NO_XMLENC
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libxml/tree.h>
#include <libxml/parser.h> 

#include <xmlsec/xmlsec.h>
#include <xmlsec/buffer.h>
#include <xmlsec/xmltree.h>
#include <xmlsec/keys.h>
#include <xmlsec/keysmngr.h>
#include <xmlsec/transforms.h>
#include <xmlsec/keyinfo.h>
#include <xmlsec/xmlenc.h>
#include <xmlsec/errors.h>

static int 	xmlSecEncCtxEncDataNodeRead		(xmlSecEncCtxPtr encCtx, 
							 xmlNodePtr node);
static int 	xmlSecEncCtxEncDataNodeWrite		(xmlSecEncCtxPtr encCtx);
static int 	xmlSecEncCtxCipherDataNodeRead		(xmlSecEncCtxPtr encCtx, 
							 xmlNodePtr node);
static int 	xmlSecEncCtxCipherReferenceNodeRead	(xmlSecEncCtxPtr encCtx, 
							 xmlNodePtr node);

/* The ID attribute in XMLEnc is 'Id' */
static const xmlChar*		xmlSecEncIds[] = { BAD_CAST "Id", NULL };


xmlSecEncCtxPtr	
xmlSecEncCtxCreate(xmlSecKeysMngrPtr keysMngr) {
    xmlSecEncCtxPtr encCtx;
    int ret;
    
    encCtx = (xmlSecEncCtxPtr) xmlMalloc(sizeof(xmlSecEncCtx));
    if(encCtx == NULL) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlMalloc",
		    XMLSEC_ERRORS_R_MALLOC_FAILED,
		    "sizeof(xmlSecEncCtx)=%d", 
		    sizeof(xmlSecEncCtx));
	return(NULL);
    }
    
    ret = xmlSecEncCtxInitialize(encCtx, keysMngr);
    if(ret < 0) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecEncCtxInitialize",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	xmlSecEncCtxDestroy(encCtx);
	return(NULL);   
    }
    return(encCtx);    
}

void  
xmlSecEncCtxDestroy(xmlSecEncCtxPtr encCtx) {
    xmlSecAssert(encCtx != NULL);
    
    xmlSecEncCtxFinalize(encCtx);
    xmlFree(encCtx);
}

int 
xmlSecEncCtxInitialize(xmlSecEncCtxPtr encCtx, xmlSecKeysMngrPtr keysMngr) {
    int ret;
    
    xmlSecAssert2(encCtx != NULL, -1);
    
    memset(encCtx, 0, sizeof(xmlSecEncCtx));

    /* initialize key info */
    ret = xmlSecKeyInfoCtxInitialize(&(encCtx->keyInfoReadCtx), keysMngr);
    if(ret < 0) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecKeyInfoCtxInitialize",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);   
    }
    encCtx->keyInfoReadCtx.mode = xmlSecKeyInfoModeRead;
    
    ret = xmlSecKeyInfoCtxInitialize(&(encCtx->keyInfoWriteCtx), keysMngr);
    if(ret < 0) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecKeyInfoCtxInitialize",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);   
    }
    encCtx->keyInfoWriteCtx.mode = xmlSecKeyInfoModeWrite;
    /* it's not wise to write private key :) */
    encCtx->keyInfoWriteCtx.keyReq.keyType = xmlSecKeyDataTypePublic;

    /* initializes transforms encCtx */
    ret = xmlSecTransformCtxInitialize(&(encCtx->encTransformCtx));
    if(ret < 0) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecTransformCtxInitialize",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);   
    }

    encCtx->allowedCipherReferenceUris = xmlSecUriTypeAny;	    
    return(0);
}

void 
xmlSecEncCtxFinalize(xmlSecEncCtxPtr encCtx) {
    xmlSecAssert(encCtx != NULL);

    xmlSecTransformCtxFinalize(&(encCtx->encTransformCtx));
    xmlSecKeyInfoCtxFinalize(&(encCtx->keyInfoReadCtx));
    xmlSecKeyInfoCtxFinalize(&(encCtx->keyInfoWriteCtx));

    if((encCtx->dontDestroyEncMethod == 0) && (encCtx->encMethod != NULL)) {
	xmlSecTransformDestroy(encCtx->encMethod, 1);
    }    
    if(encCtx->encKey != NULL) {
	xmlSecKeyDestroy(encCtx->encKey);
    }
    if(encCtx->id != NULL) {
	xmlFree(encCtx->id);
    }	
    if(encCtx->type != NULL) {
	xmlFree(encCtx->type);
    }
    if(encCtx->mimeType != NULL) {
	xmlFree(encCtx->mimeType);
    }
    if(encCtx->encoding != NULL) {
	xmlFree(encCtx->encoding);
    }	
    if(encCtx->recipient != NULL) {
	xmlFree(encCtx->recipient);
    }
    if(encCtx->carriedKeyName != NULL) {
	xmlFree(encCtx->carriedKeyName);
    }
    
    memset(encCtx, 0, sizeof(xmlSecEncCtx));
}

int 
xmlSecEncCtxBinaryEncrypt(xmlSecEncCtxPtr encCtx, xmlNodePtr tmpl, 
			  const unsigned char* data, size_t dataSize) {
    int ret;
    
    xmlSecAssert2(encCtx != NULL, -1);
    xmlSecAssert2(encCtx->result == NULL, -1);
    xmlSecAssert2(tmpl != NULL, -1);
    xmlSecAssert2(data != NULL, -1);

    /* initialize context and add ID atributes to the list of known ids */    
    encCtx->encrypt = 1;
    xmlSecAddIDs(tmpl->doc, tmpl, xmlSecEncIds);

    /* read the template and set encryption method, key, etc. */
    ret = xmlSecEncCtxEncDataNodeRead(encCtx, tmpl);
    if(ret < 0) {
    	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecEncCtxEncDataNodeRead",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }

    ret = xmlSecTransformCtxBinaryExecute(&(encCtx->encTransformCtx), data, dataSize);
    if(ret < 0) {
    	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecTransformCtxBinaryExecute",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    "dataSize=%d",
		    dataSize);
	return(-1);
    }

    encCtx->result = encCtx->encTransformCtx.result;
    xmlSecAssert2(encCtx->result != NULL, -1);
    
    ret = xmlSecEncCtxEncDataNodeWrite(encCtx);
    if(ret < 0) {
    	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecEncCtxEncDataNodeWrite",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }
    return(0);    
}


int 
xmlSecEncCtxXmlEncrypt(xmlSecEncCtxPtr encCtx, xmlNodePtr tmpl, xmlNodePtr node) {
    xmlOutputBufferPtr output;
    int ret;
    
    xmlSecAssert2(encCtx != NULL, -1);
    xmlSecAssert2(encCtx->result == NULL, -1);
    xmlSecAssert2(tmpl != NULL, -1);
    xmlSecAssert2(node != NULL, -1);
    xmlSecAssert2(node->doc != NULL, -1);

    /* initialize context and add ID atributes to the list of known ids */    
    encCtx->encrypt = 1;
    xmlSecAddIDs(tmpl->doc, tmpl, xmlSecEncIds);

    /* read the template and set encryption method, key, etc. */
    ret = xmlSecEncCtxEncDataNodeRead(encCtx, tmpl);
    if(ret < 0) {
    	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecEncCtxEncDataNodeRead",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }

    ret = xmlSecTransformCtxPrepare(&(encCtx->encTransformCtx), xmlSecTransformDataTypeBin);
    if(ret < 0) {
    	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecTransformCtxPrepare",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    "type=bin");
	return(-1);
    }
    
    xmlSecAssert2(encCtx->encTransformCtx.first != NULL, -1);
    output = xmlSecTransformCreateOutputBuffer(encCtx->encTransformCtx.first, 
						&(encCtx->encTransformCtx));
    if(output == NULL) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    xmlSecErrorsSafeString(xmlSecTransformGetName(encCtx->encTransformCtx.first)),
		    "xmlSecTransformCreateOutputBuffer",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }

    /* push data thru */
    if((encCtx->type != NULL) && xmlStrEqual(encCtx->type, xmlSecTypeEncElement)) {
	/* get the content of the node */
	xmlNodeDumpOutput(output, node->doc, node, 0, 0, NULL);
    } else if((encCtx->type != NULL) && xmlStrEqual(encCtx->type, xmlSecTypeEncContent)) {
	xmlNodePtr cur;

	/* get the content of the nodes childs */
	for(cur = node->children; cur != NULL; cur = cur->next) {
	    xmlNodeDumpOutput(output, node->doc, cur, 0, 0, NULL);
	}
    } else {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    NULL,
		    XMLSEC_ERRORS_R_INVALID_TYPE,
		    "type=\"%s\"", 
		    xmlSecErrorsSafeString(encCtx->type));
	xmlOutputBufferClose(output);
	return(-1);	    	
    }
    
    /* close the buffer and flush everything */
    xmlOutputBufferClose(output);

    encCtx->result = encCtx->encTransformCtx.result;
    xmlSecAssert2(encCtx->result != NULL, -1);
    
    ret = xmlSecEncCtxEncDataNodeWrite(encCtx);
    if(ret < 0) {
    	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecEncCtxEncDataNodeWrite",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }
    
    /* now we need to update our original document */
    if((encCtx->type != NULL) && xmlStrEqual(encCtx->type, xmlSecTypeEncElement)) {
	ret = xmlSecReplaceNode(node, tmpl);
	if(ret < 0) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
			NULL,
			"xmlSecReplaceNode",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			"node=%s",
			xmlSecErrorsSafeString(xmlSecNodeGetName(node)));
	    return(-1);
	}
	encCtx->resultReplaced = 1;			       
    } else if(xmlStrEqual(encCtx->type, xmlSecTypeEncContent)) {
	ret = xmlSecReplaceContent(node, tmpl);
	if(ret < 0) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
			NULL,
			"xmlSecReplaceContent",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			"node=%s",
			xmlSecErrorsSafeString(xmlSecNodeGetName(node)));
	    return(-1);
	}
	encCtx->resultReplaced = 1;			       
    } else {
	/* we should've catached this error before */
	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    NULL,
		    XMLSEC_ERRORS_R_INVALID_TYPE,
		    "type=\"%s\"", 
		    xmlSecErrorsSafeString(encCtx->type));
	return(-1);	    	
    }
    return(0);    
}

int 
xmlSecEncCtxUriEncrypt(xmlSecEncCtxPtr encCtx, xmlNodePtr tmpl, const xmlChar *uri) {
    int ret;
    
    xmlSecAssert2(encCtx != NULL, -1);
    xmlSecAssert2(encCtx->result == NULL, -1);
    xmlSecAssert2(tmpl != NULL, -1);
    xmlSecAssert2(uri != NULL, -1);

    /* initialize context and add ID atributes to the list of known ids */    
    encCtx->encrypt = 1;
    xmlSecAddIDs(tmpl->doc, tmpl, xmlSecEncIds);

    /* we need to add input uri transform first */
    ret = xmlSecTransformCtxSetUri(&(encCtx->encTransformCtx), uri, tmpl);
    if(ret < 0) {
    	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecTransformCtxSetUri",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    "uri=%s",
		    uri);
	return(-1);
    }

    /* read the template and set encryption method, key, etc. */
    ret = xmlSecEncCtxEncDataNodeRead(encCtx, tmpl);
    if(ret < 0) {
    	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecEncCtxEncDataNodeRead",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }

    /* encrypt the data */
    ret = xmlSecTransformCtxExecute(&(encCtx->encTransformCtx), tmpl->doc);
    if(ret < 0) {
    	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecTransformCtxExecute",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }
        
    encCtx->result = encCtx->encTransformCtx.result;
    xmlSecAssert2(encCtx->result != NULL, -1);
    
    ret = xmlSecEncCtxEncDataNodeWrite(encCtx);
    if(ret < 0) {
    	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecEncCtxEncDataNodeWrite",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }    
    
    return(0);
}

int 
xmlSecEncCtxDecrypt(xmlSecEncCtxPtr encCtx, xmlNodePtr node) {
    xmlSecBufferPtr buffer;
    int ret;
    
    xmlSecAssert2(encCtx != NULL, -1);
    xmlSecAssert2(node != NULL, -1);
    
    /* decrypt */
    buffer = xmlSecEncCtxDecryptToBuffer(encCtx, node);
    if(buffer == NULL) {
    	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecEncCtxDecryptToBuffer",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }
    
    /* replace original node if requested */
    if((encCtx->type != NULL) && xmlStrEqual(encCtx->type, xmlSecTypeEncElement)) {
	ret = xmlSecReplaceNodeBuffer(node, xmlSecBufferGetData(buffer),  xmlSecBufferGetSize(buffer));
	if(ret < 0) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
			NULL,
			"xmlSecReplaceNodeBuffer",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			"node=%s",
			xmlSecErrorsSafeString(xmlSecNodeGetName(node)));
	    return(-1);	    	
	}
	encCtx->resultReplaced = 1;			       
    } else if((encCtx->type != NULL) && xmlStrEqual(encCtx->type, xmlSecTypeEncContent)) {
	/* replace the node with the buffer */
	ret = xmlSecReplaceNodeBuffer(node, xmlSecBufferGetData(buffer), xmlSecBufferGetSize(buffer));
	if(ret < 0) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
			NULL,
			"xmlSecReplaceNodeBuffer",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			"node=%s",
			xmlSecErrorsSafeString(xmlSecNodeGetName(node)));
	    return(-1);	    	
	}	
	encCtx->resultReplaced = 1;			       
    }
    return(0);
}

xmlSecBufferPtr
xmlSecEncCtxDecryptToBuffer(xmlSecEncCtxPtr encCtx, xmlNodePtr node) {
    int ret;
    
    xmlSecAssert2(encCtx != NULL, NULL);
    xmlSecAssert2(encCtx->result == NULL, NULL);
    xmlSecAssert2(node != NULL, NULL);

    /* initialize context and add ID atributes to the list of known ids */    
    encCtx->encrypt = 0;
    xmlSecAddIDs(node->doc, node, xmlSecEncIds);

    ret = xmlSecEncCtxEncDataNodeRead(encCtx, node);
    if(ret < 0) {
    	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecEncCtxEncDataNodeRead",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(NULL);
    }

    /* decrypt the data */
    if(encCtx->cipherValueNode != NULL) {
        xmlChar* data = NULL;
        size_t dataSize = 0;

	data = xmlNodeGetContent(encCtx->cipherValueNode);
	if(data == NULL) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
			NULL,
			"xmlNodeGetContent",
			XMLSEC_ERRORS_R_INVALID_NODE_CONTENT,
			"node=%s",
			xmlSecErrorsSafeString(xmlSecNodeGetName(encCtx->cipherValueNode)));
	    return(NULL);
	}	
	dataSize = xmlStrlen(data);

        ret = xmlSecTransformCtxBinaryExecute(&(encCtx->encTransformCtx), data, dataSize);
	if(ret < 0) {
    	    xmlSecError(XMLSEC_ERRORS_HERE,
			NULL,
			"xmlSecTransformCtxBinaryExecute",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			XMLSEC_ERRORS_NO_MESSAGE);
	    if(data != NULL) {
		xmlFree(data);
	    }
	    return(NULL);
	}
	if(data != NULL) {
	    xmlFree(data);
	}
    } else {
        ret = xmlSecTransformCtxExecute(&(encCtx->encTransformCtx), node->doc);
	if(ret < 0) {
    	    xmlSecError(XMLSEC_ERRORS_HERE,
			NULL,
			"xmlSecTransformCtxBinaryExecute",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			XMLSEC_ERRORS_NO_MESSAGE);
	    return(NULL);
	}
    }
    
    encCtx->result = encCtx->encTransformCtx.result;
    xmlSecAssert2(encCtx->result != NULL, NULL);
    
    return(encCtx->result);
}

static int 
xmlSecEncCtxEncDataNodeRead(xmlSecEncCtxPtr encCtx, xmlNodePtr node) {
    xmlNodePtr cur;
    int ret;
    
    xmlSecAssert2(encCtx != NULL, -1);
    xmlSecAssert2(node != NULL, -1);

    switch(encCtx->mode) {
	case xmlEncCtxModeEncryptedData:
	    if(!xmlSecCheckNodeName(node, xmlSecNodeEncryptedData, xmlSecEncNs)) {
    		xmlSecError(XMLSEC_ERRORS_HERE,
			    NULL,
			    xmlSecErrorsSafeString(xmlSecNodeGetName(node)),
			    XMLSEC_ERRORS_R_INVALID_NODE,
			    "expected=\"%s\"",
			    xmlSecErrorsSafeString(xmlSecNodeEncryptedData));
		return(-1);	    
	    }
	    break;
	case xmlEncCtxModeEncryptedKey:
	    if(!xmlSecCheckNodeName(node, xmlSecNodeEncryptedKey, xmlSecEncNs)) {
    		xmlSecError(XMLSEC_ERRORS_HERE,
			    NULL,
			    xmlSecErrorsSafeString(xmlSecNodeGetName(node)),
			    XMLSEC_ERRORS_R_INVALID_NODE,
			    "expected=\"%s\"",
			    xmlSecErrorsSafeString(xmlSecNodeEncryptedKey));
		return(-1);	    
	    }
	    break;
    }
    
    /* first read node data */
    xmlSecAssert2(encCtx->id == NULL, -1);
    xmlSecAssert2(encCtx->type == NULL, -1);
    xmlSecAssert2(encCtx->mimeType == NULL, -1);
    xmlSecAssert2(encCtx->encoding == NULL, -1);
    xmlSecAssert2(encCtx->recipient == NULL, -1);
    xmlSecAssert2(encCtx->carriedKeyName == NULL, -1);
    
    encCtx->id = xmlGetProp(node, xmlSecAttrId);
    encCtx->type = xmlGetProp(node, xmlSecAttrType);
    encCtx->mimeType = xmlGetProp(node, xmlSecAttrMimeType);
    encCtx->encoding = xmlGetProp(node, xmlSecAttrEncoding);    
    if(encCtx->mode == xmlEncCtxModeEncryptedKey) {
	encCtx->recipient = xmlGetProp(node, xmlSecAttrRecipient);    
	/* todo: check recipient? */
    }
    cur = xmlSecGetNextElementNode(node->children);
    
    /* first node is optional EncryptionMethod, we'll read it later */
    xmlSecAssert2(encCtx->encMethodNode == NULL, -1);
    if((cur != NULL) && (xmlSecCheckNodeName(cur, xmlSecNodeEncryptionMethod, xmlSecEncNs))) {
	encCtx->encMethodNode = cur;
        cur = xmlSecGetNextElementNode(cur->next);
    }

    /* next node is optional KeyInfo, we'll process it later */
    xmlSecAssert2(encCtx->keyInfoNode == NULL, -1);
    if((cur != NULL) && (xmlSecCheckNodeName(cur, xmlSecNodeKeyInfo, xmlSecDSigNs))) {
	encCtx->keyInfoNode = cur;
	cur = xmlSecGetNextElementNode(cur->next);
    }    

    /* next is required CipherData node */
    if((cur == NULL) || (!xmlSecCheckNodeName(cur, xmlSecNodeCipherData, xmlSecEncNs))) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    xmlSecErrorsSafeString(xmlSecNodeGetName(cur)),
		    XMLSEC_ERRORS_R_INVALID_NODE,
		    "node=%s",
		    xmlSecErrorsSafeString(xmlSecNodeCipherData));
	return(-1);
    }
    
    ret = xmlSecEncCtxCipherDataNodeRead(encCtx, cur);
    if(ret < 0) {
        xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecEncCtxCipherDataNodeRead",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }
    cur = xmlSecGetNextElementNode(cur->next);

    /* next is optional EncryptionProperties node (we simply ignore it) */
    if((cur != NULL) && (xmlSecCheckNodeName(cur, xmlSecNodeEncryptionProperties, xmlSecEncNs))) {
	cur = xmlSecGetNextElementNode(cur->next);
    }

    /* there are more possible nodes for the <EncryptedKey> node */
    if(encCtx->mode == xmlEncCtxModeEncryptedKey) {
	/* next is optional ReferenceList node (we simply ignore it) */
        if((cur != NULL) && (xmlSecCheckNodeName(cur, xmlSecNodeReferenceList, xmlSecEncNs))) {
	    cur = xmlSecGetNextElementNode(cur->next);
	}

        /* next is optional CarriedKeyName node (we simply ignore it) */
	if((cur != NULL) && (xmlSecCheckNodeName(cur, xmlSecNodeCarriedKeyName, xmlSecEncNs))) {
	    encCtx->carriedKeyName = xmlNodeGetContent(cur);
	    if(encCtx->carriedKeyName == NULL) {
		xmlSecError(XMLSEC_ERRORS_HERE,
			    NULL,
			    xmlSecErrorsSafeString(xmlSecNodeGetName(cur)),
			    XMLSEC_ERRORS_R_INVALID_NODE_CONTENT,
			    "node=%s",
			    xmlSecErrorsSafeString(xmlSecNodeCipherData));
		return(-1);
	    }
	    /* TODO: decode the name? */
	    cur = xmlSecGetNextElementNode(cur->next);
	}
    }

    /* if there is something left than it's an error */
    if(cur != NULL) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    xmlSecErrorsSafeString(xmlSecNodeGetName(cur)),
		    NULL,
		    XMLSEC_ERRORS_R_UNEXPECTED_NODE,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }

    /* now read the encryption method node */
    if((encCtx->encMethod == NULL) && (encCtx->encMethodNode != NULL)) {
	encCtx->encMethod = xmlSecTransformCtxNodeRead(&(encCtx->encTransformCtx), encCtx->encMethodNode,
						xmlSecTransformUsageEncryptionMethod);
	if(encCtx->encMethod == NULL) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
		    	NULL,
			"xmlSecTransformCtxNodeRead",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			"node=%s",
			xmlSecErrorsSafeString(xmlSecNodeGetName(encCtx->encMethodNode)));
	    return(-1);	    
	}	
	encCtx->dontDestroyEncMethod = 1;
    } else if(encCtx->encMethod != NULL) {
	ret = xmlSecTransformCtxAppend(&(encCtx->encTransformCtx), encCtx->encMethod);
	if(ret < 0) {
    	    xmlSecError(XMLSEC_ERRORS_HERE,
			NULL,
			"xmlSecTransformCtxAppend",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			XMLSEC_ERRORS_NO_MESSAGE);
	    return(-1);
	}
    } else {
	/* TODO: add default global enc method? */
	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    NULL,
		    XMLSEC_ERRORS_R_INVALID_DATA,
		    "encryption method not specified");
	return(-1);
    }
    encCtx->encMethod->encode = encCtx->encrypt;
    
    /* we have encryption method, find key */
    ret = xmlSecTransformSetKeyReq(encCtx->encMethod, &(encCtx->keyInfoReadCtx.keyReq));
    if(ret < 0) {
    	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecTransformSetKeyReq",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    "transform=%s",
		    xmlSecErrorsSafeString(xmlSecTransformGetName(encCtx->encMethod)));
	return(-1);
    }	

    /* TODO: KeyInfo node != NULL and encKey != NULL */    	
    if((encCtx->encKey == NULL) && (encCtx->keyInfoReadCtx.keysMngr->getKey != NULL)) {
	encCtx->encKey = (encCtx->keyInfoReadCtx.keysMngr->getKey)(encCtx->keyInfoNode, 
							     &(encCtx->keyInfoReadCtx));
    }
    
    /* check that we have exactly what we want */
    if((encCtx->encKey == NULL) || 
       (!xmlSecKeyMatch(encCtx->encKey, NULL, &(encCtx->keyInfoReadCtx.keyReq)))) {

	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    NULL,
		    XMLSEC_ERRORS_R_KEY_NOT_FOUND,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }
    
    /* set the key to the transform */
    ret = xmlSecTransformSetKey(encCtx->encMethod, encCtx->encKey);
    if(ret < 0) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    "xmlSecTransformSetKey",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    "transform=%s",
		    xmlSecErrorsSafeString(xmlSecTransformGetName(encCtx->encMethod)));
	return(-1);
    }

    /* if we need to write result to xml node then we need base64 encode it */
    if((encCtx->encrypt) && (encCtx->cipherValueNode != NULL)) {	
	xmlSecTransformPtr base64Encode;
	
	/* we need to add base64 encode transform */
	base64Encode = xmlSecTransformCtxCreateAndAppend(&(encCtx->encTransformCtx), xmlSecTransformBase64Id);
    	if(base64Encode == NULL) {
    	    xmlSecError(XMLSEC_ERRORS_HERE,
			NULL,
			"xmlSecTransformCtxCreateAndAppend",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			XMLSEC_ERRORS_NO_MESSAGE);
	    return(-1);
	}
	base64Encode->encode = 1;
	encCtx->resultBase64Encoded = 1;
    }
    
    return(0);
}

static int 
xmlSecEncCtxEncDataNodeWrite(xmlSecEncCtxPtr encCtx) {
    int ret;
    
    xmlSecAssert2(encCtx != NULL, -1);
    xmlSecAssert2(encCtx->result != NULL, -1);
    xmlSecAssert2(encCtx->encKey != NULL, -1);
    
    /* write encrypted data to xml (if requested) */
    if(encCtx->cipherValueNode != NULL) {	
	xmlSecAssert2(xmlSecBufferGetData(encCtx->result) != NULL, -1);

	xmlNodeSetContentLen(encCtx->cipherValueNode,
			    xmlSecBufferGetData(encCtx->result),
			    xmlSecBufferGetSize(encCtx->result));
	encCtx->resultReplaced = 1;
    }

    /* update <dsig:KeyInfo/> node */
    if(encCtx->keyInfoNode != NULL) {
	ret = xmlSecKeyInfoNodeWrite(encCtx->keyInfoNode, encCtx->encKey, &(encCtx->keyInfoWriteCtx));
	if(ret < 0) {
    	    xmlSecError(XMLSEC_ERRORS_HERE,
			NULL,
			"xmlSecKeyInfoNodeWrite",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			XMLSEC_ERRORS_NO_MESSAGE);
	    return(-1);
	}	
    }
    
    return(0);
}

static int 
xmlSecEncCtxCipherDataNodeRead(xmlSecEncCtxPtr encCtx, xmlNodePtr node) {
    xmlNodePtr cur;
    int ret;
    
    xmlSecAssert2(encCtx != NULL, -1);
    xmlSecAssert2(node != NULL, -1);
    
    cur = xmlSecGetNextElementNode(node->children);
    
    /* we either have CipherValue or CipherReference node  */
    xmlSecAssert2(encCtx->cipherValueNode == NULL, -1);
    if((cur != NULL) && (xmlSecCheckNodeName(cur, xmlSecNodeCipherValue, xmlSecEncNs))) {
        /* don't need data from CipherData node when we are encrypting */
	if(encCtx->encrypt == 0) {
	    xmlSecTransformPtr base64Decode;
	
	    /* we need to add base64 decode transform */
	    base64Decode = xmlSecTransformCtxCreateAndPrepend(&(encCtx->encTransformCtx), xmlSecTransformBase64Id);
    	    if(base64Decode == NULL) {
    		xmlSecError(XMLSEC_ERRORS_HERE,
			    NULL,
			    "xmlSecTransformCtxCreateAndPrepend",
			    XMLSEC_ERRORS_R_XMLSEC_FAILED,
			    XMLSEC_ERRORS_NO_MESSAGE);
	        return(-1);
	    }
	}
	encCtx->cipherValueNode = cur;
        cur = xmlSecGetNextElementNode(cur->next);
    } else if((cur != NULL) && (xmlSecCheckNodeName(cur, xmlSecNodeCipherReference, xmlSecEncNs))) {
        /* don't need data from CipherData node when we are encrypting */
	if(encCtx->encrypt == 0) {
    	    ret = xmlSecEncCtxCipherReferenceNodeRead(encCtx, cur);
	    if(ret < 0) {
		xmlSecError(XMLSEC_ERRORS_HERE,
		    	    NULL,
			    "xmlSecEncCtxCipherReferenceNodeRead",
			    XMLSEC_ERRORS_R_XMLSEC_FAILED,
			    "node=%s",
			    xmlSecErrorsSafeString(xmlSecNodeGetName(cur)));
		return(-1);	    
	    }
	}	
        cur = xmlSecGetNextElementNode(cur->next);
    }
    
    if(cur != NULL) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    xmlSecErrorsSafeString(xmlSecNodeGetName(cur)),
		    XMLSEC_ERRORS_R_INVALID_NODE,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }
    return(0);
}

static int 
xmlSecEncCtxCipherReferenceNodeRead(xmlSecEncCtxPtr encCtx, xmlNodePtr node) {
    xmlNodePtr cur;
    xmlChar* uri;
    int ret;
    
    xmlSecAssert2(encCtx != NULL, -1);
    xmlSecAssert2(node != NULL, -1);
    
    /* first read the optional uri attr and check that we can process it */
    uri = xmlGetProp(node, xmlSecAttrURI);
    if(xmlSecUriTypeCheck(encCtx->allowedCipherReferenceUris, uri) != 1) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    NULL,
		    XMLSEC_ERRORS_R_INVALID_URI_TYPE,
		    "uri=\"%s\"", xmlSecErrorsSafeString(uri));
	xmlFree(uri);
	return(-1);	    
    }
    
    if(uri != NULL) {
	ret = xmlSecTransformCtxSetUri(&(encCtx->encTransformCtx), uri, node);
	if(ret < 0) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
		    	NULL,
			"xmlSecTransformCtxSetUri",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			"uri=%s",
			xmlSecErrorsSafeString(uri));
	    xmlFree(uri);
	    return(-1);	    
	}		
	xmlFree(uri);
    }    
    cur = xmlSecGetNextElementNode(node->children);
    
    /* the only one node is optional Transforms node */
    if((cur != NULL) && (xmlSecCheckNodeName(cur, xmlSecNodeTransforms, xmlSecEncNs))) {
	ret = xmlSecTransformCtxNodesListRead(&(encCtx->encTransformCtx), cur,
				    xmlSecTransformUsageDSigTransform);
	if(ret < 0) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
		    	NULL,
			"xmlSecTransformCtxNodesListRead",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			"node=%s",
			xmlSecErrorsSafeString(xmlSecNodeGetName(encCtx->encMethodNode)));
	    return(-1);	    
	}	
        cur = xmlSecGetNextElementNode(cur->next);
    }
    
    /* if there is something left than it's an error */
    if(cur != NULL) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    xmlSecErrorsSafeString(xmlSecNodeGetName(cur)),
		    NULL,
		    XMLSEC_ERRORS_R_UNEXPECTED_NODE,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }
    return(0);
}

void 
xmlSecEncCtxDebugDump(xmlSecEncCtxPtr encCtx, FILE* output) {
    xmlSecAssert(encCtx != NULL);

    switch(encCtx->mode) {
	case xmlEncCtxModeEncryptedData:
	    if(encCtx->encrypt) {    
		fprintf(output, "= DATA ENCRYPTION CONTEXT\n");
	    } else {
		fprintf(output, "= DATA DECRYPTION CONTEXT\n");
	    }
	    break;
	case xmlEncCtxModeEncryptedKey:
	    if(encCtx->encrypt) {    
		fprintf(output, "= KEY ENCRYPTION CONTEXT\n");
	    } else {
		fprintf(output, "= KEY DECRYPTION CONTEXT\n");
	    }
	    break;
    }
    fprintf(output, "== Status: %s\n",
	    (encCtx->resultReplaced) ? "replaced" : "not-replaced" );
    if(encCtx->id != NULL) {
	fprintf(output, "== Id: \"%s\"\n", encCtx->id);
    }
    if(encCtx->type != NULL) {
	fprintf(output, "== Type: \"%s\"\n", encCtx->type);
    }
    if(encCtx->mimeType != NULL) {
	fprintf(output, "== MimeType: \"%s\"\n", encCtx->mimeType);
    }
    if(encCtx->encoding != NULL) {
	fprintf(output, "== Encoding: \"%s\"\n", encCtx->encoding);
    }
    if(encCtx->recipient != NULL) {
	fprintf(output, "== Recipient: \"%s\"\n", encCtx->recipient);
    }
    if(encCtx->carriedKeyName != NULL) {
	fprintf(output, "== CarriedKeyName: \"%s\"\n", encCtx->carriedKeyName);
    }
    
    fprintf(output, "== Key Info Read Ctx:\n");
    xmlSecKeyInfoCtxDebugDump(&(encCtx->keyInfoReadCtx), output);
    fprintf(output, "== Key Info Write Ctx:\n");
    xmlSecKeyInfoCtxDebugDump(&(encCtx->keyInfoWriteCtx), output);

    /* todo: encKey */

    xmlSecTransformCtxDebugDump(&(encCtx->encTransformCtx), output);
    
    if((encCtx->result != NULL) && 
       (xmlSecBufferGetData(encCtx->result) != NULL) && 
       (encCtx->resultBase64Encoded != 0)) {

	fprintf(output, "== Result - start buffer:\n");
	fwrite(xmlSecBufferGetData(encCtx->result), 
	       xmlSecBufferGetSize(encCtx->result), 1,
	       output);
	fprintf(output, "\n== Result - end buffer\n");
    } else {
	fprintf(output, "== Result: %d bytes\n",
		xmlSecBufferGetSize(encCtx->result));
    }
}

void 
xmlSecEncCtxDebugXmlDump(xmlSecEncCtxPtr encCtx, FILE* output) {
    xmlSecAssert(encCtx != NULL);

    switch(encCtx->mode) {
	case xmlEncCtxModeEncryptedData:
	    if(encCtx->encrypt) {    
		fprintf(output, "<DataEncryptionContext ");
	    } else {
		fprintf(output, "<DataDecryptionContext ");
	    }
	    break;
	case xmlEncCtxModeEncryptedKey:
	    if(encCtx->encrypt) {    
		fprintf(output, "<KeyEncryptionContext ");
	    } else {
		fprintf(output, "<KeyDecryptionContext ");
	    }
	    break;
    }
    fprintf(output, "status=\"%s\" >\n", (encCtx->resultReplaced) ? "replaced" : "not-replaced" );

    if(encCtx->id != NULL) {
	fprintf(output, "<Id>%s</Id>\n", encCtx->id);
    }
    if(encCtx->type != NULL) {
	fprintf(output, "<Type>%s</Type>\n", encCtx->type);
    }
    if(encCtx->mimeType != NULL) {
	fprintf(output, "<MimeType>%s</MimeType>\n", encCtx->mimeType);
    }
    if(encCtx->encoding != NULL) {
	fprintf(output, "<Encoding>%s</Encoding>\n", encCtx->encoding);
    }
    if(encCtx->recipient != NULL) {
	fprintf(output, "<Recipient>%s</Recipient>\n", encCtx->recipient);
    }
    if(encCtx->carriedKeyName != NULL) {
	fprintf(output, "<CarriedKeyName>%s</CarriedKeyName>\n", encCtx->carriedKeyName);
    }

    fprintf(output, "<KeyInfoReadCtx>\n");
    xmlSecKeyInfoCtxDebugXmlDump(&(encCtx->keyInfoReadCtx), output);
    fprintf(output, "</KeyInfoReadCtx>\n");

    fprintf(output, "<KeyInfoWriteCtx>\n");
    xmlSecKeyInfoCtxDebugXmlDump(&(encCtx->keyInfoWriteCtx), output);
    fprintf(output, "</KeyInfoWriteCtx>\n");
    xmlSecTransformCtxDebugXmlDump(&(encCtx->encTransformCtx), output);

    /* todo: encKey */

    if((encCtx->result != NULL) && 
       (xmlSecBufferGetData(encCtx->result) != NULL) && 
       (encCtx->resultBase64Encoded != 0)) {

	fprintf(output, "<Result>");
	fwrite(xmlSecBufferGetData(encCtx->result), 
	       xmlSecBufferGetSize(encCtx->result), 1,
	       output);
	fprintf(output, "</Result>\n");
    } else {
	fprintf(output, "<Result size=\"%d\" />\n",
	       xmlSecBufferGetSize(encCtx->result));
    }

    switch(encCtx->mode) {
	case xmlEncCtxModeEncryptedData:
	    if(encCtx->encrypt) {    
		fprintf(output, "</DataEncryptionContext>\n");
	    } else {
		fprintf(output, "</DataDecryptionContext>\n");
	    }
	    break;
	case xmlEncCtxModeEncryptedKey:
	    if(encCtx->encrypt) {    
		fprintf(output, "</KeyEncryptionContext>\n");
	    } else {
		fprintf(output, "</KeyDecryptionContext>\n");
	    }
	    break;
    }
}

#endif /* XMLSEC_NO_XMLENC */


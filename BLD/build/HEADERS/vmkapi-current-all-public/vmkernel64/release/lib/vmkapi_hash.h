/* **********************************************************
 * Copyright 2006 - 2011 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Hash                                                           */ /**
 * \addtogroup Lib
 * @{
 * \defgroup Hash Hash
 *
 * The following are interfaces for a hash abstraction which enables
 * arbitrary key-value pair storage.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_HASH_H_
#define _VMKAPI_HASH_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "base/vmkapi_heap.h"

/**
 * \brief Invalid hash handle
 */
#define VMK_INVALID_HASH_HANDLE NULL

/**
 * \brief Handle to a hash table
 */
typedef void *vmk_HashTable;

/**
 * \brief Key used to store key-value pair. A key can really be anything ranging
 *        from a string to integer to whatever data structure one would like
 *        to use as a key.
 */
typedef void *vmk_HashKey;

/**
 * \brief Hash implementation uses these flags to process key.
 */
typedef vmk_uint64 vmk_HashKeyFlags;

/** No flags. */
#define VMK_HASH_KEY_FLAGS_NONE       0x0

/** 
 * Hash implementation should do a local copy of the key on insertion
 * and do not assume the memory backing up the key will be persistent.
 */
#define VMK_HASH_KEY_FLAGS_LOCAL_COPY 0x1

/**
 * \brief Key length.
 */
typedef vmk_uint32 vmk_HashKeyLen;

/**
 * \brief Value used as part of a key-value pair. There is no restriction
 *        related to the internal value representation.
 */
typedef void *vmk_HashValue;

/**
 * \brief Key iterator commands.
 */
typedef vmk_uint64 vmk_HashKeyIteratorCmd;

/** Keep iterating through the hash table. */
#define VMK_HASH_KEY_ITER_CMD_CONTINUE 0x0

/** Stop iterating through the hash table. */
#define VMK_HASH_KEY_ITER_CMD_STOP     0x1

/** Delete the iterated key-value pair. */
#define VMK_HASH_KEY_ITER_CMD_DELETE   0x2

/*
 *******************************************************************************
 * vmk_HashGetAllocSize --                                                */ /**
 *
 * \brief Return a best estimate amount of memory necessary to operate the
 *        hash table.
 *
 * \param[in]  nbEntries A "best guess" number of expected entries for hash
 *                       buckets sizing.
 *
 * \retval Best estimate amount of memory in bytes.
 *
 *******************************************************************************
 */
vmk_ByteCount
vmk_HashGetAllocSize(vmk_uint32 nbEntries);


/*
 *******************************************************************************
 * vmk_HashAlloc --                                                       */ /**
 *
 * \brief Allocate a new hash table using integer keys.
 *
 * \note vmk_HashRelease() needs to be called once done with the hash table.
 *
 * \note The hash table returned does not come with locking, it is the
 *       caller's responsibility to provide such mechanism.
 * 
 * \param[in]  moduleID  Module ID requesting the hash table.
 * \param[in]  heapID    The heap used for hash table internal allocation
 *                       related to hash structure and hash entries
 *                       structure.
 * \param[in]  nbEntries A "best guess" number of expected entries for hash
 *                       buckets sizing.
 * \param[out] hdl       Hash handle allocated for later operations.
 *
 * \retval VMK_OK        Hash table initialization was successful.
 * \retval VMK_NO_MEMORY Allocation failure.
 * \retval VMK_BAD_PARAM If hdl pointer equals to NULL.
 *                       If moduleID equals to VMK_INVALID_MODULE_ID.
 *                       If heapID equals to VMK_INVALID_HEAP_ID.
 *                       If nbEntries equals to zero.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashAlloc(vmk_ModuleID moduleID,
              vmk_HeapID heapID,
              vmk_uint32 nbEntries,
              vmk_HashTable *hdl);


/*
 *******************************************************************************
 * vmk_HashAllocWithStrKeys --                                            */ /**
 *
 * \brief Allocate a new hash table using string keys.
 *
 * \note vmk_HashRelease() needs to be called once done with the hash table.
 *
 * \note The hash table returned does not come with locking, it is the
 *       caller's responsibility to provide such mechanism.
 *
 * \param[in]  moduleID  Module ID requesting the hash table.
 * \param[in]  heapID    The heap used for hash table internal allocation
 *                       related to hash structure and hash entries
 *                       structure.
 * \param[in]  maxStrLen Maximum string size expected.
 * \param[in]  flags     Hash key flags.
 * \param[in]  nbEntries A "best guess" number of expected entries for hash
 *                       buckets sizing.
 * \param[out] hdl       Hash handle allocated for later operations.
 *
 * \retval VMK_OK        Hash table initialization was successful.
 * \retval VMK_NO_MEMORY Allocation failure.
 * \retval VMK_BAD_PARAM If hdl pointer equals to NULL.
 *                       If moduleID equals to VMK_INVALID_MODULE_ID.
 *                       If heapID equals to VMK_INVALID_HEAP_ID.
 *                       If nbEntries equals to zero.
 *                       If flags is invalid.
 *                       If maxStrLen equals to 0.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashAllocWithStrKeys(vmk_ModuleID moduleID,
                         vmk_HeapID heapID,
                         vmk_HashKeyLen maxStrLen,
                         vmk_HashKeyFlags flags,
                         vmk_uint32 nbEntries,
                         vmk_HashTable *hdl);


/*
 *******************************************************************************
 * vmk_HashAllocWithOpaqueKeys --                                         */ /**
 *
 * \brief Allocate a new hash table using opaque keys.
 *
 * \note The hash table returned does not come with locking, it is the
 *       caller's responsibility to provide such mechanism.
 *
 * \note vmk_HashRelease() needs to be called once done with the hash table.
 *
 * \param[in]  moduleID  Module ID requesting the hash table.
 * \param[in]  heapID    The heap used for hash table internal allocation
 *                       related to hash structure and hash entries
 *                       structure.
 * \param[in]  keyLen    Constant key size expected.
 * \param[in]  flags     Hash key flags.
 * \param[in]  nbEntries A "best guess" number of expected entries for hash
 *                       buckets sizing.
 * \param[out] hdl       Hash handle allocated for later operations.
 *
 * \retval VMK_OK        Hash table initialization was successful.
 * \retval VMK_NO_MEMORY Allocation failure.
 * \retval VMK_BAD_PARAM If hdl pointer equals to NULL.
 *                       If moduleID equals to VMK_INVALID_MODULE_ID.
 *                       If heapID equals to VMK_INVALID_HEAP_ID.
 *                       If nbEntries equals to zero.
 *                       If flags is invalid.
 *                       If keyLen equals to 0.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashAllocWithOpaqueKeys(vmk_ModuleID moduleID,
                            vmk_HeapID heapID,
                            vmk_HashKeyLen keyLen,
                            vmk_HashKeyFlags flags,
                            vmk_uint32 nbEntries,
                            vmk_HashTable *hdl);


/*
 *******************************************************************************
 * vmk_HashRelease --                                                     */ /**
 *
 * \brief Release a hash table.
 *
 * \param[in] hdl       Hash handle.
 *
 * \retval VMK_OK        Hash table was released successful.
 * \retval VMK_BUSY      If the hash table is not empty.
 * \retval VMK_BAD_PARAM If hdl equals to VMK_INVALID_HASH_HANDLE.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashRelease(vmk_HashTable hdl);


/*
 *******************************************************************************
 * vmk_HashIsEmpty --                                                     */ /**
 *
 * \brief Check if a given hash table is empty.
 *
 * \param[in] hdl       Hash handle.
 *
 * \retval VMK_TRUE     Hash is empty.
 * \retval VMK_FALSE    Hash has at least one key-value pair.
 *
 *******************************************************************************
 */
vmk_Bool
vmk_HashIsEmpty(vmk_HashTable hdl);


/*
 *******************************************************************************
 * vmk_HashKeyIterator --                                                 */ /**
 *
 * \brief Iterator used to iterate the key-value pairs on a given hash table.
 *
 * \note The return value is a command set given back to the iterator engine to
 *       let it know what to do next. It can be a binary union of any of the 
 *       vmk_HashKeyIteratorCmd defined above.
 *
 * \param[in] hdl       Hash handle.
 * \param[in] key       Key.
 * \param[in] value     Value.
 * \param[in] data      Private data passed while calling vmk_HashKeyIterate.
 *
 * \retval VMK_HASH_KEY_ITER_CMD_CONTINUE Move to the next key-value pair.
 * \retval VMK_HASH_KEY_ITER_CMD_STOP     Stop iterating.
 * \retval VMK_HASH_KEY_ITER_CMD_DELETE   Delete the current key-value pair.
 *
 *******************************************************************************
 */
typedef vmk_HashKeyIteratorCmd (*vmk_HashKeyIterator)(vmk_HashTable hdl,
                                                      vmk_HashKey key,
                                                      vmk_HashValue value,
                                                      vmk_AddrCookie data);


/*
 *******************************************************************************
 * vmk_HashKeyIterate --                                                  */ /**
 *
 * \brief Iterate through the key-value pairs from a given hash table.
 *
 * \param[in] hdl       Hash handle.
 * \param[in] iterator  Iterator callback.
 * \param[in] data      Private data passed to the iterator for each key-value
 *                      pair.
 *
 * \retval VMK_OK        Iterator went through the entire hash table or until
 *                       stop command was issued.
 * \retval VMK_BAD_PARAM If hdl equals to VMK_INVALID_HASH_HANDLE.
 *                       If iterator equals to NULL.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashKeyIterate(vmk_HashTable hdl,
                   vmk_HashKeyIterator iterator,
                   vmk_AddrCookie data);


/*
 *******************************************************************************
 * vmk_HashKeyInsert --                                                   */ /**
 *
 * \brief Insert a key-value pair into a given hash table.
 *
 * \note The key passed will be copied locally only if the flag 
 *       VMK_HASH_KEY_FLAGS_LOCAL_COPY was passed while creating the hash.
 *
 * \note The value passed won't be copied so the reference needs to be persistent
 *       till key-value pair removal.
 *
 * \param[in] hdl        Hash handle.
 * \param[in] key        Key.
 * \param[in] value      Value. A NULL value is valid.
 *
 * \retval VMK_OK        Key-value pair insertion successful.
 * \retval VMK_NO_MEMORY Allocation failure.
 * \retval VMK_BAD_PARAM If hdl pointer equals to VMK_INVALID_HASH_HANDLE.
 *                       If key equals to NULL on a string or opaque keys hash.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashKeyInsert(vmk_HashTable hdl,               
                  vmk_HashKey key,
                  vmk_HashValue value);


/*
 *******************************************************************************
 * vmk_HashKeyUpdate --                                                   */ /**
 *
 * \brief Update a key-value pair on a given hash table and return the previous
 *        associated value.
 *
 * \note The value passed won't be copied so the reference needs to be persistent
 *       till key-value pair removal.
 *
 * \param[in]  hdl       Hash handle.
 * \param[in]  key       Key.
 * \param[in]  newValue  Updated value. A NULL value is valid.
 * \param[out] oldValue  Value before update. A NULL value would mean that the
 *                       caller is not interested in getting the previous
 *                       associated value.
 *
 * \retval VMK_OK        Key-value pair update successful.
 * \retval VMK_NOT_FOUND If there is no key-value pair matching the key
 *                       parameter.
 * \retval VMK_BAD_PARAM If hdl pointer equals to VMK_INVALID_HASH_HANDLE.
 *                       If key equals to NULL on a string or opaque keys hash.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashKeyUpdate(vmk_HashTable hdl,
                  vmk_HashKey key,
                  vmk_HashValue newValue,
                  vmk_HashValue *oldValue);


/*
 *******************************************************************************
 * vmk_HashKeyDelete --                                                   */ /**
 *
 * \brief Delete a key-value pair from a given hash table and return the current
 *        value.
 *
 * \param[in]  hdl       Hash handle.
 * \param[in]  key       Key.
 * \param[out] value     Value before remove. A NULL value would mean that the
 *                       caller is not interested in getting the current
 *                       associated value.
 *
 * \retval VMK_OK        Key-value pair removal successful.
 * \retval VMK_NOT_FOUND If there is no key-value pair matching the key
 *                       parameter.
 * \retval VMK_BAD_PARAM If hdl pointer equals to VMK_INVALID_HASH_HANDLE.
 *                       If key equals to NULL on a string or opaque keys hash.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashKeyDelete(vmk_HashTable hdl,
                  vmk_HashKey key,
                  vmk_HashValue *value);


/*
 *******************************************************************************
 * vmk_HashKeyFind --                                                     */ /**
 *
 * \brief Find a key-value pair and return the current associated value.
 *
 * \param[in]  hdl       Hash handle.
 * \param[in]  key       Key.
 * \param[out] value     Value associated to the key. A NULL value would mean
 *                       that the caller is not interested in getting the current
 *                       associated value.
 *
 * \retval VMK_OK        Key-value pair found successfully.
 * \retval VMK_NOT_FOUND If there is no key-value pair matching the key
 *                       parameter.
 * \retval VMK_BAD_PARAM If hdl pointer equals to VMK_INVALID_HASH_HANDLE.
 *                       If key equals to NULL on a string or opaque keys hash.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashKeyFind(vmk_HashTable hdl,
                vmk_HashKey key,
                vmk_HashValue *value);


#endif /* _VMKAPI_HASH_H_ */
/** @} */
/** @} */

// Package cachestorage provides the Chrome DevTools Protocol
// commands, types, and events for the CacheStorage domain.
//
// Generated by the cdproto-gen command.
package cachestorage

// Code generated by cdproto-gen. DO NOT EDIT.

import (
	"context"

	"github.com/chromedp/cdproto/cdp"
	"github.com/chromedp/cdproto/storage"
)

// DeleteCacheParams deletes a cache.
type DeleteCacheParams struct {
	CacheID CacheID `json:"cacheId"` // Id of cache for deletion.
}

// DeleteCache deletes a cache.
//
// See: https://chromedevtools.github.io/devtools-protocol/tot/CacheStorage#method-deleteCache
//
// parameters:
//
//	cacheID - Id of cache for deletion.
func DeleteCache(cacheID CacheID) *DeleteCacheParams {
	return &DeleteCacheParams{
		CacheID: cacheID,
	}
}

// Do executes CacheStorage.deleteCache against the provided context.
func (p *DeleteCacheParams) Do(ctx context.Context) (err error) {
	return cdp.Execute(ctx, CommandDeleteCache, p, nil)
}

// DeleteEntryParams deletes a cache entry.
type DeleteEntryParams struct {
	CacheID CacheID `json:"cacheId"` // Id of cache where the entry will be deleted.
	Request string  `json:"request"` // URL spec of the request.
}

// DeleteEntry deletes a cache entry.
//
// See: https://chromedevtools.github.io/devtools-protocol/tot/CacheStorage#method-deleteEntry
//
// parameters:
//
//	cacheID - Id of cache where the entry will be deleted.
//	request - URL spec of the request.
func DeleteEntry(cacheID CacheID, request string) *DeleteEntryParams {
	return &DeleteEntryParams{
		CacheID: cacheID,
		Request: request,
	}
}

// Do executes CacheStorage.deleteEntry against the provided context.
func (p *DeleteEntryParams) Do(ctx context.Context) (err error) {
	return cdp.Execute(ctx, CommandDeleteEntry, p, nil)
}

// RequestCacheNamesParams requests cache names.
type RequestCacheNamesParams struct {
	SecurityOrigin string          `json:"securityOrigin,omitempty"` // At least and at most one of securityOrigin, storageKey, storageBucket must be specified. Security origin.
	StorageKey     string          `json:"storageKey,omitempty"`     // Storage key.
	StorageBucket  *storage.Bucket `json:"storageBucket,omitempty"`  // Storage bucket. If not specified, it uses the default bucket.
}

// RequestCacheNames requests cache names.
//
// See: https://chromedevtools.github.io/devtools-protocol/tot/CacheStorage#method-requestCacheNames
//
// parameters:
func RequestCacheNames() *RequestCacheNamesParams {
	return &RequestCacheNamesParams{}
}

// WithSecurityOrigin at least and at most one of securityOrigin, storageKey,
// storageBucket must be specified. Security origin.
func (p RequestCacheNamesParams) WithSecurityOrigin(securityOrigin string) *RequestCacheNamesParams {
	p.SecurityOrigin = securityOrigin
	return &p
}

// WithStorageKey storage key.
func (p RequestCacheNamesParams) WithStorageKey(storageKey string) *RequestCacheNamesParams {
	p.StorageKey = storageKey
	return &p
}

// WithStorageBucket storage bucket. If not specified, it uses the default
// bucket.
func (p RequestCacheNamesParams) WithStorageBucket(storageBucket *storage.Bucket) *RequestCacheNamesParams {
	p.StorageBucket = storageBucket
	return &p
}

// RequestCacheNamesReturns return values.
type RequestCacheNamesReturns struct {
	Caches []*Cache `json:"caches,omitempty"` // Caches for the security origin.
}

// Do executes CacheStorage.requestCacheNames against the provided context.
//
// returns:
//
//	caches - Caches for the security origin.
func (p *RequestCacheNamesParams) Do(ctx context.Context) (caches []*Cache, err error) {
	// execute
	var res RequestCacheNamesReturns
	err = cdp.Execute(ctx, CommandRequestCacheNames, p, &res)
	if err != nil {
		return nil, err
	}

	return res.Caches, nil
}

// RequestCachedResponseParams fetches cache entry.
type RequestCachedResponseParams struct {
	CacheID        CacheID   `json:"cacheId"`        // Id of cache that contains the entry.
	RequestURL     string    `json:"requestURL"`     // URL spec of the request.
	RequestHeaders []*Header `json:"requestHeaders"` // headers of the request.
}

// RequestCachedResponse fetches cache entry.
//
// See: https://chromedevtools.github.io/devtools-protocol/tot/CacheStorage#method-requestCachedResponse
//
// parameters:
//
//	cacheID - Id of cache that contains the entry.
//	requestURL - URL spec of the request.
//	requestHeaders - headers of the request.
func RequestCachedResponse(cacheID CacheID, requestURL string, requestHeaders []*Header) *RequestCachedResponseParams {
	return &RequestCachedResponseParams{
		CacheID:        cacheID,
		RequestURL:     requestURL,
		RequestHeaders: requestHeaders,
	}
}

// RequestCachedResponseReturns return values.
type RequestCachedResponseReturns struct {
	Response *CachedResponse `json:"response,omitempty"` // Response read from the cache.
}

// Do executes CacheStorage.requestCachedResponse against the provided context.
//
// returns:
//
//	response - Response read from the cache.
func (p *RequestCachedResponseParams) Do(ctx context.Context) (response *CachedResponse, err error) {
	// execute
	var res RequestCachedResponseReturns
	err = cdp.Execute(ctx, CommandRequestCachedResponse, p, &res)
	if err != nil {
		return nil, err
	}

	return res.Response, nil
}

// RequestEntriesParams requests data from cache.
type RequestEntriesParams struct {
	CacheID    CacheID `json:"cacheId"`              // ID of cache to get entries from.
	SkipCount  int64   `json:"skipCount,omitempty"`  // Number of records to skip.
	PageSize   int64   `json:"pageSize,omitempty"`   // Number of records to fetch.
	PathFilter string  `json:"pathFilter,omitempty"` // If present, only return the entries containing this substring in the path
}

// RequestEntries requests data from cache.
//
// See: https://chromedevtools.github.io/devtools-protocol/tot/CacheStorage#method-requestEntries
//
// parameters:
//
//	cacheID - ID of cache to get entries from.
func RequestEntries(cacheID CacheID) *RequestEntriesParams {
	return &RequestEntriesParams{
		CacheID: cacheID,
	}
}

// WithSkipCount number of records to skip.
func (p RequestEntriesParams) WithSkipCount(skipCount int64) *RequestEntriesParams {
	p.SkipCount = skipCount
	return &p
}

// WithPageSize number of records to fetch.
func (p RequestEntriesParams) WithPageSize(pageSize int64) *RequestEntriesParams {
	p.PageSize = pageSize
	return &p
}

// WithPathFilter if present, only return the entries containing this
// substring in the path.
func (p RequestEntriesParams) WithPathFilter(pathFilter string) *RequestEntriesParams {
	p.PathFilter = pathFilter
	return &p
}

// RequestEntriesReturns return values.
type RequestEntriesReturns struct {
	CacheDataEntries []*DataEntry `json:"cacheDataEntries,omitempty"` // Array of object store data entries.
	ReturnCount      float64      `json:"returnCount,omitempty"`      // Count of returned entries from this storage. If pathFilter is empty, it is the count of all entries from this storage.
}

// Do executes CacheStorage.requestEntries against the provided context.
//
// returns:
//
//	cacheDataEntries - Array of object store data entries.
//	returnCount - Count of returned entries from this storage. If pathFilter is empty, it is the count of all entries from this storage.
func (p *RequestEntriesParams) Do(ctx context.Context) (cacheDataEntries []*DataEntry, returnCount float64, err error) {
	// execute
	var res RequestEntriesReturns
	err = cdp.Execute(ctx, CommandRequestEntries, p, &res)
	if err != nil {
		return nil, 0, err
	}

	return res.CacheDataEntries, res.ReturnCount, nil
}

// Command names.
const (
	CommandDeleteCache           = "CacheStorage.deleteCache"
	CommandDeleteEntry           = "CacheStorage.deleteEntry"
	CommandRequestCacheNames     = "CacheStorage.requestCacheNames"
	CommandRequestCachedResponse = "CacheStorage.requestCachedResponse"
	CommandRequestEntries        = "CacheStorage.requestEntries"
)

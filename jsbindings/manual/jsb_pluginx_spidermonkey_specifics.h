#ifndef __JSB_PLUGINX_PLUGINX_SPIDERMONKEY_SPECIFICS_H__
#define __JSB_PLUGINX_PLUGINX_SPIDERMONKEY_SPECIFICS_H__

#include <typeinfo>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "jsapi.h"
#include "uthash.h"

#ifdef ANDROID
#include <android/log.h>
#endif

namespace pluginx {

#ifdef ANDROID
#define  LOG_TAG    "jsb_pluginx"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
#else
#define  LOGD(...) printf(__VA_ARGS__)
#endif

#define JSB_PLUGINX_PRECONDITION( condition, ...) do {							\
	if( ! (condition) ) {														\
        LOGD("jsb: ERROR: File %s: Line: %d, Function: %s", __FILE__, __LINE__, __FUNCTION__ );			\
        LOGD(__VA_ARGS__);                                        \
		JSContext* globalContext = ScriptingCore::getInstance()->getGlobalContext();	\
		if( ! JS_IsExceptionPending( globalContext ) ) {						\
			JS_ReportError( globalContext, __VA_ARGS__ );							\
		}																		\
		return JS_FALSE;														\
	}																			\
} while(0)

#define JSB_PLUGINX_PRECONDITION2( condition, context, ret_value, ...) do {             \
    if( ! (condition) ) {														\
        LOGD("jsb: ERROR: File %s: Line: %d, Function: %s", __FILE__, __LINE__, __FUNCTION__ );			\
        LOGD(__VA_ARGS__);                                        \
        if( ! JS_IsExceptionPending( context ) ) {							\
            JS_ReportError( context, __VA_ARGS__ );								\
        }																		\
        return ret_value;														\
    }                                                                           \
} while(0)


typedef struct js_proxy {
	void *ptr;
	JSObject *obj;
	UT_hash_handle hh;
} js_proxy_t;

extern js_proxy_t *_native_js_global_ht;
extern js_proxy_t *_js_native_global_ht;

typedef struct js_type_class {
	uint32_t type;
	JSClass *jsclass;
	JSObject *proto;
	JSObject *parentProto;
	UT_hash_handle hh;
} js_type_class_t;

extern js_type_class_t *_js_global_type_ht;

static unsigned int getHashCodeByString(const char *key)
{
	unsigned int len = strlen(key);
	const char *end=key+len;
	unsigned int hash;

	for (hash = 0; key < end; key++)
	{
		hash *= 16777619;
		hash ^= (unsigned int) (unsigned char) toupper(*key);
	}
	return (hash);
}

template< typename DERIVED >
class TypeTest
{
public:

	static int s_id()
	{
		// return id unique for DERIVED
		// NOT SURE IT WILL BE REALLY UNIQUE FOR EACH CLASS!!
		/* Commented by James Chen
		Using 'getHashCodeByString(typeid(*native_obj).name())' instead of 'reinterpret_cast<long>(typeid(*native_obj).name());'.
		Since on win32 platform, 'reinterpret_cast<long>(typeid(*native_obj).name());' invoking in cocos2d.dll and outside cocos2d.dll(in TestJavascript.exe) will return different address.
		But the return string from typeid(*native_obj).name() is the same string, so we must convert the string to hash id to make sure we can get unique id.
		*/
		// static const long id = reinterpret_cast<long>(typeid( DERIVED ).name());
        static const long id = getHashCodeByString(typeid( DERIVED ).name());
		return id;
	}

	static const char* s_name()
	{
		// return id unique for DERIVED
		// ALWAYS VALID BUT STRING, NOT INT - BUT VALID AND CROSS-PLATFORM/CROSS-VERSION COMPATBLE
		// AS FAR AS YOU KEEP THE CLASS NAME
		return typeid( DERIVED ).name();
	}
};


#define JSB_PLUGINX_NEW_PROXY(p, native_obj, js_obj) \
do { \
	p = (js_proxy_t *)malloc(sizeof(js_proxy_t)); \
	assert(p); \
    js_proxy_t* native_obj##js_obj##tmp = NULL; \
    HASH_FIND_PTR(_native_js_global_ht, &native_obj, native_obj##js_obj##tmp); \
    assert(!native_obj##js_obj##tmp); \
	p->ptr = native_obj; \
	p->obj = js_obj; \
	HASH_ADD_PTR(_native_js_global_ht, ptr, p); \
	p = (js_proxy_t *)malloc(sizeof(js_proxy_t)); \
	assert(p); \
    native_obj##js_obj##tmp = NULL; \
    HASH_FIND_PTR(_js_native_global_ht, &js_obj, native_obj##js_obj##tmp); \
    assert(!native_obj##js_obj##tmp); \
	p->ptr = native_obj; \
	p->obj = js_obj; \
	HASH_ADD_PTR(_js_native_global_ht, obj, p); \
} while(0) \

#define JSB_PLUGINX_GET_PROXY(p, native_obj) \
do { \
	HASH_FIND_PTR(_native_js_global_ht, &native_obj, p); \
} while (0)

#define JSB_PLUGINX_GET_NATIVE_PROXY(p, js_obj) \
do { \
	HASH_FIND_PTR(_js_native_global_ht, &js_obj, p); \
} while (0)

#define JSB_PLUGINX_REMOVE_PROXY(nproxy, jsproxy) \
do { \
	if (nproxy) { HASH_DEL(_native_js_global_ht, nproxy); free(nproxy); } \
	if (jsproxy) { HASH_DEL(_js_native_global_ht, jsproxy); free(jsproxy); } \
} while (0)

#define JSB_PLUGINX_TEST_NATIVE_OBJECT(cx, native_obj) \
if (!native_obj) { \
	JS_ReportError(cx, "Invalid Native Object"); \
	return JS_FALSE; \
}

/**
 * You don't need to manage the returned pointer. They live for the whole life of
 * the app.
 */
template <class T>
inline js_type_class_t *js_get_type_from_native(T* native_obj) {
    js_type_class_t *typeProxy;
    long typeId = getHashCodeByString(typeid(*native_obj).name());
    HASH_FIND_INT(_js_global_type_ht, &typeId, typeProxy);
    if (!typeProxy) {
        typeId = getHashCodeByString(typeid(T).name());
        HASH_FIND_INT(_js_global_type_ht, &typeId, typeProxy);
    }
    return typeProxy;
}

/**
 * The returned pointer should be deleted using JS_REMOVE_PROXY. Most of the
 * time you do that in the C++ destructor.
 */
template<class T>
inline js_proxy_t *js_get_or_create_proxy(JSContext *cx, T *native_obj) {
    js_proxy_t *proxy;
    HASH_FIND_PTR(_native_js_global_ht, &native_obj, proxy);
    if (!proxy) {
        js_type_class_t *typeProxy = js_get_type_from_native<T>(native_obj);
        assert(typeProxy);
        JSObject* js_obj = JS_NewObject(cx, typeProxy->jsclass, typeProxy->proto, typeProxy->parentProto);
        JSB_PLUGINX_NEW_PROXY(proxy, native_obj, js_obj);
#ifdef DEBUG
        JS_AddNamedObjectRoot(cx, &proxy->obj, typeid(*native_obj).name());
#else
        JS_AddObjectRoot(cx, &proxy->obj);
#endif
        return proxy;
    } else {
        return proxy;
    }
    return NULL;
}


} // namespace pluginx {

#endif /* __JSB_PLUGINX_PLUGINX_SPIDERMONKEY_SPECIFICS_H__ */


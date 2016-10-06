/*****************************************************************************
 * runtime.cpp: support for NPRuntime API for Netscape Script-able plugins
 *              FYI: http://www.mozilla.org/projects/plugins/npruntime.html
 *****************************************************************************
 * Copyright (C) 2002-2012  VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Damien Fouilleul <damien.fouilleul@laposte.net>
 *          JP Dinger <jpd@videolan.org>
 *          Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef __NPORUNTIME_H__
#define __NPORUNTIME_H__

/*
** support framework for runtime script objects
*/

//on windows, to avoid including <npapi.h>
//from Microsoft SDK (rather then from Mozilla SDK),
//#include it indirectly via <npfunctions.h>
#include <npfunctions.h>
#include <npruntime.h>
#include <stdlib.h>

#include <memory>

#include "utils.hpp"

class RuntimeNPObject : public NPObject
{
public:
    // Lazy child object cration helper. Doing this avoids
    // ownership problems with firefox.
    template<class T> void InstantObj( NPObject *&obj );

    inline int negativeToZero( int i )
    {
        return i < 0 ? 0 : i;
    }

    bool isValid()
    {
        return _instance != NULL;
    }

    enum InvokeResult
    {
        INVOKERESULT_NO_ERROR       = 0,    /* returns no error */
        INVOKERESULT_GENERIC_ERROR  = 1,    /* returns error */
        INVOKERESULT_NO_SUCH_METHOD = 2,    /* throws method does not exist */
        INVOKERESULT_INVALID_ARGS   = 3,    /* throws invalid arguments */
        INVOKERESULT_INVALID_VALUE  = 4,    /* throws invalid value in assignment */
        INVOKERESULT_OUT_OF_MEMORY  = 5,    /* throws out of memory */
    };

    virtual InvokeResult getProperty(int index, npapi::OutVariant &result);
    virtual InvokeResult setProperty(int index, const NPVariant &value);
    virtual InvokeResult removeProperty(int index);
    virtual InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, npapi::OutVariant &result);
    virtual InvokeResult invokeDefault(const NPVariant *args, uint32_t argCount, npapi::OutVariant &result);

    bool returnInvokeResult(InvokeResult result);

protected:
    void *operator new(size_t n)
    {
        /*
        ** Assume that browser has a smarter memory allocator
        ** than plain old malloc() and use it instead.
        */
        return NPN_MemAlloc(n);
    };

    void operator delete(void *p)
    {
        NPN_MemFree(p);
    };

    RuntimeNPObject(NPP instance, const NPClass *aClass) :
        _instance(instance)
    {
        _class = const_cast<NPClass *>(aClass);
        referenceCount = 1;
    };
    virtual ~RuntimeNPObject() = default;

    bool isPluginRunning()
    {
        return (_instance->pdata != NULL);
    }
    template<class T> T *getPrivate()
    {
        return reinterpret_cast<T *>(_instance->pdata);
    }

    NPP _instance;

    template <typename>
    friend class RuntimeNPClass;
};

template<class T>
class RuntimeNPClass : public NPClass
{
public:
    static NPClass *getClass()
    {
        static NPClass *singleton = new RuntimeNPClass<T>;
        return singleton;
    }

protected:
    RuntimeNPClass();
    virtual ~RuntimeNPClass();

    static NPObject *Allocate(NPP instance, NPClass *aClass)
    {
        return new T(instance, aClass);
    }

    static void Deallocate(NPObject *npobj)
    {
        RuntimeNPObject *vObj = static_cast<RuntimeNPObject *>(npobj);
        delete vObj;
    }

    static void Invalidate(NPObject *npobj)
    {
        RuntimeNPObject *vObj = static_cast<RuntimeNPObject *>(npobj);
        vObj->_instance = NULL;
    }

    static bool HasMethod(NPObject *npobj, NPIdentifier name)
    {
        const RuntimeNPClass* vClass = static_cast<RuntimeNPClass*>(npobj->_class);
        return vClass->indexOfMethod(name) != -1;
    }

    static bool HasProperty(NPObject *npobj, NPIdentifier name)
    {
        const RuntimeNPClass* vClass = static_cast<RuntimeNPClass*>(npobj->_class);
        return vClass->indexOfProperty(name) != -1;
    }

    static bool GetProperty(NPObject *npobj, NPIdentifier name, NPVariant *result)
    {
        RuntimeNPObject *vObj = static_cast<RuntimeNPObject *>(npobj);
        if( vObj->isValid() )
        {
            const RuntimeNPClass* vClass = static_cast<RuntimeNPClass*>(npobj->_class);
            int index = vClass->indexOfProperty(name);
            if( index != -1 )
            {
                npapi::OutVariant res{ result };
                return vObj->returnInvokeResult(vObj->getProperty(index, res));
            }
        }
        return false;
    }

    static bool SetProperty(NPObject *npobj, NPIdentifier name, const NPVariant *value)
    {
        RuntimeNPObject *vObj = static_cast<RuntimeNPObject *>(npobj);
        if( vObj->isValid() )
        {
            const RuntimeNPClass* vClass = static_cast<RuntimeNPClass*>(npobj->_class);
            int index = vClass->indexOfProperty(name);
            if( index != -1 )
            {
                return vObj->returnInvokeResult(vObj->setProperty(index, *value));
            }
        }
        return false;
    }

    static bool RemoveProperty(NPObject *npobj, NPIdentifier name)
    {
        RuntimeNPObject *vObj = static_cast<RuntimeNPObject *>(npobj);
        if( vObj->isValid() )
        {
            const RuntimeNPClass* vClass = static_cast<RuntimeNPClass*>(npobj->_class);
            int index = vClass->indexOfProperty(name);
            if( index != -1 )
            {
                return vObj->returnInvokeResult(vObj->removeProperty(index));
            }
        }
        return false;
    }

    static bool ClassInvoke(NPObject *npobj, NPIdentifier name,
                                        const NPVariant *args, uint32_t argCount,
                                        NPVariant *result)
    {
        RuntimeNPObject *vObj = static_cast<RuntimeNPObject *>(npobj);
        if( vObj->isValid() )
        {
            const RuntimeNPClass* vClass = static_cast<RuntimeNPClass*>(npobj->_class);
            int index = vClass->indexOfMethod(name);
            if( index != -1 )
            {
                npapi::OutVariant res{ result };
                return vObj->returnInvokeResult(vObj->invoke(index, args, argCount, res));
            }
        }
        return false;
    }

    static bool InvokeDefault(NPObject *npobj,
                                               const NPVariant *args,
                                               uint32_t argCount,
                                               NPVariant *result)
    {
        RuntimeNPObject *vObj = static_cast<RuntimeNPObject *>(npobj);
        if( vObj->isValid() )
        {
            npapi::OutVariant res{ result };
            return vObj->returnInvokeResult(vObj->invokeDefault(args, argCount, res ));
        }
        return false;
    }

    int indexOfMethod(NPIdentifier name) const;
    int indexOfProperty(NPIdentifier name) const;

private:
    std::unique_ptr<NPIdentifier[]> propertyIdentifiers;
    std::unique_ptr<NPIdentifier[]> methodIdentifiers;
};

template<class T>
inline void RuntimeNPObject::InstantObj( NPObject *&obj )
{
    if( !obj )
        obj = NPN_CreateObject(_instance, RuntimeNPClass<T>::getClass());
}

template<class T>
RuntimeNPClass<T>::RuntimeNPClass()
{
    // retreive property identifiers from names
    if( T::propertyCount > 0 )
    {
        propertyIdentifiers.reset( new NPIdentifier[T::propertyCount] );
        NPN_GetStringIdentifiers(const_cast<const NPUTF8**>(T::propertyNames),
            T::propertyCount, propertyIdentifiers.get() );
    }

    // retreive method identifiers from names
    if( T::methodCount > 0 )
    {
        methodIdentifiers.reset( new NPIdentifier[T::methodCount] );
        NPN_GetStringIdentifiers(const_cast<const NPUTF8**>(T::methodNames),
            T::methodCount, methodIdentifiers.get() );
    }

    // fill in NPClass structure
    structVersion  = NP_CLASS_STRUCT_VERSION;
    allocate       = &Allocate;
    deallocate     = &Deallocate;
    invalidate     = &Invalidate;
    hasMethod      = &HasMethod;
    invoke         = &ClassInvoke;
    invokeDefault  = &InvokeDefault;
    hasProperty    = &HasProperty;
    getProperty    = &GetProperty;
    setProperty    = &SetProperty;
    removeProperty = &RemoveProperty;
    enumerate      = 0;
    construct      = 0;
}

template<class T>
RuntimeNPClass<T>::~RuntimeNPClass()
{
}

template<class T>
int RuntimeNPClass<T>::indexOfMethod(NPIdentifier name) const
{
    if( methodIdentifiers )
    {
        for(int c = 0; c< T::methodCount; ++c )
        {
            if( name == methodIdentifiers[c] )
                return c;
        }
    }
    return -1;
}

template<class T>
int RuntimeNPClass<T>::indexOfProperty(NPIdentifier name) const
{
    if( propertyIdentifiers )
    {
        for(int c = 0; c< T::propertyCount; ++c )
        {
            if( name == propertyIdentifiers[c] )
                return c;
        }
    }
    return -1;
}

#endif
